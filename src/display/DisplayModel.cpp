#include "DisplayModel.h"

#include <algorithm>
#include <cmath>

namespace DisplayModel {

const std::vector<int> kScaleSteps = {100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500};

// ---------------------------------------------------------------------------
// Modes
// ---------------------------------------------------------------------------

bool isPortrait(Orientation o)
{
    return o == Orientation::Portrait || o == Orientation::PortraitFlipped;
}

Size oriented(Size native, Orientation o)
{
    return isPortrait(o) ? Size{native.height, native.width} : native;
}

std::vector<Mode> orientedModes(const Monitor& m)
{
    std::vector<Mode> out;
    out.reserve(m.modes.size());
    for (const Mode& mode : m.modes) {
        const Size s = oriented(mode.size(), m.orientation);
        out.push_back({s.width, s.height, mode.refresh});
    }
    return out;
}

std::vector<Size> sizes(const std::vector<Mode>& modes)
{
    std::vector<Size> out;
    for (const Mode& m : modes)
        if (std::find(out.begin(), out.end(), m.size()) == out.end())
            out.push_back(m.size());
    std::sort(out.begin(), out.end(), [](const Size& a, const Size& b) {
        const long long pa = 1LL * a.width * a.height;
        const long long pb = 1LL * b.width * b.height;
        return pa != pb ? pa > pb : a.width > b.width;
    });
    return out;
}

std::vector<int> refreshRates(const std::vector<Mode>& modes, Size size)
{
    std::vector<int> out;
    for (const Mode& m : modes)
        if (m.size() == size && m.refresh > 0 && std::find(out.begin(), out.end(), m.refresh) == out.end())
            out.push_back(m.refresh);
    std::sort(out.rbegin(), out.rend());
    return out;
}

std::optional<Mode> bestMode(const std::vector<Mode>& modes, Size size, int refresh, int preferredRefresh)
{
    const auto rates = refreshRates(modes, size);
    if (rates.empty())
        return std::nullopt;
    for (int wanted : {refresh, preferredRefresh})
        if (wanted > 0 && std::find(rates.begin(), rates.end(), wanted) != rates.end())
            return Mode{size.width, size.height, wanted};
    return Mode{size.width, size.height, rates.front()};
}

QString orientationName(Orientation o)
{
    switch (o) {
    case Orientation::Landscape:        return "Landscape";
    case Orientation::Portrait:         return "Portrait";
    case Orientation::LandscapeFlipped: return "Landscape (flipped)";
    case Orientation::PortraitFlipped:  return "Portrait (flipped)";
    }
    return {};
}

QString topologyName(Topology t)
{
    switch (t) {
    case Topology::Internal: return "PC screen only";
    case Topology::Clone:    return "Duplicate";
    case Topology::Extend:   return "Extend";
    case Topology::External: return "Second screen only";
    case Topology::Unknown:  break;
    }
    return "Unknown";
}

// ---------------------------------------------------------------------------
// Scale
// ---------------------------------------------------------------------------

ScaleInfo scaleFromRelative(int min, int cur, int max)
{
    ScaleInfo info;
    const int last = static_cast<int>(kScaleSteps.size()) - 1;
    const int recommended = -min;   // min counts the steps below the recommended one
    if (min > 0 || max < 0 || recommended > last || cur < min || cur > max)
        return info;
    info.valid = true;
    info.recommended = kScaleSteps[recommended];
    info.current = kScaleSteps[std::min(recommended + cur, last)];
    for (int i = 0; i <= std::min(recommended + max, last); ++i)
        info.options.push_back(kScaleSteps[i]);
    return info;
}

std::optional<int> relativeScale(const ScaleInfo& info, int percent)
{
    if (!info.valid || std::find(info.options.begin(), info.options.end(), percent) == info.options.end())
        return std::nullopt;
    const auto at = [](int p) {
        return static_cast<int>(std::find(kScaleSteps.begin(), kScaleSteps.end(), p) - kScaleSteps.begin());
    };
    return at(percent) - at(info.recommended);
}

// ---------------------------------------------------------------------------
// Brightness
// ---------------------------------------------------------------------------

int brightnessPercent(int value, int min, int max)
{
    if (max <= min)
        return 0;
    const double p = 100.0 * (std::clamp(value, min, max) - min) / (max - min);
    return static_cast<int>(std::lround(p));
}

int brightnessValue(int percent, int min, int max)
{
    if (max <= min)
        return min;
    const double v = min + (max - min) * std::clamp(percent, 0, 100) / 100.0;
    return static_cast<int>(std::lround(v));
}

bool wmiInstanceMatches(const QString& monitorId, const QString& instanceName)
{
    // \\?\DISPLAY#BOE0867#4&1a2b&0&UID8388688#{guid}  ->  DISPLAY\BOE0867\4&1a2b&0&UID8388688
    QString id = monitorId;
    if (id.startsWith("\\\\?\\"))
        id = id.mid(4);
    const int guid = id.indexOf("#{");
    if (guid >= 0)
        id = id.left(guid);
    id.replace('#', '\\');
    if (id.isEmpty())
        return false;
    // the instance adds "_0" (the monitor's index on that device)
    return instanceName.compare(id, Qt::CaseInsensitive) == 0
        || instanceName.startsWith(id + '_', Qt::CaseInsensitive);
}

// ---------------------------------------------------------------------------
// Per game
// ---------------------------------------------------------------------------

bool GameSettings::isEmpty() const
{
    return width <= 0 && refresh <= 0 && hdr < 0 && scale <= 0 && brightness < 0;
}

QJsonObject toJson(const GameSettings& s)
{
    QJsonObject o;
    o["Enabled"]    = s.enabled;
    o["MonitorId"]  = s.monitorId;
    o["Width"]      = s.width;
    o["Height"]     = s.height;
    o["Refresh"]    = s.refresh;
    o["Hdr"]        = s.hdr;
    o["Scale"]      = s.scale;
    o["Brightness"] = s.brightness;
    return o;
}

GameSettings gameSettingsFromJson(const QJsonObject& o)
{
    GameSettings s;
    s.enabled    = o["Enabled"].toBool(false);
    s.monitorId  = o["MonitorId"].toString();
    s.width      = std::max(0, o["Width"].toInt(0));
    s.height     = std::max(0, o["Height"].toInt(0));
    if (s.width == 0 || s.height == 0)
        s.width = s.height = 0;
    s.refresh    = std::max(0, o["Refresh"].toInt(0));
    s.hdr        = std::clamp(o["Hdr"].toInt(-1), -1, 1);
    s.scale      = std::max(0, o["Scale"].toInt(0));
    s.brightness = std::clamp(o["Brightness"].toInt(-1), -1, 100);
    return s;
}

QString describe(const GameSettings& s)
{
    QStringList parts;
    if (s.width > 0)
        parts << (s.refresh > 0 ? QString("%1 x %2 @ %3 Hz").arg(s.width).arg(s.height).arg(s.refresh)
                                : QString("%1 x %2").arg(s.width).arg(s.height));
    else if (s.refresh > 0)
        parts << QString("%1 Hz").arg(s.refresh);
    if (s.hdr >= 0)
        parts << (s.hdr ? "HDR on" : "HDR off");
    if (s.scale > 0)
        parts << QString("scale %1 %").arg(s.scale);
    if (s.brightness >= 0)
        parts << QString("brightness %1 %").arg(s.brightness);
    return parts.isEmpty() ? QString("no changes") : parts.join(", ");
}

int pickMonitor(const std::vector<Monitor>& monitors, const QString& id)
{
    for (size_t i = 0; i < monitors.size(); ++i)
        if (!id.isEmpty() && monitors[i].id.compare(id, Qt::CaseInsensitive) == 0)
            return static_cast<int>(i);
    for (size_t i = 0; i < monitors.size(); ++i)
        if (monitors[i].primary)
            return static_cast<int>(i);
    return monitors.empty() ? -1 : 0;
}

Changes plan(const Monitor& now, const GameSettings& want)
{
    Changes c;
    if (want.width > 0 || want.refresh > 0) {
        const Size size = want.width > 0 ? Size{want.width, want.height} : now.mode.size();
        const int refresh = want.refresh > 0 ? want.refresh : now.mode.refresh;
        const auto mode = bestMode(orientedModes(now), size, refresh, now.mode.refresh);
        if (mode && !(*mode == now.mode))
            c.mode = *mode;
    }
    if (want.hdr >= 0 && now.hdrSupported && now.hdrOn != (want.hdr == 1))
        c.hdr = want.hdr == 1;
    if (want.scale > 0 && now.scaleKnown && now.scale != want.scale)
        c.scale = want.scale;
    if (want.brightness >= 0 && now.brightnessSupported && now.brightness != want.brightness)
        c.brightness = want.brightness;
    return c;
}

Restore undoFor(const Monitor& now, const Changes& changes)
{
    Restore r;
    r.monitorId   = now.id;
    r.monitorName = now.name;
    r.mode        = changes.mode.has_value();
    if (changes.hdr)
        r.hdr = now.hdrOn;
    if (changes.scale)
        r.scale = now.scale;
    if (changes.brightness)
        r.brightness = now.brightness;
    return r;
}

void merge(Restore& into, const Restore& later)
{
    if (into.monitorId.isEmpty()) {
        into.monitorId   = later.monitorId;
        into.monitorName = later.monitorName;
    }
    into.mode = into.mode || later.mode;
    if (!into.hdr)
        into.hdr = later.hdr;
    if (!into.scale)
        into.scale = later.scale;
    if (!into.brightness)
        into.brightness = later.brightness;
}

QJsonObject toJson(const Restore& r)
{
    QJsonObject o;
    o["MonitorId"]   = r.monitorId;
    o["MonitorName"] = r.monitorName;
    o["Mode"]        = r.mode;
    if (r.hdr)
        o["Hdr"] = *r.hdr;
    if (r.scale)
        o["Scale"] = *r.scale;
    if (r.brightness)
        o["Brightness"] = *r.brightness;
    return o;
}

Restore restoreFromJson(const QJsonObject& o)
{
    Restore r;
    r.monitorId   = o["MonitorId"].toString();
    r.monitorName = o["MonitorName"].toString();
    r.mode        = o["Mode"].toBool(false);
    if (o.contains("Hdr"))
        r.hdr = o["Hdr"].toBool();
    if (o.contains("Scale") && o["Scale"].toInt() > 0)
        r.scale = o["Scale"].toInt();
    if (o.contains("Brightness"))
        r.brightness = std::clamp(o["Brightness"].toInt(), 0, 100);
    return r;
}

} // namespace DisplayModel
