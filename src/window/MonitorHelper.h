#pragma once

#include <optional>
#include <vector>

struct MonitorGeometry
{
    int index{0};
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

class MonitorHelper
{
public:
    MonitorHelper() = delete;

    struct Point { int x{0}; int y{0}; };

    static int  getMonitorCount();

    // 1-based index, full screen rect
    static std::optional<MonitorGeometry> getMonitorGeometry(int index);

    // 1-based index, work area (excludes taskbar)
    static std::optional<MonitorGeometry> getMonitorWorkArea(int index);

    static std::optional<MonitorGeometry> getPrimaryMonitorGeometry();

    static int  getMonitorIndexFromPoint(int x, int y);
    static int  getMonitorIndexFromWindow(void* hwnd);  // pass HWND

    static bool isPointOnMonitor(int x, int y, int monitorIndex);

    static std::optional<Point> calculateMonitorCenterPoint(int monitorIndex);

    // Logs all monitor info via Logger::instance()
    static void logAllMonitors();

    // Returns all monitors sorted by index
    static std::vector<MonitorGeometry> enumerateMonitors();
};
