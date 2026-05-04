#include "MonitorHelper.h"

#include "../core/Logger.h"

#include <Windows.h>

#include <string>

// ---------------------------------------------------------------------------
// Internal enumeration helpers
// ---------------------------------------------------------------------------

struct EnumContext
{
    std::vector<MonitorGeometry> monitors;
};

static BOOL CALLBACK monitorEnumProc(HMONITOR hMon, HDC /*hdc*/,
                                      LPRECT /*clip*/, LPARAM data)
{
    auto* ctx = reinterpret_cast<EnumContext*>(data);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (::GetMonitorInfoW(hMon, &mi)) {
        MonitorGeometry mg;
        mg.index = static_cast<int>(ctx->monitors.size()) + 1;
        mg.x     = mi.rcMonitor.left;
        mg.y     = mi.rcMonitor.top;
        mg.w     = mi.rcMonitor.right  - mi.rcMonitor.left;
        mg.h     = mi.rcMonitor.bottom - mi.rcMonitor.top;
        ctx->monitors.push_back(mg);
    }
    return TRUE;
}

// Work-area variant (excludes taskbar)
struct WorkAreaEnumContext
{
    int targetIndex{0};
    int current{0};
    std::optional<MonitorGeometry> result;
};

static BOOL CALLBACK workAreaEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM data)
{
    auto* ctx = reinterpret_cast<WorkAreaEnumContext*>(data);
    ctx->current++;
    if (ctx->current == ctx->targetIndex) {
        MONITORINFO mi{};
        mi.cbSize = sizeof(mi);
        if (::GetMonitorInfoW(hMon, &mi)) {
            MonitorGeometry mg;
            mg.index = ctx->targetIndex;
            mg.x     = mi.rcWork.left;
            mg.y     = mi.rcWork.top;
            mg.w     = mi.rcWork.right  - mi.rcWork.left;
            mg.h     = mi.rcWork.bottom - mi.rcWork.top;
            ctx->result = mg;
        }
        return FALSE; // stop enumeration
    }
    return TRUE;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::vector<MonitorGeometry> MonitorHelper::enumerateMonitors()
{
    EnumContext ctx;
    ::EnumDisplayMonitors(nullptr, nullptr, monitorEnumProc,
                          reinterpret_cast<LPARAM>(&ctx));
    return ctx.monitors;
}

int MonitorHelper::getMonitorCount()
{
    return ::GetSystemMetrics(SM_CMONITORS);
}

std::optional<MonitorGeometry> MonitorHelper::getMonitorGeometry(int index)
{
    for (const auto& m : enumerateMonitors())
        if (m.index == index) return m;
    return std::nullopt;
}

std::optional<MonitorGeometry> MonitorHelper::getMonitorWorkArea(int index)
{
    WorkAreaEnumContext ctx{ index };
    ::EnumDisplayMonitors(nullptr, nullptr, workAreaEnumProc,
                          reinterpret_cast<LPARAM>(&ctx));
    return ctx.result;
}

std::optional<MonitorGeometry> MonitorHelper::getPrimaryMonitorGeometry()
{
    return getMonitorGeometry(1);
}

int MonitorHelper::getMonitorIndexFromPoint(int x, int y)
{
    for (const auto& m : enumerateMonitors())
        if (isPointOnMonitor(x, y, m.index)) return m.index;
    return 1;
}

int MonitorHelper::getMonitorIndexFromWindow(void* hwnd)
{
    RECT r{};
    if (!::GetWindowRect(static_cast<HWND>(hwnd), &r)) return 1;
    const int cx = r.left + (r.right  - r.left) / 2;
    const int cy = r.top  + (r.bottom - r.top)  / 2;
    return getMonitorIndexFromPoint(cx, cy);
}

bool MonitorHelper::isPointOnMonitor(int x, int y, int monitorIndex)
{
    const auto m = getMonitorGeometry(monitorIndex);
    if (!m) return false;
    return x >= m->x && x < m->x + m->w
        && y >= m->y && y < m->y + m->h;
}

std::optional<MonitorHelper::Point>
MonitorHelper::calculateMonitorCenterPoint(int monitorIndex)
{
    const auto m = getMonitorGeometry(monitorIndex);
    if (!m) return std::nullopt;
    return Point{ m->x + m->w / 2, m->y + m->h / 2 };
}

void MonitorHelper::logAllMonitors()
{
    const auto monitors = enumerateMonitors();
    Logger::instance().info("Total monitors: " + std::to_string(monitors.size()),
                            "MonitorHelper");
    for (const auto& m : monitors) {
        Logger::instance().info(
            "Monitor " + std::to_string(m.index) + ": " +
            std::to_string(m.w) + "x" + std::to_string(m.h) +
            " @ (" + std::to_string(m.x) + "," + std::to_string(m.y) + ")",
            "MonitorHelper");
    }
}
