#include "../window/WindowStabilizer.h"

#include "../core/Logger.h"
#include "PlatformBackend.h"

#include <QTimer>

#include <cstdlib>

// For systems without a "window moved" hook: look at the window a few times a second and put
// it back when a game has moved it.
struct WindowStabilizer::Impl
{
    QTimer timer;
    quintptr tracked{0};
    Fit::Rect desired;
    bool running{false};

    Impl()
    {
        timer.setInterval(100);
        QObject::connect(&timer, &QTimer::timeout, &timer, [this]() { check(); });
    }

    void check()
    {
        PlatformBackend& os = platform();
        if (!tracked || !os.isValid(tracked) || os.windowState(tracked).minimized)
            return;

        Fit::Rect curr;
        if (!os.windowRect(tracked, curr))
            return;

        if (std::abs(curr.left - desired.left) <= 10 && std::abs(curr.top - desired.top) <= 10
            && std::abs(curr.width() - desired.width()) <= 10
            && std::abs(curr.height() - desired.height()) <= 10)
            return;

        os.setBounds(tracked, desired);
    }
};

WindowStabilizer::WindowStabilizer()
    : m_impl(new Impl())
{
}

WindowStabilizer::~WindowStabilizer()
{
    stop();
    delete m_impl;
}

void WindowStabilizer::start()
{
    if (m_impl->running) {
        return;
    }
    m_impl->timer.start();
    m_impl->running = true;
    Logger::instance().info("Window stabilizer enabled", "WindowStabilizer");
}

void WindowStabilizer::stop()
{
    if (!m_impl->running) {
        return;
    }
    m_impl->timer.stop();
    m_impl->running = false;
    Logger::instance().info("Window stabilizer disabled", "WindowStabilizer");
}

bool WindowStabilizer::isRunning() const
{
    return m_impl->running;
}

void WindowStabilizer::trackWindow(quintptr hwnd, int x, int y, int w, int h)
{
    m_impl->tracked = hwnd;
    m_impl->desired = Fit::Rect::fromXYWH(x, y, w, h);
}

void WindowStabilizer::clearTrackedWindow()
{
    m_impl->tracked = 0;
    m_impl->desired = Fit::Rect{};
}
