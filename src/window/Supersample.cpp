#include "Supersample.h"

#include <algorithm>
#include <cmath>

namespace Supersample {

namespace {

bool intersects(const Fit::Rect& a, const Fit::Rect& b)
{
    return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

} // namespace

Fit::Rect letterbox(int srcW, int srcH, int areaW, int areaH)
{
    if (srcW <= 0 || srcH <= 0 || areaW <= 0 || areaH <= 0)
        return Fit::Rect::fromXYWH(0, 0, std::max(areaW, 0), std::max(areaH, 0));

    // Compare srcW/srcH with areaW/areaH using integers to avoid rounding drift.
    const long long lhs = static_cast<long long>(srcW) * areaH;
    const long long rhs = static_cast<long long>(areaW) * srcH;

    int w = areaW;
    int h = areaH;
    if (lhs > rhs)        // picture is wider than the area: bars above and below
        h = static_cast<int>(std::llround(static_cast<double>(areaW) * srcH / srcW));
    else if (lhs < rhs)   // picture is taller: bars left and right
        w = static_cast<int>(std::llround(static_cast<double>(areaH) * srcW / srcH));

    w = std::clamp(w, 1, areaW);
    h = std::clamp(h, 1, areaH);
    return Fit::Rect::fromXYWH((areaW - w) / 2, (areaH - h) / 2, w, h);
}

Size renderSize(int aspectW, int aspectH, int monW, int monH, double factor)
{
    const Fit::Rect shown = letterbox(aspectW, aspectH, monW, monH);
    const double f = std::max(factor, 1.0);
    return {static_cast<int>(std::llround(shown.width() * f)),
            static_cast<int>(std::llround(shown.height() * f))};
}

Point parkingOrigin(const std::vector<Fit::Rect>& monitors, const Fit::Rect& target, int w, int h)
{
    const Fit::Rect behind = Fit::Rect::fromXYWH(target.left, target.top, w, h);

    int virtualRight = target.right;
    bool visibleElsewhere = false;
    for (const auto& m : monitors) {
        virtualRight = std::max(virtualRight, m.right);
        if (m != target && intersects(behind, m))
            visibleElsewhere = true;
    }

    if (!visibleElsewhere)
        return {target.left, target.top};
    return {virtualRight + 64, target.top};
}

bool mapPoint(const Fit::Rect& dest, int gameW, int gameH, int x, int y, bool clamp, Point& out)
{
    const int dw = dest.width();
    const int dh = dest.height();
    if (dw <= 0 || dh <= 0 || gameW <= 0 || gameH <= 0)
        return false;

    int dx = x - dest.left;
    int dy = y - dest.top;
    const bool inside = dx >= 0 && dy >= 0 && dx < dw && dy < dh;
    if (!inside) {
        if (!clamp)
            return false;
        dx = std::clamp(dx, 0, dw - 1);
        dy = std::clamp(dy, 0, dh - 1);
    }

    const long long gx = ((2LL * dx + 1) * gameW) / (2LL * dw);
    const long long gy = ((2LL * dy + 1) * gameH) / (2LL * dh);
    out.x = static_cast<int>(std::clamp<long long>(gx, 0, gameW - 1));
    out.y = static_cast<int>(std::clamp<long long>(gy, 0, gameH - 1));
    return true;
}

} // namespace Supersample
