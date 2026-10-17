#include "WindowManager.h"

#include "../config/AppConfig.h"
#include "../core/Logger.h"
#include "KillPlanner.h"
#include "MonitorHelper.h"

#include <Windows.h>
#include <dwmapi.h>
#include <tlhelp32.h>

#include <QDir>
#include <QFileInfo>
#include <QProcess>

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

uint64_t filetimeToU64(const FILETIME& ft)
{
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

uint64_t nowFileTime()
{
    FILETIME ft{};
    GetSystemTimeAsFileTime(&ft);
    return filetimeToU64(ft);
}

uint64_t processCreationTime(DWORD pid)
{
    const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return 0;
    FILETIME created{}, exited{}, kernel{}, user{};
    const BOOL ok = GetProcessTimes(h, &created, &exited, &kernel, &user);
    CloseHandle(h);
    return ok ? filetimeToU64(created) : 0;
}

std::unordered_set<std::string> toLowerSet(const QStringList& list)
{
    std::unordered_set<std::string> out;
    for (const auto& s : list)
        out.insert(s.toLower().toStdString());
    return out;
}

Fit::Rect toFitRect(const RECT& r)
{
    return Fit::Rect{r.left, r.top, r.right, r.bottom};
}

bool isIgnoredHelperClass(const QString& cls)
{
    // Visible-but-not-the-game windows that processes commonly own
    return cls.compare("ConsoleWindowClass", Qt::CaseInsensitive) == 0
        || cls.compare("IME", Qt::CaseInsensitive) == 0
        || cls.compare("MSCTFIME UI", Qt::CaseInsensitive) == 0;
}

struct ScanContext
{
    QString targetPathLower;
    QString targetExeLower;
    std::vector<WindowManager::WindowInfo> windows;
};

bool hasInvalidGeometry(int x, int y, int w, int h)
{
    return (w <= 0 || h <= 0
        || x <= -32000 || x >= 32000
        || y <= -32000 || y >= 32000);
}

QString toQString(const wchar_t* src)
{
    return QString::fromWCharArray(src ? src : L"");
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    auto* ctx = reinterpret_cast<ScanContext*>(lParam);
    if (!ctx) {
        return TRUE;
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        return TRUE;
    }

    const int width = rect.right - rect.left;
    const int height = rect.bottom - rect.top;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) {
        return TRUE;
    }

    const QString imagePath = WindowManager::processImagePath(static_cast<qint64>(pid));
    if (imagePath.isEmpty()) {
        return TRUE;
    }

    const QString imageLower = imagePath.toLower();
    const QString imageExeLower = QFileInfo(imagePath).fileName().toLower();

    if (imageLower != ctx->targetPathLower && imageExeLower != ctx->targetExeLower) {
        return TRUE;
    }

    // The filter lists only exclude *other* processes; an explicitly selected
    // target (e.g. dolphin.exe) must still be found.
    const bool explicitPathMatch = imageLower == ctx->targetPathLower;
    if (!explicitPathMatch
        && (ignoreListInternal().contains(imageExeLower, Qt::CaseInsensitive)
            || teknoLoaderListInternal().contains(imageExeLower, Qt::CaseInsensitive))) {
        return TRUE;
    }

    WindowManager::WindowInfo info;
    info.hwnd = reinterpret_cast<quintptr>(hwnd);
    info.pid = static_cast<qint64>(pid);
    info.title = WindowManager::windowTitle(info.hwnd);
    info.className = WindowManager::windowClass(info.hwnd);
    info.visible = IsWindowVisible(hwnd) != FALSE;
    info.x = rect.left;
    info.y = rect.top;
    info.w = width;
    info.h = height;

    if (info.title.trimmed().isEmpty()) {
        info.title = "<untitled>";
    }

    ctx->windows.push_back(info);
    return TRUE;
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

    ScanContext ctx;
    ctx.targetPathLower = m_targetExePath.toLower();
    ctx.targetExeLower = QFileInfo(m_targetExePath).fileName().toLower();

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    m_scanned = std::move(ctx.windows);

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
    if (m_activeWindow && IsWindow(reinterpret_cast<HWND>(m_activeWindow))) {
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

bool WindowManager::applyRect(quintptr hwndRaw, int x, int y, int w, int h, bool borderless)
{
    const HWND hwnd = reinterpret_cast<HWND>(hwndRaw);
    if (!IsWindow(hwnd)) {
        m_lastMessage = "Target window is no longer valid.";
        Logger::instance().warn(
            "applyRect: HWND " + std::to_string(hwndRaw) + " is no longer valid",
            "WindowManager");
        return false;
    }
    Logger::instance().debug(
        "applyRect: hwnd=" + std::to_string(hwndRaw) +
        " x=" + std::to_string(x) + " y=" + std::to_string(y) +
        " w=" + std::to_string(w) + " h=" + std::to_string(h) +
        " borderless=" + (borderless ? "true" : "false"),
        "WindowManager");

    if (borderless) {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    }

    const BOOL moved = SetWindowPos(
        hwnd,
        HWND_TOP,
        x,
        y,
        w,
        h,
        SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );

    if (!moved) {
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

    RECT rect{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_activeWindow), &rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    AppConfig::instance().saveCurrentWindowProfile(
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top
    );

    m_lastMessage = "Current window position saved.";
    return true;
}

bool WindowManager::moveToMonitor(int monitorIndex)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const auto mon = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!mon) {
        m_lastMessage = QString("Monitor %1 is not available.").arg(monitorIndex);
        return false;
    }

    // Restoring first avoids minimized/off-screen geometry values like
    // x=-32768, w=65535, h=0 when moving games between monitors.
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
    }

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    int w = rect.right - rect.left;
    int h = rect.bottom - rect.top;
    bool usedFallbackRect = false;

    const bool invalidRect = hasInvalidGeometry(rect.left, rect.top, w, h);

    if (invalidRect) {
        // Try another visible scanned window first. Some games expose multiple
        // top-level windows and the first one can be a transient helper window.
        for (const auto& candidate : m_scanned) {
            if (!candidate.visible || candidate.hwnd == m_activeWindow)
                continue;

            const HWND candHwnd = reinterpret_cast<HWND>(candidate.hwnd);
            RECT candRect{};
            if (!GetWindowRect(candHwnd, &candRect))
                continue;

            const int cw = candRect.right - candRect.left;
            const int ch = candRect.bottom - candRect.top;
            const bool candidateInvalid = hasInvalidGeometry(candRect.left, candRect.top, cw, ch);

            if (!candidateInvalid) {
                m_activeWindow = candidate.hwnd;
                hwnd = candHwnd;
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
        ShowWindow(hwnd, SW_SHOW);
        BringWindowToTop(hwnd);

        // Some games immediately re-position their surface. Re-assert briefly,
        // similar to stabilizer behavior, but only during this explicit move.
        for (int i = 0; i < 3; ++i) {
            Sleep(120);
            RECT now{};
            if (!GetWindowRect(hwnd, &now))
                break;

            const int nw = now.right - now.left;
            const int nh = now.bottom - now.top;
            const bool nowInvalid = hasInvalidGeometry(now.left, now.top, nw, nh);
            const int actualMonitor = MonitorHelper::getMonitorIndexFromWindow(hwnd);
            if (!nowInvalid && actualMonitor == monitorIndex)
                break;

            applyRect(m_activeWindow, mon->x, mon->y, w, h, false);
            ShowWindow(hwnd, SW_SHOW);
            BringWindowToTop(hwnd);
        }
    }
    return ok;
}

bool WindowManager::nudge(int dx, int dy, int dw, int dh, bool symmetric)
{
    if (!ensureActiveWindow()) {
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_activeWindow), &rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    int x = rect.left + dx;
    int y = rect.top + dy;
    int w = (rect.right - rect.left) + dw;
    int h = (rect.bottom - rect.top) + dh;

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

    RECT rect{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_activeWindow), &rect)) {
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

    RECT rect{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_activeWindow), &rect)) {
        return false;
    }

    x = rect.left;
    y = rect.top;
    w = rect.right - rect.left;
    h = rect.bottom - rect.top;
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
    PostMessage(reinterpret_cast<HWND>(m_activeWindow), WM_CLOSE, 0, 0);
    m_lastMessage = "Sent WM_CLOSE to window.";
    return true;
}

bool WindowManager::hideWindow()
{
    if (!ensureActiveWindow()) return false;
    ShowWindow(reinterpret_cast<HWND>(m_activeWindow), SW_HIDE);
    m_lastMessage = "Window hidden.";
    return true;
}

bool WindowManager::showWindow()
{
    if (!ensureActiveWindow()) return false;
    ShowWindow(reinterpret_cast<HWND>(m_activeWindow), SW_SHOW);
    m_lastMessage = "Window shown.";
    return true;
}

bool WindowManager::minimizeWindow()
{
    if (!ensureActiveWindow()) return false;
    ShowWindow(reinterpret_cast<HWND>(m_activeWindow), SW_MINIMIZE);
    m_lastMessage = "Window minimized.";
    return true;
}

bool WindowManager::maximizeWindow()
{
    if (!ensureActiveWindow()) return false;
    ShowWindow(reinterpret_cast<HWND>(m_activeWindow), SW_MAXIMIZE);
    m_lastMessage = "Window maximized.";
    return true;
}

bool WindowManager::restoreWindow()
{
    if (!ensureActiveWindow()) return false;
    ShowWindow(reinterpret_cast<HWND>(m_activeWindow), SW_RESTORE);
    m_lastMessage = "Window restored.";
    return true;
}

// ---------------------------------------------------------------------------
// Window layout / style
// ---------------------------------------------------------------------------

bool WindowManager::setWindowed()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_WINDOWEDGE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    m_lastMessage = "Window restored to bordered windowed mode.";
    return true;
}

bool WindowManager::setBorderlessFullscreen()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }

    const int x = mi.rcMonitor.left;
    const int y = mi.rcMonitor.top;
    const int w = mi.rcMonitor.right - mi.rcMonitor.left;
    const int h = mi.rcMonitor.bottom - mi.rcMonitor.top;
    return applyRect(m_activeWindow, x, y, w, h, true);
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
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }
    return applyRect(m_activeWindow, mi.rcWork.left, mi.rcWork.top, w, h, false);
}

bool WindowManager::setFakeFullscreenAspect(int w, int h)
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }
    const int mw = mi.rcWork.right  - mi.rcWork.left;
    const int mh = mi.rcWork.bottom - mi.rcWork.top;
    int fw = w, fh = h;
    if (fw > mw || fh > mh) {
        const double scale = std::min(
            static_cast<double>(mw) / fw,
            static_cast<double>(mh) / fh);
        fw = static_cast<int>(fw * scale);
        fh = static_cast<int>(fh * scale);
    }
    const int x = mi.rcWork.left + (mw - fw) / 2;
    const int y = mi.rcWork.top  + (mh - fh) / 2;
    return applyRect(m_activeWindow, x, y, fw, fh, false);
}

bool WindowManager::fitToScreen()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hmon, &mi)) {
        m_lastMessage = "Failed to get monitor info.";
        return false;
    }

    const int x = mi.rcWork.left;
    const int y = mi.rcWork.top;
    const int w = mi.rcWork.right - mi.rcWork.left;
    const int h = mi.rcWork.bottom - mi.rcWork.top;
    return applyRect(m_activeWindow, x, y, w, h, false);
}

bool WindowManager::toggleTopmost()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    const bool isTopmost = (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    SetWindowPos(hwnd, isTopmost ? HWND_NOTOPMOST : HWND_TOPMOST,
        0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    m_lastMessage = isTopmost ? "Topmost removed." : "Topmost set.";
    return true;
}

bool WindowManager::toggleToolWindow()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) {
        exStyle &= ~WS_EX_TOOLWINDOW;
        m_lastMessage = "Tool window style removed.";
    } else {
        exStyle |= WS_EX_TOOLWINDOW;
        m_lastMessage = "Tool window style set.";
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    return true;
}

bool WindowManager::toggleLayered()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_LAYERED) {
        exStyle &= ~WS_EX_LAYERED;
        m_lastMessage = "Layered style removed.";
    } else {
        exStyle |= WS_EX_LAYERED;
        SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        m_lastMessage = "Layered style set (alpha=255).";
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    return true;
}

bool WindowManager::toggleNoActivate()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_NOACTIVATE) {
        exStyle &= ~WS_EX_NOACTIVATE;
        m_lastMessage = "No-activate style removed.";
    } else {
        exStyle |= WS_EX_NOACTIVATE;
        m_lastMessage = "No-activate style set.";
    }
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    return true;
}

bool WindowManager::resetAll()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);

    // Restore standard window styles
    LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    style |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
    SetWindowLongPtr(hwnd, GWL_STYLE, style);

    // Strip extended overrides
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle &= ~(WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE |
                 WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    exStyle |= WS_EX_WINDOWEDGE;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    // Remove topmost, restore if minimised/hidden
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(hwnd, SW_RESTORE);
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);

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

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            m_preLaunchPids.insert(static_cast<uint32_t>(entry.th32ProcessID));
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

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

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return false;

    struct FindWindowCtx { DWORD pid; HWND hwnd; };

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    bool found = false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            const DWORD pid = entry.th32ProcessID;

            // Skip processes that existed before launch
            if (m_preLaunchPids.count(static_cast<uint32_t>(pid))) continue;

            const QString name = QString::fromWCharArray(entry.szExeFile);
            const QString nameLower = name.toLower();

            // Filter out noise and ignored system processes
            if (noise.contains(nameLower, Qt::CaseInsensitive)) continue;
            if (ignoreListInternal().contains(nameLower, Qt::CaseInsensitive)) continue;

            // Find a visible window owned by this PID
            FindWindowCtx ctx{pid, nullptr};
            EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
                auto* fc = reinterpret_cast<FindWindowCtx*>(lp);
                DWORD wpid = 0;
                GetWindowThreadProcessId(hwnd, &wpid);
                if (wpid == fc->pid && IsWindowVisible(hwnd)) {
                    fc->hwnd = hwnd;
                    return FALSE; // stop enumeration
                }
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));

            if (ctx.hwnd) {
                const QString fullPath = processImagePath(static_cast<qint64>(pid));
                Logger::instance().info(
                    "Batch child detected: " + name.toStdString() +
                    " (PID: " + std::to_string(pid) + ")",
                    "WindowManager");

                // Switch the tracked target to the real game exe
                m_targetExePath = fullPath.isEmpty() ? name : fullPath;
                AppConfig::instance().setCurrentExePath(m_targetExePath);
                AppConfig::instance().save();

                m_sessionPids.insert(static_cast<uint32_t>(pid));
                m_batchLaunchPending = false;
                found = true;
                break;
            }

        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);

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
    Logger::instance().info(
        "Launching: " + m_targetExePath.toStdString(), "WindowManager");
    const QFileInfo targetInfo(m_targetExePath);
    const QString workingDir = targetInfo.absolutePath();

    const bool isBatch =
        targetInfo.suffix().compare("bat", Qt::CaseInsensitive) == 0 ||
        targetInfo.suffix().compare("cmd", Qt::CaseInsensitive) == 0;

    qint64 pid = 0;
    bool ok = false;
    if (isBatch) {
        ok = QProcess::startDetached("cmd.exe", {"/c", m_targetExePath}, workingDir, &pid);
    } else {
        ok = QProcess::startDetached(m_targetExePath, {}, workingDir, &pid);
    }

    if (ok) {
        m_sessionPids.clear();
        if (pid > 0)
            m_sessionPids.insert(static_cast<uint32_t>(pid));
        m_launchTime = nowFileTime();
    }

    m_lastMessage = ok ? "Launch target started." : "Failed to launch target.";
    Logger::instance().info(
        ok ? "Launch succeeded" : "Launch failed", "WindowManager");
    return ok;
}

bool WindowManager::focusTarget()
{
    if (!ensureActiveWindow()) return false;
    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);
    // AllowSetForegroundWindow bypasses the foreground lock so we can steal focus
    AllowSetForegroundWindow(ASFW_ANY);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    BringWindowToTop(hwnd);
    m_lastMessage = "Focus restored to game window.";
    return true;
}

int WindowManager::activeWindowMonitorIndex() const
{
    if (!m_activeWindow) return 0;
    return MonitorHelper::getMonitorIndexFromWindow(reinterpret_cast<void*>(m_activeWindow));
}

bool WindowManager::killTarget()
{
    Logger::instance().info("killTarget called", "WindowManager");

    // Snapshot every process, then let KillPlanner pick the ones that belong
    // to this session. Unrelated loaders/emulators are left alone.
    std::vector<KillPlanner::ProcInfo> procs;
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                KillPlanner::ProcInfo p;
                p.pid = entry.th32ProcessID;
                p.ppid = entry.th32ParentProcessID;
                p.created = processCreationTime(entry.th32ProcessID);
                p.exeLower = QString::fromWCharArray(entry.szExeFile).toLower().toStdString();
                p.pathLower = processImagePath(entry.th32ProcessID).toLower().toStdString();
                procs.push_back(std::move(p));
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    KillPlanner::Request req;
    req.rootPids = m_sessionPids;
    req.targetPathLower = QDir::toNativeSeparators(m_targetExePath).toLower().toStdString();
    req.targetExeLower = QFileInfo(m_targetExePath).fileName().toLower().toStdString();
    req.launchTime = m_launchTime;
    req.ignoreExes = toLowerSet(ignoreListInternal());
    req.loaderExes = toLowerSet(teknoLoaderListInternal());
    req.selfPid = GetCurrentProcessId();

    if (m_activeWindow && IsWindow(reinterpret_cast<HWND>(m_activeWindow))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(reinterpret_cast<HWND>(m_activeWindow), &pid);
        if (pid > 0)
            req.rootPids.insert(pid);
    }

    int count = 0;
    for (const uint32_t pid : KillPlanner::plan(procs, req)) {
        if (const HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid)) {
            if (TerminateProcess(proc, 1))
                ++count;
            CloseHandle(proc);
        }
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
    const HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!proc) {
        return {};
    }

    std::wstring buf;
    buf.resize(2048);
    DWORD len = static_cast<DWORD>(buf.size());

    const BOOL ok = QueryFullProcessImageNameW(proc, 0, buf.data(), &len);
    CloseHandle(proc);

    if (!ok || len == 0) {
        return {};
    }

    buf.resize(len);
    return QString::fromStdWString(buf);
}

QString WindowManager::windowTitle(quintptr hwndRaw)
{
    const HWND hwnd = reinterpret_cast<HWND>(hwndRaw);
    wchar_t title[1024]{};
    const int len = GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    if (len <= 0) {
        return {};
    }
    return toQString(title);
}

QString WindowManager::windowClass(quintptr hwndRaw)
{
    const HWND hwnd = reinterpret_cast<HWND>(hwndRaw);
    wchar_t cls[256]{};
    const int len = GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls)));
    if (len <= 0) {
        return {};
    }
    return toQString(cls);
}

// ---------------------------------------------------------------------------
// Fit to monitor
// ---------------------------------------------------------------------------

WindowManager::PrimaryWindow WindowManager::findPrimaryWindow() const
{
    PrimaryWindow best;
    if (m_targetExePath.trimmed().isEmpty())
        return best;

    ScanContext ctx;
    ctx.targetPathLower = m_targetExePath.toLower();
    ctx.targetExeLower = QFileInfo(m_targetExePath).fileName().toLower();
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    qint64 bestScore = -1;
    for (const auto& w : ctx.windows) {
        if (isIgnoredHelperClass(w.className))
            continue;

        const HWND hwnd = reinterpret_cast<HWND>(w.hwnd);
        const qint64 area = static_cast<qint64>(w.w) * w.h;
        const bool titled = w.title != "<untitled>";
        const bool ready = w.visible && !IsIconic(hwnd) && area >= 10000
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

    const HWND hwnd = reinterpret_cast<HWND>(m_activeWindow);
    if (IsIconic(hwnd)) {
        out.error = "The window is minimized. Restore it first.";
        return false;
    }

    RECT outer{};
    RECT clientLocal{};
    if (!GetWindowRect(hwnd, &outer) || !GetClientRect(hwnd, &clientLocal)) {
        out.error = "Failed to read the window rectangle.";
        return false;
    }

    POINT origin{0, 0};
    ClientToScreen(hwnd, &origin);
    const RECT client{origin.x, origin.y,
                      origin.x + (clientLocal.right - clientLocal.left),
                      origin.y + (clientLocal.bottom - clientLocal.top)};

    RECT frame = outer;
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &frame, sizeof(frame));

    const int monitorIndex = request.monitorIndex > 0
        ? request.monitorIndex
        : MonitorHelper::getMonitorIndexFromWindow(hwnd);
    const auto monitor = MonitorHelper::getMonitorGeometry(monitorIndex);
    const auto work = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!monitor || !work) {
        out.error = QString("Monitor %1 is not available.").arg(monitorIndex);
        return false;
    }

    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);

    out.hwnd = m_activeWindow;
    out.title = windowTitle(m_activeWindow);
    out.className = windowClass(m_activeWindow);
    out.hasFrame = (style & (WS_CAPTION | WS_THICKFRAME)) != 0;
    out.outer = toFitRect(outer);
    out.client = toFitRect(client);
    out.visibleFrame = toFitRect(frame);
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

    const HWND hwnd = reinterpret_cast<HWND>(m.hwnd);

    // A maximized window ignores SetWindowPos geometry; start from normal state.
    if (IsZoomed(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE);
        Sleep(80);
    }

    if (m.stripFrame) {
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
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
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.width(), r.height(),
                     SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        Sleep(120);
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
