#pragma once

#include <cstdint>

/**
 * Pure geometry for "make the picture fit the monitor exactly".
 *
 * A window has an outer rectangle (what SetWindowPos positions) and a client
 * rectangle (the actual picture). The gap between them - title bar, borders,
 * invisible resize borders - is the frame inset. To make the *client* cover a
 * target rectangle, the outer rectangle must be the target grown by the insets.
 * The difference between that and the current outer rectangle is the correction
 * the user would otherwise apply by hand with the move / width / height buttons.
 *
 * No Win32 or Qt dependencies, so it is unit-tested directly.
 */
namespace Fit {

struct Rect
{
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};

    int width() const { return right - left; }
    int height() const { return bottom - top; }
    bool operator==(const Rect& o) const
    {
        return left == o.left && top == o.top && right == o.right && bottom == o.bottom;
    }
    bool operator!=(const Rect& o) const { return !(*this == o); }

    static Rect fromXYWH(int x, int y, int w, int h) { return {x, y, x + w, y + h}; }
};

struct Insets
{
    int left{0};
    int top{0};
    int right{0};
    int bottom{0};
};

struct Input
{
    Rect outer;                 // window rectangle (screen coordinates)
    Rect client;                // client rectangle in screen coordinates
    Rect target;                // where the picture should end up
    bool stripFrame{false};     // frame will be removed first: insets become zero
};

struct Plan
{
    Insets insets;              // insets the plan was computed with
    Rect   desiredOuter;        // outer rectangle that puts the client on target
    int    dx{0};               // correction to the outer rectangle:
    int    dy{0};               //   move by (dx, dy)
    int    dw{0};               //   grow width by dw
    int    dh{0};               //   grow height by dh

    bool needsChange() const { return dx != 0 || dy != 0 || dw != 0 || dh != 0; }
};

Insets measureInsets(const Rect& outer, const Rect& client);
Plan   compute(const Input& in);

/**
 * Decides *when* to propose a fit after a game is launched: only once the
 * game's window exists, is usable, and has kept the same position and size for
 * `settleMs` (games and emulators often rebuild their window while their
 * graphics engine starts). Fires at most once per arm().
 */
class Trigger
{
public:
    enum class Result { Idle, Waiting, Fire, TimedOut };

    void arm(std::int64_t nowMs, int timeoutMs = 120000, int settleMs = 1800);
    void disarm();
    bool armed() const { return m_armed; }

    // hwnd == 0 or !ready means "no usable window right now".
    Result update(std::int64_t nowMs, std::uintptr_t hwnd, bool ready, const Rect& rect);

private:
    bool          m_armed{false};
    std::int64_t  m_armedAt{0};
    int           m_timeoutMs{0};
    int           m_settleMs{0};
    std::uintptr_t m_candidate{0};
    Rect          m_candidateRect;
    std::int64_t  m_stableSince{0};
};

} // namespace Fit
