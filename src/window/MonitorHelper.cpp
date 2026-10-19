#include "MonitorHelper.h"

#include "../core/Logger.h"
#include "../platform/PlatformBackend.h"

#include <string>

namespace {

MonitorGeometry toGeometry(int index, const Fit::Rect& r)
{
    return MonitorGeometry{index, r.left, r.top, r.width(), r.height()};
}

} // namespace

std::vector<MonitorGeometry> MonitorHelper::enumerateMonitors()
{
    std::vector<MonitorGeometry> out;
    for (const auto& m : platform().monitors())
        out.push_back(toGeometry(static_cast<int>(out.size()) + 1, m.full));
    return out;
}

int MonitorHelper::getMonitorCount()
{
    return static_cast<int>(platform().monitors().size());
}

std::optional<MonitorGeometry> MonitorHelper::getMonitorGeometry(int index)
{
    for (const auto& m : enumerateMonitors())
        if (m.index == index) return m;
    return std::nullopt;
}

std::optional<MonitorGeometry> MonitorHelper::getMonitorWorkArea(int index)
{
    const auto monitors = platform().monitors();
    if (index < 1 || index > static_cast<int>(monitors.size()))
        return std::nullopt;
    return toGeometry(index, monitors[static_cast<size_t>(index) - 1].work);
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

int MonitorHelper::getMonitorIndexFromWindow(quintptr window)
{
    Fit::Rect r;
    if (!platform().windowRect(window, r)) return 1;
    const int cx = r.left + r.width() / 2;
    const int cy = r.top  + r.height() / 2;
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
