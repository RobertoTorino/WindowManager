#include "WindowStabilizer.h"

#include "../core/Logger.h"

#include <Windows.h>

#include <cmath>

struct WindowStabilizer::Impl
{
    HWINEVENTHOOK hook{nullptr};
    HWND trackedHwnd{nullptr};
    RECT desiredRect{0, 0, 0, 0};
    bool running{false};
    bool reapplying{false};

    static Impl* self;

    static void CALLBACK WinEventProc(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG, LONG, DWORD, DWORD)
    {
        if (!self || !self->running || self->reapplying) {
            return;
        }

        if (event != EVENT_OBJECT_LOCATIONCHANGE) {
            return;
        }

        if (!self->trackedHwnd || hwnd != self->trackedHwnd || !IsWindow(hwnd) || IsIconic(hwnd)) {
            return;
        }

        RECT curr{};
        if (!GetWindowRect(hwnd, &curr)) {
            return;
        }

        const int dx = std::abs(curr.left - self->desiredRect.left);
        const int dy = std::abs(curr.top - self->desiredRect.top);
        const int dw = std::abs((curr.right - curr.left) - (self->desiredRect.right - self->desiredRect.left));
        const int dh = std::abs((curr.bottom - curr.top) - (self->desiredRect.bottom - self->desiredRect.top));

        if (dx <= 10 && dy <= 10 && dw <= 10 && dh <= 10) {
            return;
        }

        self->reapplying = true;
        SetWindowPos(
            hwnd,
            HWND_TOP,
            self->desiredRect.left,
            self->desiredRect.top,
            self->desiredRect.right - self->desiredRect.left,
            self->desiredRect.bottom - self->desiredRect.top,
            SWP_NOOWNERZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
        );
        self->reapplying = false;
    }
};

WindowStabilizer::Impl* WindowStabilizer::Impl::self = nullptr;

WindowStabilizer::WindowStabilizer()
    : m_impl(new Impl())
{
    Impl::self = m_impl;
}

WindowStabilizer::~WindowStabilizer()
{
    stop();
    if (Impl::self == m_impl) {
        Impl::self = nullptr;
    }
    delete m_impl;
}

void WindowStabilizer::start()
{
    if (m_impl->running) {
        return;
    }

    m_impl->hook = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE,
        EVENT_OBJECT_LOCATIONCHANGE,
        nullptr,
        &Impl::WinEventProc,
        0,
        0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    if (m_impl->hook) {
        m_impl->running = true;
        Logger::instance().info("Window stabilizer enabled", "WindowStabilizer");
    } else {
        Logger::instance().warn("Failed to enable window stabilizer", "WindowStabilizer");
    }
}

void WindowStabilizer::stop()
{
    if (!m_impl->running) {
        return;
    }

    if (m_impl->hook) {
        UnhookWinEvent(m_impl->hook);
        m_impl->hook = nullptr;
    }
    m_impl->running = false;
    Logger::instance().info("Window stabilizer disabled", "WindowStabilizer");
}

bool WindowStabilizer::isRunning() const
{
    return m_impl->running;
}

void WindowStabilizer::trackWindow(quintptr hwnd, int x, int y, int w, int h)
{
    m_impl->trackedHwnd = reinterpret_cast<HWND>(hwnd);
    m_impl->desiredRect.left = x;
    m_impl->desiredRect.top = y;
    m_impl->desiredRect.right = x + w;
    m_impl->desiredRect.bottom = y + h;
}

void WindowStabilizer::clearTrackedWindow()
{
    m_impl->trackedHwnd = nullptr;
    m_impl->desiredRect = RECT{0, 0, 0, 0};
}
