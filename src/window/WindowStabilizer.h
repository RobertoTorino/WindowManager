#pragma once

#include <QtGlobal>

class WindowStabilizer
{
public:
    WindowStabilizer();
    ~WindowStabilizer();

    void start();
    void stop();
    bool isRunning() const;

    void trackWindow(quintptr hwnd, int x, int y, int w, int h);
    void clearTrackedWindow();

private:
    struct Impl;
    Impl* m_impl;
};
