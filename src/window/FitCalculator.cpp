#include "FitCalculator.h"

#include <algorithm>

namespace Fit {

Insets measureInsets(const Rect& outer, const Rect& client)
{
    Insets i;
    i.left   = std::max(0, client.left - outer.left);
    i.top    = std::max(0, client.top - outer.top);
    i.right  = std::max(0, outer.right - client.right);
    i.bottom = std::max(0, outer.bottom - client.bottom);
    return i;
}

Plan compute(const Input& in)
{
    Plan p;
    p.insets = in.stripFrame ? Insets{} : measureInsets(in.outer, in.client);

    p.desiredOuter = Rect{
        in.target.left   - p.insets.left,
        in.target.top    - p.insets.top,
        in.target.right  + p.insets.right,
        in.target.bottom + p.insets.bottom,
    };

    p.dx = p.desiredOuter.left - in.outer.left;
    p.dy = p.desiredOuter.top  - in.outer.top;
    p.dw = p.desiredOuter.width()  - in.outer.width();
    p.dh = p.desiredOuter.height() - in.outer.height();
    return p;
}

void Trigger::arm(std::int64_t nowMs, int timeoutMs, int settleMs)
{
    m_armed = true;
    m_armedAt = nowMs;
    m_timeoutMs = timeoutMs;
    m_settleMs = settleMs;
    m_candidate = 0;
    m_candidateRect = {};
    m_stableSince = nowMs;
}

void Trigger::disarm()
{
    m_armed = false;
    m_candidate = 0;
}

Trigger::Result Trigger::update(std::int64_t nowMs, std::uintptr_t hwnd, bool ready, const Rect& rect)
{
    if (!m_armed)
        return Result::Idle;

    if (nowMs - m_armedAt > m_timeoutMs) {
        disarm();
        return Result::TimedOut;
    }

    if (hwnd == 0 || !ready) {
        m_candidate = 0;
        return Result::Waiting;
    }

    // New window, or it moved/resized: restart the settle clock.
    if (hwnd != m_candidate || rect != m_candidateRect) {
        m_candidate = hwnd;
        m_candidateRect = rect;
        m_stableSince = nowMs;
        return Result::Waiting;
    }

    if (nowMs - m_stableSince >= m_settleMs) {
        disarm();
        return Result::Fire;
    }
    return Result::Waiting;
}

} // namespace Fit
