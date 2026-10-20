#pragma once

#include <QtGlobal>

#include <vector>

#include "FitCalculator.h"

/**
 * Geometry for the "downscaled view": the game window is made larger than the
 * monitor (so it renders more pixels), parked out of sight, and an overlay on the
 * monitor shows a scaled-down live copy of it.
 *
 * Pure maths, no Win32/Qt, unit-tested.
 */
namespace Supersample {

struct Size
{
    int w{0};
    int h{0};
};

struct Point
{
    int x{0};
    int y{0};
};

// Largest rectangle with the aspect ratio srcW:srcH that fits in an area of
// areaW x areaH, centred. Coordinates are relative to the area's top-left.
Fit::Rect letterbox(int srcW, int srcH, int areaW, int areaH);

// Render size for the game window so that, once letterboxed on a monitor of
// monW x monH, the picture is `factor` times denser than what is shown.
// (aspectW:aspectH) is the picture's shape.
Size renderSize(int aspectW, int aspectH, int monW, int monH, double factor);

// Where to park the game window while it is shown downscaled. Prefers hiding it
// directly behind the overlay (the monitor's own origin); if a window of
// w x h there would be visible on another monitor, parks it just beyond the right
// edge of the whole desktop instead.
Point parkingOrigin(const std::vector<Fit::Rect>& monitors, const Fit::Rect& target, int w, int h);

// Maps a point on the overlay (client pixels) to the game's client pixels, so mouse
// input can be forwarded. `dest` is the letterboxed picture rectangle on the overlay;
// the game's client area is gameW x gameH. Uses pixel centres, so a 2x picture maps
// overlay pixel n to game pixel 2n+1 (the middle of the block it represents).
// Outside the picture (the black bars) it returns false, unless `clamp` is set, in
// which case the nearest edge pixel is used (for a drag that leaves the picture).
bool mapPoint(const Fit::Rect& dest, int gameW, int gameH, int x, int y, bool clamp, Point& out);

// When to put the game window back after the game resized it. Some games reset their window to
// their own resolution every time they get the focus (Tekken 7): that is answered every time. A game
// that resizes again at once would start a loop, so at most kMax corrections within kWindowMs; after
// that the view follows the game until it has been quiet for kWindowMs.
class ReassertPolicy
{
public:
    static constexpr int    kMax      = 5;
    static constexpr qint64 kWindowMs = 10'000;

    bool allow(qint64 nowMs);   // true: put it back now (counted)
    bool gaveUp() const { return m_gaveUp; }

private:
    std::vector<qint64> m_times;   // corrections within the window
    bool m_gaveUp{false};
};

} // namespace Supersample
