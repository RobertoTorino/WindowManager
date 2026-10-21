#include "MagpieProcess.h"

#include "magpie/MagpieRelease.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

#include <iterator>

#ifdef Q_OS_WIN
#include <windows.h>
#include <tlhelp32.h>
#endif

namespace Magpie {

QList<RunningMagpie> runningMagpies()
{
    QList<RunningMagpie> out;
#ifdef Q_OS_WIN
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return out;
    PROCESSENTRY32W e{};
    e.dwSize = sizeof(e);
    for (BOOL ok = Process32FirstW(snap, &e); ok; ok = Process32NextW(snap, &e)) {
        if (_wcsicmp(e.szExeFile, L"Magpie.exe") != 0)
            continue;
        RunningMagpie m;
        m.pid = e.th32ProcessID;
        if (HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, e.th32ProcessID)) {
            wchar_t path[MAX_PATH * 2];
            DWORD size = static_cast<DWORD>(std::size(path));
            if (QueryFullProcessImageNameW(h, 0, path, &size))
                m.path = QDir::fromNativeSeparators(QString::fromWCharArray(path, static_cast<int>(size)));
            CloseHandle(h);
        }
        out << m;
    }
    CloseHandle(snap);
#endif
    return out;
}

bool isFrom(const RunningMagpie& m, const QString& dir)
{
    return !m.path.isEmpty()
           && QFileInfo(m.path).absoluteFilePath().compare(QFileInfo(exePath(dir)).absoluteFilePath(),
                                                          Qt::CaseInsensitive) == 0;
}

bool postQuit()
{
#ifdef Q_OS_WIN
    const UINT msg = RegisterWindowMessageW(L"WM_MAGPIE_QUIT");
    return msg != 0 && PostMessageW(HWND_BROADCAST, msg, 0, 0);
#else
    return false;
#endif
}

WindowFacts windowFacts(quintptr hwnd)
{
    WindowFacts f;
#ifdef Q_OS_WIN
    const HWND h = reinterpret_cast<HWND>(hwnd);
    if (!h || !IsWindow(h))
        return f;
    wchar_t cls[256];
    const int n = GetClassNameW(h, cls, static_cast<int>(std::size(cls)));
    f.className = QString::fromWCharArray(cls, n);
    DWORD pid = 0;
    GetWindowThreadProcessId(h, &pid);
    f.pid = pid;
    if (HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) {
        wchar_t path[MAX_PATH * 2];
        DWORD size = static_cast<DWORD>(std::size(path));
        if (QueryFullProcessImageNameW(p, 0, path, &size))
            f.exePath = QString::fromWCharArray(path, static_cast<int>(size));
        CloseHandle(p);
    }
    f.visible = IsWindowVisible(h);
    f.maximized = IsZoomed(h);
    RECT r{};
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetWindowRect(h, &r) && GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) {
        const bool framed = GetWindowLongW(h, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME);
        f.fullScreen = !framed && r.left <= mi.rcMonitor.left && r.top <= mi.rcMonitor.top
                       && r.right >= mi.rcMonitor.right && r.bottom >= mi.rcMonitor.bottom;
        f.monitorWidth = mi.rcMonitor.right - mi.rcMonitor.left;
        f.monitorHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
        MONITORINFOEXW mix{};
        mix.cbSize = sizeof(mix);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mix)
            && EnumDisplaySettingsW(mix.szDevice, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
            f.monitorHz = static_cast<int>(mode.dmDisplayFrequency);
    }
    RECT c{};
    if (GetClientRect(h, &c)) {
        f.clientWidth = c.right - c.left;
        f.clientHeight = c.bottom - c.top;
    }
#else
    Q_UNUSED(hwnd)
#endif
    return f;
}

bool processAlive(qint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (!p)
        return false;
    const bool alive = WaitForSingleObject(p, 0) == WAIT_TIMEOUT;
    CloseHandle(p);
    return alive;
#else
    Q_UNUSED(pid)
    return false;
#endif
}

bool startInTray(const QString& dir)
{
#ifdef Q_OS_WIN
    return QProcess::startDetached(exePath(dir), {"-t"}, dir);
#else
    Q_UNUSED(dir)
    return false;
#endif
}

Scaling scaling()
{
    Scaling s;
#ifdef Q_OS_WIN
    const HWND h = FindWindowW(L"Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22", nullptr);
    if (!h || !IsWindowVisible(h))
        return s;
    const auto prop = [h](const wchar_t* name) {
        return static_cast<int>(reinterpret_cast<INT_PTR>(GetPropW(h, name)));
    };
    s.source = reinterpret_cast<quintptr>(GetPropW(h, L"Magpie.SrcHWND"));
    s.srcLeft = prop(L"Magpie.SrcLeft");
    s.srcTop = prop(L"Magpie.SrcTop");
    s.destLeft = prop(L"Magpie.DestLeft");
    s.destTop = prop(L"Magpie.DestTop");
    s.srcWidth = prop(L"Magpie.SrcRight") - prop(L"Magpie.SrcLeft");
    s.srcHeight = prop(L"Magpie.SrcBottom") - prop(L"Magpie.SrcTop");
    s.destWidth = prop(L"Magpie.DestRight") - prop(L"Magpie.DestLeft");
    s.destHeight = prop(L"Magpie.DestBottom") - prop(L"Magpie.DestTop");
    s.windowed = GetPropW(h, L"Magpie.Windowed") != nullptr;
    s.active = s.source != 0 && s.srcWidth > 0 && s.srcHeight > 0 && s.destWidth > 0 && s.destHeight > 0;
#endif
    return s;
}

} // namespace Magpie
