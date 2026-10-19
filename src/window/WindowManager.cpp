#include "WindowManager.h"

#include "../config/AppConfig.h"
#include "../core/Logger.h"
#include "KillPlanner.h"
#include "MonitorHelper.h"
#include "../platform/PlatformBackend.h"

#include <QDir>
#include <QFileInfo>
#include <QThread>

#include <algorithm>
#include <string>
#include <unordered_set>

namespace {

// Processes whose windows are never managed
const QStringList& ignoreListInternal()
{
    static const QStringList list{
        "explorer.exe", "searchui.exe", "shellexperiencehost.exe",
        "searchhost.exe", "startmenuexperiencehost.exe", "applicationframehost.exe",
        "taskmgr.exe", "cmd.exe", "conhost.exe", "lockapp.exe",
        "autohotkey64.exe", "autohotkey32.exe", "nexus.exe", "teknoparrotui.exe",
        "idea64.exe", "code.exe", "chrome.exe", "msedge.exe", "firefox.exe", "steam.exe"
    };
    return list;
}

// Arcade/emulator loaders — pass-through processes whose children are the real target
const QStringList& teknoLoaderListInternal()
{
    static const QStringList list{
        "budgieloader.exe", "elfldr2.exe", "sdaemon.exe", "teknoparrot.exe",
        "parrotloader.exe", "openparrotloader.exe", "openparrotkonamiloader.exe",
        "openparrotloader64.exe", "dolphin.exe", "dolphinnogui.exe", "play.exe"
    };
    return list;
}

std::unordered_set<std::string> toLowerSet(const QStringList& list)
{
    std::unordered_set<std::string> out;
    for (const auto& s : list)
        out.insert(s.toLower().toStdString());
    return out;
}

bool isIgnoredHelperClass(const QString& cls)
{
    // Visible-but-not-the-game windows that processes commonly own
    return cls.compare("ConsoleWindowClass", Qt::CaseInsensitive) == 0
        || cls.compare("IME", Qt::CaseInsensitive) == 0
        || cls.compare("MSCTFIME UI", Qt::CaseInsensitive) == 0;
}

bool hasInvalidGeometry(int x, int y, int w, int h)
{
    return (w <= 0 || h <= 0
        || x <= -32000 || x >= 32000
        || y <= -32000 || y >= 32000);
}

// Top-level windows owned by the target exe, in OS order.
std::vector<WindowManager::WindowInfo> windowsOfTarget(const QString& targetExePath)
{
    const QString targetPathLower = targetExePath.toLower();
    const QString targetExeLower = QFileInfo(targetExePath).fileName().toLower();

    std::vector<WindowManager::WindowInfo> windows;
    PlatformBackend& os = platform();
    for (const auto& tw : os.topLevelWindows()) {
        const QString imagePath = os.processImagePath(tw.pid);
        if (imagePath.isEmpty()) {
            continue;
        }

        const QString imageLower = imagePath.toLower();
        const QString imageExeLower = QFileInfo(imagePath).fileName().toLower();

        if (imageLower != targetPathLower && imageExeLower != targetExeLower) {
            continue;
        }

        // The filter lists only exclude *other* processes; an explicitly selected
        // target (e.g. dolphin.exe) must still be found.
        const bool explicitPathMatch = imageLower == targetPathLower;
        if (!explicitPathMatch
            && (ignoreListInternal().contains(imageExeLower, Qt::CaseInsensitive)
                || teknoLoaderListInternal().contains(imageExeLower, Qt::CaseInsensitive))) {
            continue;
        }

        WindowManager::WindowInfo info;
        info.hwnd = tw.handle;
        info.pid = tw.pid;
        info.title = os.windowTitle(tw.handle);
        info.className = os.windowClass(tw.handle);
        info.visible = os.windowState(tw.handle).visible;
        info.x = tw.rect.left;
        info.y = tw.rect.top;
        info.w = tw.rect.width();
        info.h = tw.rect.height();

        if (info.title.trimmed().isEmpty()) {
            info.title = "<untitled>";
        }

        windows.push_back(info);
    }
    return windows;
}

} // namespace

void WindowManager::setTargetExecutable(const QString& exePath)
{
    Logger::instance().info(
        "Target executable set: " + exePath.toStdString(), "WindowManager");
    m_targetExePath = exePath;
    AppConfig::instance().setCurrentExePath(exePath);
    AppConfig::instance().save();
}

QString WindowManager::targetExecutable() const
{
    return m_targetExePath;
}

const std::vector<WindowManager::WindowInfo>& WindowManager::scannedWindows() const
{
    return m_scanned;
}

quintptr WindowManager::activeWindow() const
{
    return m_activeWindow;
}

bool WindowManager::scanWindowsForTarget()
{
    m_scanned.clear();
    m_activeWindow = 0;

    if (m_targetExePath.trimmed().isEmpty()) {
        m_lastMessage = "No executable selected.";
        Logger::instance().warn("Scan aborted: no executable selected", "WindowManager");
        return false;
    }

    Logger::instance().info(
        "Scanning windows for: " + m_targetExePath.toStdString(), "WindowManager");

    m_scanned = windowsOfTarget(m_targetExePath);

    std::sort(m_scanned.begin(), m_scanned.end(), [](const WindowInfo& a, const WindowInfo& b) {
        if (a.visible != b.visible) return a.visible > b.visible;
        return static_cast<qint64>(a.w) * a.h > static_cast<qint64>(b.w) * b.h;
    });

    if (!m_scanned.empty()) {
        m_activeWindow = m_scanned.front().hwnd;
        for (const auto& win : m_scanned) {
            if (win.visible && !hasInvalidGeometry(win.x, win.y, win.w, win.h)) {
                m_activeWindow = win.hwnd;
                break;
            }
        }
        m_lastMessage = QString("Found %1 window(s) for target executable.").arg(m_scanned.size());
        Logger::instance().info(
            "Scan found " + std::to_string(m_scanned.size()) + " window(s). Active HWND: " +
            std::to_string(m_activeWindow), "WindowManager");
        return true;
    }

    m_lastMessage = "No matching top-level window was found.";
    Logger::instance().warn("Scan complete: no matching windows found", "WindowManager");
    return false;
}

bool WindowManager::ensureActiveWindow()
{
    if (m_activeWindow && platform().isValid(m_activeWindow)) {
        return true;
    }

    Logger::instance().info("Active window stale or missing; re-scanning", "WindowManager");
    if (scanWindowsForTarget() && m_activeWindow) {
        return true;
    }

    m_lastMessage = "No active window available.";
    Logger::instance().warn("ensureActiveWindow failed: no window found", "WindowManager");
    return false;
}

bool WindowManager::applyRect(quintptr hwnd, int x, int y, int w, int h, bool borderless)
{
    PlatformBackend& os = platform();
    if (!os.isValid(hwnd)) {
        m_lastMessage = "Target window is no longer valid.";
        Logger::instance().warn(
            "applyRect: HWND " + std::to_string(hwnd) + " is no longer valid",
            "WindowManager");
        return false;
    }
    Logger::instance().debug(
        "applyRect: hwnd=" + std::to_string(hwnd) +
        " x=" + std::to_string(x) + " y=" + std::to_string(y) +
        " w=" + std::to_string(w) + " h=" + std::to_string(h) +
        " borderless=" + (borderless ? "true" : "false"),
        "WindowManager");

    if (borderless) {
        os.stripFrame(hwnd);
    }

    if (!os.setBounds(hwnd, Fit::Rect::fromXYWH(x, y, w, h))) {
        m_lastMessage = "Failed to move/resize target window.";
        return false;
    }

    m_lastMessage = QString("Applied window settings: %1,%2 (%3x%4)").arg(x).arg(y).arg(w).arg(h);
    return true;
}

bool WindowManager::applySavedProfile()
{
    if (!ensureActiveWindow()) {
        return false;
    }

    const auto profile = AppConfig::instance().currentWindowProfile();
    if (!profile || !profile->hasProfile) {
        m_lastMessage = "No saved profile exists for the selected executable.";
        return false;
    }

    const bool ok = applyRect(m_activeWindow, profile->x, profile->y, profile->w, profile->h, true);
    if (ok) {
        Logger::instance().info("Applied saved profile", "WindowManager");
    }
    return ok;
}

bool WindowManager::saveCurrentPosition()
{
    if (!ensureActiveWindow()) {
        return false;
    }

    Fit::Rect rect;
    if (!platform().windowRect(m_activeWindow, rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    AppConfig::instance().saveCurrentWindowProfile(
        rect.left,
        rect.top,
        rect.width(),
        rect.height()
    );

    m_lastMessage = "Current window position saved.";
    return true;
}

bool WindowManager::moveToMonitor(int monitorIndex)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    PlatformBackend& os = platform();

    const auto mon = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!mon) {
        m_lastMessage = QString("Monitor %1 is not available.").arg(monitorIndex);
        return false;
    }

    // Restoring first avoids minimized/off-screen geometry values like
    // x=-32768, w=65535, h=0 when moving games between monitors.
    if (os.windowState(m_activeWindow).minimized) {
        os.show(m_activeWindow, PlatformBackend::ShowState::Restore);
    }

    Fit::Rect rect;
    if (!os.windowRect(m_activeWindow, rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    int w = rect.width();
    int h = rect.height();
    bool usedFallbackRect = false;

    const bool invalidRect = hasInvalidGeometry(rect.left, rect.top, w, h);

    if (invalidRect) {
        // Try another visible scanned window first. Some games expose multiple
        // top-level windows and the first one can be a transient helper window.
        for (const auto& candidate : m_scanned) {
            if (!candidate.visible || candidate.hwnd == m_activeWindow)
                continue;

            Fit::Rect candRect;
            if (!os.windowRect(candidate.hwnd, candRect))
                continue;

            const int cw = candRect.width();
            const int ch = candRect.height();
            const bool candidateInvalid = hasInvalidGeometry(candRect.left, candRect.top, cw, ch);

            if (!candidateInvalid) {
                m_activeWindow = candidate.hwnd;
                rect = candRect;
                w = cw;
                h = ch;

                Logger::instance().info(
                    "moveToMonitor: switched active window to visible candidate HWND " +
                    std::to_string(m_activeWindow),
                    "WindowManager");
                break;
            }
        }

        const bool stillInvalid = hasInvalidGeometry(rect.left, rect.top, w, h);

        if (stillInvalid) {
        // If the live rect is invalid, use saved profile size when available,
        // otherwise fall back to monitor work area size.
        const auto profile = AppConfig::instance().currentWindowProfile();
        if (profile && profile->hasProfile && profile->w > 0 && profile->h > 0) {
            w = profile->w;
            h = profile->h;
        } else {
            w = mon->w;
            h = mon->h;
        }

        Logger::instance().warn(
            "moveToMonitor: invalid source geometry detected, using fallback size " +
            std::to_string(w) + "x" + std::to_string(h),
            "WindowManager");
            usedFallbackRect = true;
        }
    }

    // For fallback rects, anchor at monitor origin to guarantee visibility.
    const int x = usedFallbackRect ? mon->x : (mon->x + (mon->w - w) / 2);
    const int y = usedFallbackRect ? mon->y : (mon->y + (mon->h - h) / 2);

    const bool ok = applyRect(m_activeWindow, x, y, w, h, false);
    if (ok) {
        os.show(m_activeWindow, PlatformBackend::ShowState::Show);
        os.bringToTop(m_activeWindow);

        // Some games immediately re-position their surface. Re-assert briefly,
        // similar to stabilizer behavior, but only during this explicit move.
        for (int i = 0; i < 3; ++i) {
            QThread::msleep(120);
            Fit::Rect now;
            if (!os.windowRect(m_activeWindow, now))
                break;

            const bool nowInvalid = hasInvalidGeometry(now.left, now.top, now.width(), now.height());
            const int actualMonitor = MonitorHelper::getMonitorIndexFromWindow(m_activeWindow);
            if (!nowInvalid && actualMonitor == monitorIndex)
                break;

            applyRect(m_activeWindow, mon->x, mon->y, w, h, false);
            os.show(m_activeWindow, PlatformBackend::ShowState::Show);
            os.bringToTop(m_activeWindow);
        }
    }
    return ok;
}

bool WindowManager::nudge(int dx, int dy, int dw, int dh, bool symmetric)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    Fit::Rect rect;
    if (!platform().windowRect(m_activeWindow, rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    int x = rect.left + dx;
    int y = rect.top + dy;
    int w = rect.width() + dw;
    int h = rect.height() + dh;

    if (symmetric) {
        // Grow/shrink around the centre: each edge moves by half the delta.
        x -= dw / 2;
        y -= dh / 2;
    }

    w = std::max(100, w);
    h = std::max(100, h);
    return applyRect(m_activeWindow, x, y, w, h, false);
}

bool WindowManager::setCustomSize(int width, int height)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    if (width < 100 || height < 100) {
        m_lastMessage = "Invalid size requested.";
        return false;
    }

    Fit::Rect rect;
    if (!platform().windowRect(m_activeWindow, rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    return applyRect(m_activeWindow, rect.left, rect.top, width, height, false);
}

bool WindowManager::applyHorizontalOverscan(int pixels)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    const auto mon = MonitorHelper::getMonitorWorkArea(1);
    if (!mon) {
        m_lastMessage = "Primary monitor work area not found.";
        return false;
    }

    const int clamped = std::max(0, pixels);
    const int w = std::max(100, mon->w - (clamped * 2));
    const int x = mon->x + clamped;
    return applyRect(m_activeWindow, x, mon->y, w, mon->h, false);
}

bool WindowManager::applyVerticalOverscan(int pixels)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    const auto mon = MonitorHelper::getMonitorWorkArea(1);
    if (!mon) {
        m_lastMessage = "Primary monitor work area not found.";
        return false;
    }

    const int clamped = std::max(0, pixels);
    const int h = std::max(100, mon->h - (clamped * 2));
    const int y = mon->y + clamped;
    return applyRect(m_activeWindow, mon->x, y, mon->w, h, false);
}

bool WindowManager::activeWindowRect(int& x, int& y, int& w, int& h) const
{
    if (!m_activeWindow) {
        return false;
    }

    Fit::Rect rect;
    if (!platform().windowRect(m_activeWindow, rect)) {
        return false;
    }

    x = rect.left;
    y = rect.top;
    w = rect.width();
    h = rect.height();
    return true;
}

void WindowManager::setActiveWindow(quintptr hwnd)
{
    m_activeWindow = hwnd;
}

const QStringList& WindowManager::ignoreList()
{
    return ignoreListInternal();
}

const QStringList& WindowManager::teknoLoaderList()
{
    return teknoLoaderListInternal();
}

// ---------------------------------------------------------------------------
// Window visibility / state
// ---------------------------------------------------------------------------

bool WindowManager::destroyWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().requestClose(m_activeWindow);
    m_lastMessage = "Sent WM_CLOSE to window.";
    return true;
}

bool WindowManager::hideWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().show(m_activeWindow, PlatformBackend::ShowState::Hide);
    m_lastMessage = "Window hidden.";
    return true;
}

bool WindowManager::showWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().show(m_activeWindow, PlatformBackend::ShowState::Show);
    m_lastMessage = "Window shown.";
    return true;
}

bool WindowManager::minimizeWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().show(m_activeWindow, PlatformBackend::ShowState::Minimize);
    m_lastMessage = "Window minimized.";
    return true;
}

bool WindowManager::maximizeWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().show(m_activeWindow, PlatformBackend::ShowState::Maximize);
    m_lastMessage = "Window maximized.";
    return true;
}

bool WindowManager::restoreWindow()
{
    if (!ensureActiveWindow()) return false;
    platform().show(m_activeWindow, PlatformBackend::ShowState::Restore);
    m_lastMessage = "Window restored.";
    return true;
}

// ---------------------------------------------------------------------------
// Window layout / style
// ---------------------------------------------------------------------------

bool WindowManager::setWindowed()
{
    if (!ensureActiveWindow()) return false;
    platform().restoreFrame(m_activeWindow);
    m_lastMessage = "Window restored to bordered windowed mode.";
    return true;
}

bool WindowManager::setBorderlessFullscreen()
{
    if (!ensureActiveWindow()) return false;

    const auto mon = platform().nearestMonitor(m_activeWindow);
    if (!mon) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }

    const Fit::Rect& r = mon->full;
    return applyRect(m_activeWindow, r.left, r.top, r.width(), r.height(), true);
}

bool WindowManager::setBorderlessSize(int monitorIndex, int w, int h)
{
    if (!ensureActiveWindow()) return false;
    const auto mon = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!mon) {
        m_lastMessage = QString("Monitor %1 not available.").arg(monitorIndex);
        return false;
    }
    return applyRect(m_activeWindow, mon->x, mon->y, w, h, true);
}

bool WindowManager::setFakeFullscreen(int w, int h)
{
    if (!ensureActiveWindow()) return false;

    const auto mon = platform().nearestMonitor(m_activeWindow);
    if (!mon) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }
    return applyRect(m_activeWindow, mon->work.left, mon->work.top, w, h, false);
}

bool WindowManager::setFakeFullscreenAspect(int w, int h)
{
    if (!ensureActiveWindow()) return false;

    const auto mon = platform().nearestMonitor(m_activeWindow);
    if (!mon) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }
    const Fit::Rect& work = mon->work;
    const int mw = work.width();
    const int mh = work.height();
    int fw = w, fh = h;
    if (fw > mw || fh > mh) {
        const double scale = std::min(
            static_cast<double>(mw) / fw,
            static_cast<double>(mh) / fh);
        fw = static_cast<int>(fw * scale);
        fh = static_cast<int>(fh * scale);
    }
    const int x = work.left + (mw - fw) / 2;
    const int y = work.top  + (mh - fh) / 2;
    return applyRect(m_activeWindow, x, y, fw, fh, false);
}

bool WindowManager::fitToScreen()
{
    if (!ensureActiveWindow()) return false;

    const auto mon = platform().nearestMonitor(m_activeWindow);
    if (!mon) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }

    const Fit::Rect& r = mon->work;
    return applyRect(m_activeWindow, r.left, r.top, r.width(), r.height(), false);
}

bool WindowManager::toggleTopmost()
{
    if (!ensureActiveWindow()) return false;
    if (!platform().supportsFlag(PlatformBackend::WindowFlag::Topmost)) {
        m_lastMessage = "This style is not supported on this system.";
        return false;
    }
    const bool nowSet = platform().toggleFlag(m_activeWindow, PlatformBackend::WindowFlag::Topmost);
    m_lastMessage = nowSet ? "Topmost set." : "Topmost removed.";
    return true;
}

bool WindowManager::toggleToolWindow()
{
    if (!ensureActiveWindow()) return false;
    if (!platform().supportsFlag(PlatformBackend::WindowFlag::ToolWindow)) {
        m_lastMessage = "This style is not supported on this system.";
        return false;
    }
    const bool nowSet = platform().toggleFlag(m_activeWindow, PlatformBackend::WindowFlag::ToolWindow);
    m_lastMessage = nowSet ? "Tool window style set." : "Tool window style removed.";
    return true;
}

bool WindowManager::toggleLayered()
{
    if (!ensureActiveWindow()) return false;
    if (!platform().supportsFlag(PlatformBackend::WindowFlag::Layered)) {
        m_lastMessage = "This style is not supported on this system.";
        return false;
    }
    const bool nowSet = platform().toggleFlag(m_activeWindow, PlatformBackend::WindowFlag::Layered);
    m_lastMessage = nowSet ? "Layered style set (alpha=255)." : "Layered style removed.";
    return true;
}

bool WindowManager::toggleNoActivate()
{
    if (!ensureActiveWindow()) return false;
    if (!platform().supportsFlag(PlatformBackend::WindowFlag::NoActivate)) {
        m_lastMessage = "This style is not supported on this system.";
        return false;
    }
    const bool nowSet = platform().toggleFlag(m_activeWindow, PlatformBackend::WindowFlag::NoActivate);
    m_lastMessage = nowSet ? "No-activate style set." : "No-activate style removed.";
    return true;
}

bool WindowManager::resetAll()
{
    if (!ensureActiveWindow()) return false;
    platform().resetWindow(m_activeWindow);

    // Clear saved profile
    AppConfig::instance().clearWindowProfile(m_targetExePath);
    AppConfig::instance().save();

    m_lastMessage = "Window reset to default state.";
    return true;
}

// ---------------------------------------------------------------------------
// Process control
// ---------------------------------------------------------------------------

void WindowManager::snapshotProcesses()
{
    m_preLaunchPids.clear();
    m_batchLaunchPending = true;

    for (const auto& p : platform().processes())
        m_preLaunchPids.insert(p.pid);

    Logger::instance().info(
        "Pre-launch snapshot: " + std::to_string(m_preLaunchPids.size()) + " running processes",
        "WindowManager");
}

bool WindowManager::pollForBatchChild()
{
    if (!m_batchLaunchPending) return false;

    // Processes that are just noise from cmd / batch execution
    static const QStringList noise{
        "cmd.exe", "conhost.exe", "werfault.exe", "taskhostw.exe", "timeout.exe"
    };

    PlatformBackend& os = platform();
    const auto windows = os.topLevelWindows();
    bool found = false;

    for (const auto& proc : os.processes()) {
        const uint32_t pid = proc.pid;

        // Skip processes that existed before launch
        if (m_preLaunchPids.count(pid)) continue;

        const QString& name = proc.exeName;
        const QString nameLower = name.toLower();

        // Filter out noise and ignored system processes
        if (noise.contains(nameLower, Qt::CaseInsensitive)) continue;
        if (ignoreListInternal().contains(nameLower, Qt::CaseInsensitive)) continue;

        // Find a visible window owned by this PID
        const bool hasVisibleWindow = std::any_of(windows.begin(), windows.end(),
            [&](const PlatformBackend::TopLevelWindow& w) {
                return w.pid == static_cast<qint64>(pid) && os.windowState(w.handle).visible;
            });
        if (!hasVisibleWindow) continue;

        const QString fullPath = os.processImagePath(pid);
        Logger::instance().info(
            "Batch child detected: " + name.toStdString() +
            " (PID: " + std::to_string(pid) + ")",
            "WindowManager");

        // Remember which exe this script starts, so "is it running" works later
        if (!fullPath.isEmpty())
            if (auto game = AppConfig::instance().gameByPath(m_targetExePath)) {
                game->childExePath = fullPath;
                AppConfig::instance().saveGame(*game);
            }

        // Switch the tracked target to the real game exe
        m_targetExePath = fullPath.isEmpty() ? name : fullPath;
        AppConfig::instance().setCurrentExePath(m_targetExePath);
        AppConfig::instance().save();

        m_sessionPids.insert(pid);
        m_batchLaunchPending = false;
        found = true;
        break;
    }

    if (found) {
        // Immediately populate the window table
        scanWindowsForTarget();
    }
    return found;
}

bool WindowManager::launchTarget()
{
    if (m_targetExePath.trimmed().isEmpty()) {
        m_lastMessage = "No launch target selected.";
        Logger::instance().warn("launchTarget: no launch target selected", "WindowManager");
        return false;
    }
    const QString workingDir = QFileInfo(m_targetExePath).absolutePath();

    // Fail-safe: never start a second copy of a game that is already running.
    if (isTargetRunning()) {
        m_lastMessage = "Already running: close the game first, then launch it again.";
        Logger::instance().warn(
            "Launch refused: already running: " + m_targetExePath.toStdString(), "WindowManager");
        return false;
    }

    Logger::instance().info(
        "Launching: " + m_targetExePath.toStdString(), "WindowManager");

    qint64 pid = 0;
    const bool ok = platform().startProcess(m_targetExePath, workingDir, &pid);

    if (ok) {
        m_sessionPids.clear();
        if (pid > 0)
            m_sessionPids.insert(static_cast<uint32_t>(pid));
        m_launchTime = platform().now();
    }

    m_lastMessage = ok ? "Launch target started." : "Failed to launch target.";
    Logger::instance().info(
        ok ? "Launch succeeded" : "Launch failed", "WindowManager");
    return ok;
}

bool WindowManager::focusTarget()
{
    if (!ensureActiveWindow()) return false;
    platform().focus(m_activeWindow);
    m_lastMessage = "Focus restored to game window.";
    return true;
}

int WindowManager::activeWindowMonitorIndex() const
{
    if (!m_activeWindow) return 0;
    return MonitorHelper::getMonitorIndexFromWindow(m_activeWindow);
}

bool WindowManager::isTargetRunning() const
{
    return isExeRunning(m_targetExePath);
}

QString WindowManager::realExePath(const QString& exePath)
{
    const QString suffix = QFileInfo(exePath).suffix();
    if (suffix.compare("bat", Qt::CaseInsensitive) != 0 && suffix.compare("cmd", Qt::CaseInsensitive) != 0)
        return exePath;
    const auto game = AppConfig::instance().gameByPath(exePath);
    return game ? game->childExePath : QString();   // empty: not known yet
}

bool WindowManager::isExeRunning(const QString& exePath)
{
    const QString real = realExePath(exePath);
    if (real.trimmed().isEmpty())
        return false;
    const QString wanted = QDir::toNativeSeparators(QFileInfo(real).absoluteFilePath()).toLower();

    PlatformBackend& os = platform();
    for (const auto& proc : os.processes()) {
        if (proc.pid == os.currentPid())
            continue;
        const QString path = os.processImagePath(proc.pid);
        if (!path.isEmpty() && QDir::toNativeSeparators(path).toLower() == wanted)
            return true;
    }
    return false;
}

bool WindowManager::killTarget()
{
    Logger::instance().info("killTarget called", "WindowManager");

    // Snapshot every process, then let KillPlanner pick the ones that belong
    // to this session. Unrelated loaders/emulators are left alone.
    PlatformBackend& os = platform();
    std::vector<KillPlanner::ProcInfo> procs;
    for (const auto& proc : os.processes()) {
        KillPlanner::ProcInfo p;
        p.pid = proc.pid;
        p.ppid = proc.ppid;
        p.created = os.processCreationTime(proc.pid);
        p.exeLower = proc.exeName.toLower().toStdString();
        p.pathLower = os.processImagePath(proc.pid).toLower().toStdString();
        procs.push_back(std::move(p));
    }

    KillPlanner::Request req;
    req.rootPids = m_sessionPids;
    const QString realTarget = realExePath(m_targetExePath);   // a script's game exe, not the script
    req.targetPathLower = QDir::toNativeSeparators(realTarget).toLower().toStdString();
    req.targetExeLower = QFileInfo(realTarget).fileName().toLower().toStdString();
    req.launchTime = m_launchTime;
    req.ignoreExes = toLowerSet(ignoreListInternal());
    req.loaderExes = toLowerSet(teknoLoaderListInternal());
    req.selfPid = os.currentPid();

    if (m_activeWindow && os.isValid(m_activeWindow)) {
        const qint64 pid = os.windowPid(m_activeWindow);
        if (pid > 0)
            req.rootPids.insert(static_cast<uint32_t>(pid));
    }

    int count = 0;
    for (const uint32_t pid : KillPlanner::plan(procs, req)) {
        if (os.terminateProcess(pid))
            ++count;
    }

    m_sessionPids.clear();
    m_launchTime = 0;
    m_activeWindow = 0;
    m_scanned.clear();
    m_lastMessage = QString("Killed %1 process(es).").arg(count);
    Logger::instance().info(
        "killTarget: terminated " + std::to_string(count) + " process(es)",
        "WindowManager");
    return count > 0;
}

QString WindowManager::lastMessage() const
{
    return m_lastMessage;
}

QString WindowManager::processImagePath(qint64 pid)
{
    return platform().processImagePath(pid);
}

QString WindowManager::windowTitle(quintptr hwnd)
{
    return platform().windowTitle(hwnd);
}

QString WindowManager::windowClass(quintptr hwnd)
{
    return platform().windowClass(hwnd);
}

// ---------------------------------------------------------------------------
// Fit to monitor
// ---------------------------------------------------------------------------

WindowManager::PrimaryWindow WindowManager::findPrimaryWindow() const
{
    PrimaryWindow best;
    if (m_targetExePath.trimmed().isEmpty())
        return best;

    PlatformBackend& os = platform();
    qint64 bestScore = -1;
    for (const auto& w : windowsOfTarget(m_targetExePath)) {
        if (isIgnoredHelperClass(w.className))
            continue;

        const qint64 area = static_cast<qint64>(w.w) * w.h;
        const bool titled = w.title != "<untitled>";
        const bool ready = w.visible && !os.windowState(w.hwnd).minimized && area >= 10000
                           && !hasInvalidGeometry(w.x, w.y, w.w, w.h);

        // Ready windows beat everything, then titled ones, then the largest.
        const qint64 score = (ready ? (qint64(1) << 60) : 0)
                           + (titled ? (qint64(1) << 59) : 0)
                           + std::max<qint64>(area, 0);
        if (score > bestScore) {
            bestScore = score;
            best.hwnd = w.hwnd;
            best.ready = ready;
            best.rect = Fit::Rect::fromXYWH(w.x, w.y, w.w, w.h);
        }
    }
    return best;
}

bool WindowManager::measureFit(const FitRequest& request, FitMeasurement& out)
{
    out = FitMeasurement{};

    if (!ensureActiveWindow()) {
        out.error = m_lastMessage;
        return false;
    }

    PlatformBackend& os = platform();
    const PlatformBackend::WindowState state = os.windowState(m_activeWindow);
    if (state.minimized) {
        out.error = "The window is minimized. Restore it first.";
        return false;
    }

    Fit::Rect outer;
    Fit::Rect client;
    if (!os.windowRect(m_activeWindow, outer) || !os.clientRect(m_activeWindow, client)) {
        out.error = "Failed to read the window rectangle.";
        return false;
    }

    const Fit::Rect frame = os.visibleFrame(m_activeWindow).value_or(outer);

    const int monitorIndex = request.monitorIndex > 0
        ? request.monitorIndex
        : MonitorHelper::getMonitorIndexFromWindow(m_activeWindow);
    const auto monitor = MonitorHelper::getMonitorGeometry(monitorIndex);
    const auto work = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!monitor || !work) {
        out.error = QString("Monitor %1 is not available.").arg(monitorIndex);
        return false;
    }

    out.hwnd = m_activeWindow;
    out.title = os.windowTitle(m_activeWindow);
    out.className = os.windowClass(m_activeWindow);
    out.hasFrame = state.hasFrame;
    out.outer = outer;
    out.client = client;
    out.visibleFrame = frame;
    out.monitorIndex = monitorIndex;
    out.monitor = Fit::Rect::fromXYWH(monitor->x, monitor->y, monitor->w, monitor->h);
    out.target = request.workArea
        ? Fit::Rect::fromXYWH(work->x, work->y, work->w, work->h)
        : out.monitor;
    out.stripFrame = request.stripFrame && out.hasFrame;
    out.plan = Fit::compute({out.outer, out.client, out.target, out.stripFrame});
    out.valid = true;
    return true;
}

bool WindowManager::applyFit(const FitRequest& request)
{
    FitMeasurement m;
    if (!measureFit(request, m)) {
        m_lastMessage = m.error;
        return false;
    }

    PlatformBackend& os = platform();

    // A maximized window ignores resize requests; start from normal state.
    if (os.windowState(m.hwnd).maximized) {
        os.show(m.hwnd, PlatformBackend::ShowState::Restore);
        QThread::msleep(80);
    }

    if (m.stripFrame) {
        os.stripFrame(m.hwnd);
    }

    // Measure the real insets *after* the style change and correct; repeat a
    // couple of times because some games nudge their window right after a move.
    FitRequest pass = request;
    pass.stripFrame = false;
    constexpr int kMaxPasses = 3;
    for (int i = 0; i < kMaxPasses; ++i) {
        if (!measureFit(pass, m)) {
            m_lastMessage = m.error;
            return false;
        }
        if (!m.plan.needsChange())
            break;

        const Fit::Rect& r = m.plan.desiredOuter;
        Logger::instance().info(
            "applyFit pass " + std::to_string(i + 1) + ": dx=" + std::to_string(m.plan.dx) +
            " dy=" + std::to_string(m.plan.dy) + " dw=" + std::to_string(m.plan.dw) +
            " dh=" + std::to_string(m.plan.dh), "WindowManager");
        os.setBounds(m.hwnd, r);
        QThread::msleep(120);
    }

    if (!measureFit(pass, m)) {
        m_lastMessage = m.error;
        return false;
    }

    const bool exact = !m.plan.needsChange();
    if (exact) {
        m_lastMessage = QString("Picture fits monitor %1 exactly (%2x%3 at %4,%5).")
            .arg(m.monitorIndex).arg(m.client.width()).arg(m.client.height())
            .arg(m.client.left).arg(m.client.top);
    } else {
        m_lastMessage = QString("Applied, but the game keeps resizing: still off by "
                                "x%1 y%2 w%3 h%4.")
            .arg(m.plan.dx).arg(m.plan.dy).arg(m.plan.dw).arg(m.plan.dh);
    }

    // Remember the result so "Force saved settings" can restore it. Only valid
    // when the frame is gone, because applying a saved rect also strips the frame.
    if (!m.hasFrame) {
        AppConfig::instance().saveCurrentWindowProfile(
            m.outer.left, m.outer.top, m.outer.width(), m.outer.height());
    }
    return exact;
}
