#include "WindowManager.h"

#include "../config/AppConfig.h"
#include "../core/Logger.h"
#include "MonitorHelper.h"

#include <Windows.h>
#include <tlhelp32.h>

#include <QFileInfo>
#include <QProcess>

#include <algorithm>
#include <string>

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

struct ScanContext
{
    QString targetPathLower;
    QString targetExeLower;
    std::vector<WindowManager::WindowInfo> windows;
};

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

    // Skip processes on the ignore list
    if (ignoreListInternal().contains(imageExeLower, Qt::CaseInsensitive)) {
        return TRUE;
    }
    // Skip TeknoLoaders (launcher/loader processes that are not the game itself)
    if (teknoLoaderListInternal().contains(imageExeLower, Qt::CaseInsensitive)) {
        return TRUE;
    }

    if (imageLower != ctx->targetPathLower && imageExeLower != ctx->targetExeLower) {
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
        return false;
    }

    ScanContext ctx;
    ctx.targetPathLower = m_targetExePath.toLower();
    ctx.targetExeLower = QFileInfo(m_targetExePath).fileName().toLower();

    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&ctx));

    m_scanned = std::move(ctx.windows);

    std::sort(m_scanned.begin(), m_scanned.end(), [](const WindowInfo& a, const WindowInfo& b) {
        return (a.w * a.h) > (b.w * b.h);
    });

    if (!m_scanned.empty()) {
        m_activeWindow = m_scanned.front().hwnd;
        m_lastMessage = QString("Found %1 window(s) for target executable.").arg(m_scanned.size());
        return true;
    }

    m_lastMessage = "No matching top-level window was found.";
    return false;
}

bool WindowManager::ensureActiveWindow()
{
    if (m_activeWindow && IsWindow(reinterpret_cast<HWND>(m_activeWindow))) {
        return true;
    }

    if (scanWindowsForTarget() && m_activeWindow) {
        return true;
    }

    m_lastMessage = "No active window available.";
    return false;
}

bool WindowManager::applyRect(quintptr hwndRaw, int x, int y, int w, int h, bool borderless)
{
    const HWND hwnd = reinterpret_cast<HWND>(hwndRaw);
    if (!IsWindow(hwnd)) {
        m_lastMessage = "Target window is no longer valid.";
        return false;
    }

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

    const auto mon = MonitorHelper::getMonitorWorkArea(monitorIndex);
    if (!mon) {
        m_lastMessage = QString("Monitor %1 is not available.").arg(monitorIndex);
        return false;
    }

    RECT rect{};
    if (!GetWindowRect(reinterpret_cast<HWND>(m_activeWindow), &rect)) {
        m_lastMessage = "Failed to read current window rectangle.";
        return false;
    }

    const int w = rect.right - rect.left;
    const int h = rect.bottom - rect.top;
    const int x = mon->x + (mon->w - w) / 2;
    const int y = mon->y + (mon->h - h) / 2;

    return applyRect(m_activeWindow, x, y, w, h, false);
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
        x -= dw;
        y -= dh;
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

bool WindowManager::launchTarget()
{
    if (m_targetExePath.trimmed().isEmpty()) {
        m_lastMessage = "No executable selected.";
        return false;
    }
    const bool ok = QProcess::startDetached(m_targetExePath);
    m_lastMessage = ok ? "Executable launched." : "Failed to launch executable.";
    return ok;
}

bool WindowManager::killTarget()
{
    int count = 0;

    // Kill all known loader/companion processes (safety: skip ignored system apps)
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                const QString name = QString::fromWCharArray(entry.szExeFile).toLower();
                if (ignoreListInternal().contains(name, Qt::CaseInsensitive)) continue;
                if (teknoLoaderListInternal().contains(name, Qt::CaseInsensitive)) {
                    HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, entry.th32ProcessID);
                    if (proc) {
                        TerminateProcess(proc, 1);
                        CloseHandle(proc);
                        ++count;
                    }
                }
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
    }

    // Kill the active game PID if it is not a system process
    if (m_activeWindow && IsWindow(reinterpret_cast<HWND>(m_activeWindow))) {
        DWORD pid = 0;
        GetWindowThreadProcessId(reinterpret_cast<HWND>(m_activeWindow), &pid);
        if (pid > 0) {
            const QString exeName = QFileInfo(processImagePath(static_cast<qint64>(pid))).fileName().toLower();
            if (!ignoreListInternal().contains(exeName, Qt::CaseInsensitive)) {
                HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                if (proc) {
                    TerminateProcess(proc, 1);
                    CloseHandle(proc);
                    ++count;
                }
            }
        }
        m_activeWindow = 0;
    }

    m_scanned.clear();
    m_lastMessage = QString("Killed %1 process(es).").arg(count);
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
