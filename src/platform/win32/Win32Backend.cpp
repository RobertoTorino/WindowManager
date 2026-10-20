#include "../PlatformBackend.h"

#include <Windows.h>
#include <dwmapi.h>
#include <tlhelp32.h>

#include <QImage>
#include <QProcess>
#include <QFileInfo>

#include <string>

namespace {

Fit::Rect toFitRect(const RECT& r)
{
    return Fit::Rect{r.left, r.top, r.right, r.bottom};
}

uint64_t filetimeToU64(const FILETIME& ft)
{
    return (static_cast<uint64_t>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

HWND toHwnd(quintptr window)
{
    return reinterpret_cast<HWND>(window);
}

void applyFrameChange(HWND hwnd)
{
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM lParam)
{
    auto* out = reinterpret_cast<std::vector<PlatformBackend::TopLevelWindow>*>(lParam);

    RECT rect{};
    if (!GetWindowRect(hwnd, &rect))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0)
        return TRUE;

    PlatformBackend::TopLevelWindow w;
    w.handle = reinterpret_cast<quintptr>(hwnd);
    w.pid = static_cast<qint64>(pid);
    w.rect = toFitRect(rect);
    out->push_back(w);
    return TRUE;
}

struct MonitorEnum
{
    std::vector<PlatformBackend::Monitor> monitors;
};

BOOL CALLBACK collectMonitor(HMONITOR hMon, HDC, LPRECT, LPARAM data)
{
    auto* ctx = reinterpret_cast<MonitorEnum*>(data);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(hMon, &mi))
        ctx->monitors.push_back({toFitRect(mi.rcMonitor), toFitRect(mi.rcWork)});
    return TRUE;
}

class Win32Backend final : public PlatformBackend
{
public:
    // ----- Windows ---------------------------------------------------------
    std::vector<TopLevelWindow> topLevelWindows() const override
    {
        std::vector<TopLevelWindow> out;
        EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&out));
        return out;
    }

    bool isValid(quintptr window) const override
    {
        return window && IsWindow(toHwnd(window));
    }

    WindowState windowState(quintptr window) const override
    {
        WindowState s;
        if (!isValid(window))
            return s;
        const HWND h = toHwnd(window);
        s.valid = true;
        s.visible = IsWindowVisible(h) != 0;
        s.minimized = IsIconic(h) != 0;
        s.maximized = IsZoomed(h) != 0;

        const LONG_PTR style = GetWindowLongPtr(h, GWL_STYLE);
        s.hasCaption = (style & WS_CAPTION) != 0;
        s.hasFrame = (style & (WS_CAPTION | WS_THICKFRAME)) != 0;

        const LONG_PTR ex = GetWindowLongPtr(h, GWL_EXSTYLE);
        s.topmost = (ex & WS_EX_TOPMOST) != 0;
        s.toolWindow = (ex & WS_EX_TOOLWINDOW) != 0;
        s.layered = (ex & WS_EX_LAYERED) != 0;
        s.noActivate = (ex & WS_EX_NOACTIVATE) != 0;
        return s;
    }

    QString windowTitle(quintptr window) const override
    {
        wchar_t title[1024]{};
        if (GetWindowTextW(toHwnd(window), title, static_cast<int>(std::size(title))) <= 0)
            return {};
        return QString::fromWCharArray(title);
    }

    QString windowClass(quintptr window) const override
    {
        wchar_t cls[256]{};
        if (GetClassNameW(toHwnd(window), cls, static_cast<int>(std::size(cls))) <= 0)
            return {};
        return QString::fromWCharArray(cls);
    }

    qint64 windowPid(quintptr window) const override
    {
        DWORD pid = 0;
        GetWindowThreadProcessId(toHwnd(window), &pid);
        return static_cast<qint64>(pid);
    }

    bool windowRect(quintptr window, Fit::Rect& out) const override
    {
        RECT r{};
        if (!GetWindowRect(toHwnd(window), &r))
            return false;
        out = toFitRect(r);
        return true;
    }

    bool clientRect(quintptr window, Fit::Rect& out) const override
    {
        const HWND hwnd = toHwnd(window);
        RECT local{};
        if (!GetClientRect(hwnd, &local))
            return false;
        POINT origin{0, 0};
        ClientToScreen(hwnd, &origin);
        out = Fit::Rect::fromXYWH(origin.x, origin.y,
                                  local.right - local.left, local.bottom - local.top);
        return true;
    }

    std::optional<Fit::Rect> visibleFrame(quintptr window) const override
    {
        RECT frame{};
        if (FAILED(DwmGetWindowAttribute(toHwnd(window), DWMWA_EXTENDED_FRAME_BOUNDS,
                                         &frame, sizeof(frame))))
            return std::nullopt;
        return toFitRect(frame);
    }

    bool setBounds(quintptr window, const Fit::Rect& r) override
    {
        return SetWindowPos(toHwnd(window), HWND_TOP, r.left, r.top, r.width(), r.height(),
                            SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE;
    }

    void stripFrame(quintptr window) override
    {
        const HWND hwnd = toHwnd(window);
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style &= ~(WS_CAPTION | WS_THICKFRAME);
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle &= ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
    }

    void restoreFrame(quintptr window) override
    {
        const HWND hwnd = toHwnd(window);
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style |= WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_WINDOWEDGE;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

        applyFrameChange(hwnd);
    }

    void resetWindow(quintptr window) override
    {
        const HWND hwnd = toHwnd(window);

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
        applyFrameChange(hwnd);
    }

    void show(quintptr window, ShowState state) override
    {
        int cmd = SW_SHOW;
        switch (state) {
        case ShowState::Hide:     cmd = SW_HIDE; break;
        case ShowState::Show:     cmd = SW_SHOW; break;
        case ShowState::Minimize: cmd = SW_MINIMIZE; break;
        case ShowState::Maximize: cmd = SW_MAXIMIZE; break;
        case ShowState::Restore:  cmd = SW_RESTORE; break;
        }
        ShowWindow(toHwnd(window), cmd);
    }

    void bringToTop(quintptr window) override
    {
        BringWindowToTop(toHwnd(window));
    }

    void focus(quintptr window) override
    {
        const HWND hwnd = toHwnd(window);
        // AllowSetForegroundWindow bypasses the foreground lock so we can steal focus
        AllowSetForegroundWindow(ASFW_ANY);
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        BringWindowToTop(hwnd);
    }

    quintptr foregroundWindow() const override
    {
        return reinterpret_cast<quintptr>(GetForegroundWindow());
    }

    void requestClose(quintptr window) override
    {
        PostMessage(toHwnd(window), WM_CLOSE, 0, 0);
    }

    bool supportsFlag(WindowFlag) const override { return true; }

    bool toggleFlag(quintptr window, WindowFlag flag) override
    {
        const HWND hwnd = toHwnd(window);

        if (flag == WindowFlag::Topmost) {
            const bool wasTopmost = (GetWindowLongPtr(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
            SetWindowPos(hwnd, wasTopmost ? HWND_NOTOPMOST : HWND_TOPMOST,
                0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            return !wasTopmost;
        }

        LONG_PTR bit = WS_EX_TOOLWINDOW;
        if (flag == WindowFlag::Layered)
            bit = WS_EX_LAYERED;
        else if (flag == WindowFlag::NoActivate)
            bit = WS_EX_NOACTIVATE;

        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        const bool nowSet = (exStyle & bit) == 0;
        if (nowSet) {
            exStyle |= bit;
            if (flag == WindowFlag::Layered)
                SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
        } else {
            exStyle &= ~bit;
        }
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        applyFrameChange(hwnd);
        return nowSet;
    }

    // ----- Monitors --------------------------------------------------------
    std::vector<Monitor> monitors() const override
    {
        MonitorEnum ctx;
        EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&ctx));
        return ctx.monitors;
    }

    std::optional<Monitor> nearestMonitor(quintptr window) const override
    {
        const HMONITOR hmon = MonitorFromWindow(toHwnd(window), MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (!GetMonitorInfoW(hmon, &mi))
            return std::nullopt;
        return Monitor{toFitRect(mi.rcMonitor), toFitRect(mi.rcWork)};
    }

    // ----- Processes -------------------------------------------------------
    std::vector<ProcessEntry> processes() const override
    {
        std::vector<ProcessEntry> out;
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
            return out;

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Process32FirstW(snapshot, &entry)) {
            do {
                out.push_back({entry.th32ProcessID, entry.th32ParentProcessID,
                               QString::fromWCharArray(entry.szExeFile)});
            } while (Process32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return out;
    }

    QString processImagePath(qint64 pid) const override
    {
        const HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
        if (!proc)
            return {};

        std::wstring buf;
        buf.resize(2048);
        DWORD len = static_cast<DWORD>(buf.size());

        const BOOL ok = QueryFullProcessImageNameW(proc, 0, buf.data(), &len);
        CloseHandle(proc);

        if (!ok || len == 0)
            return {};

        buf.resize(len);
        return QString::fromStdWString(buf);
    }

    uint64_t processCreationTime(uint32_t pid) const override
    {
        const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!h)
            return 0;
        FILETIME created{}, exited{}, kernel{}, user{};
        const BOOL ok = GetProcessTimes(h, &created, &exited, &kernel, &user);
        CloseHandle(h);
        return ok ? filetimeToU64(created) : 0;
    }

    uint64_t now() const override
    {
        FILETIME ft{};
        GetSystemTimeAsFileTime(&ft);
        return filetimeToU64(ft);
    }

    uint32_t currentPid() const override
    {
        return GetCurrentProcessId();
    }

    bool terminateProcess(uint32_t pid) override
    {
        const HANDLE proc = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (!proc)
            return false;
        const bool ok = TerminateProcess(proc, 1) != FALSE;
        CloseHandle(proc);
        return ok;
    }

    bool startProcess(const QString& path, const QString& workingDir, qint64* pid) override
    {
        const QString suffix = QFileInfo(path).suffix();
        const bool isBatch = suffix.compare("bat", Qt::CaseInsensitive) == 0
                          || suffix.compare("cmd", Qt::CaseInsensitive) == 0;
        if (isBatch)
            return QProcess::startDetached("cmd.exe", {"/c", path}, workingDir, pid);
        return QProcess::startDetached(path, {}, workingDir, pid);
    }

    // ----- Capture ---------------------------------------------------------
    QImage captureScreen(const Fit::Rect& rect) const override
    {
        const int w = rect.width();
        const int h = rect.height();
        if (w <= 0 || h <= 0)
            return {};

        // GDI BitBlt of the screen rectangle
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem    = CreateCompatibleDC(hdcScreen);
        HBITMAP hBmp  = CreateCompatibleBitmap(hdcScreen, w, h);
        HGDIOBJ hOld  = SelectObject(hdcMem, hBmp);
        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, rect.left, rect.top, SRCCOPY);
        SelectObject(hdcMem, hOld);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);

        const QImage image = QImage::fromHBITMAP(hBmp);
        DeleteObject(hBmp);
        return image;
    }
};

} // namespace

PlatformBackend& nativePlatformBackend()
{
    static Win32Backend backend;
    return backend;
}
