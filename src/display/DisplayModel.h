#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

/**
 * Pure logic behind the Display tab: no OS calls, no widgets (tst_display).
 *
 * DisplayBackend fills in Monitor; this unit decides which modes to offer, what a
 * scale step means, how brightness maps to percent, and which changes a game's
 * display settings need and how to undo them.
 */
namespace DisplayModel {

// Same order as the Windows DMDO_* values.
enum class Orientation { Landscape = 0, Portrait = 1, LandscapeFlipped = 2, PortraitFlipped = 3 };

// How several displays are used (Win+P).
enum class Topology { Unknown, Internal, Clone, Extend, External };

struct Size
{
    int width{0};
    int height{0};
    bool operator==(const Size&) const = default;
};

struct Mode
{
    int width{0};
    int height{0};
    int refresh{0};   // Hz
    bool operator==(const Mode&) const = default;
    Size size() const { return {width, height}; }
};

struct Monitor
{
    QString id;           // stable across restarts (the monitor's device path)
    QString deviceName;   // \\.\DISPLAY1; changes when monitors are added or removed
    QString name;         // "DELL U2720Q"
    bool    primary{false};
    bool    internal{false};   // laptop panel

    Mode              mode;          // current, as shown (portrait: taller than wide)
    Orientation       orientation{Orientation::Landscape};
    std::vector<Mode> modes;         // available, in landscape (native) orientation

    bool hdrSupported{false};
    bool hdrOn{false};

    bool             scaleKnown{false};
    int              scale{100};          // percent
    int              recommendedScale{100};
    std::vector<int> scaleOptions;        // percent, ascending

    bool brightnessSupported{false};
    int  brightness{0};   // percent

    QStringList colorProfiles;   // file names associated with the display
    QString     colorProfile;    // the default one (empty: none)
};

// ----- Modes ------------------------------------------------------------------
bool  isPortrait(Orientation o);
Size  oriented(Size native, Orientation o);   // swaps width and height for portrait
// The monitor's modes as shown in its current orientation.
std::vector<Mode> orientedModes(const Monitor& m);
// Distinct sizes, largest first.
std::vector<Size> sizes(const std::vector<Mode>& modes);
// Refresh rates for one size, highest first.
std::vector<int>  refreshRates(const std::vector<Mode>& modes, Size size);
// The mode with this size and refresh rate. refresh 0 (or one the size lacks): the preferred
// rate if the size has it, else the highest.
std::optional<Mode> bestMode(const std::vector<Mode>& modes, Size size, int refresh, int preferredRefresh);
QString orientationName(Orientation o);
QString topologyName(Topology t);

// ----- Scale (Windows works in steps relative to the recommended one) ------------
extern const std::vector<int> kScaleSteps;   // 100, 125, ... 500
struct ScaleInfo
{
    bool             valid{false};
    int              current{100};
    int              recommended{100};
    std::vector<int> options;
};
// min / cur / max are steps relative to the recommended scale (min <= 0 <= max).
ScaleInfo scaleFromRelative(int min, int cur, int max);
// The relative step for a percentage, or nothing if it is not one of the options.
std::optional<int> relativeScale(const ScaleInfo& info, int percent);

// ----- Brightness ---------------------------------------------------------------
int brightnessPercent(int value, int min, int max);
int brightnessValue(int percent, int min, int max);

// The WMI instance for a monitor: "DISPLAY\BOE0867\4&1a2b&0&UID8388688_0" belongs to
// "\\?\DISPLAY#BOE0867#4&1a2b&0&UID8388688#{e6f07b5f-...}".
bool wmiInstanceMatches(const QString& monitorId, const QString& instanceName);

// ----- Per game -------------------------------------------------------------------
// Settings a game wants while it runs. 0 / -1 = leave as it is.
struct GameSettings
{
    bool    enabled{false};
    QString monitorId;      // empty: the primary monitor
    int     width{0};
    int     height{0};
    int     refresh{0};
    int     hdr{-1};        // -1 keep, 0 off, 1 on
    int     scale{0};       // percent
    int     brightness{-1}; // percent

    bool isEmpty() const;
    bool operator==(const GameSettings&) const = default;
};
QJsonObject toJson(const GameSettings& s);
GameSettings gameSettingsFromJson(const QJsonObject& o);
QString describe(const GameSettings& s);   // "1280 x 720 @ 60 Hz, HDR on"

// The monitor a game's settings are for: its id, else the primary, else the first. -1 if none.
int pickMonitor(const std::vector<Monitor>& monitors, const QString& id);

// What has to change on a monitor; empty fields stay as they are.
struct Changes
{
    std::optional<Mode> mode;
    std::optional<bool> hdr;
    std::optional<int>  scale;
    std::optional<int>  brightness;
    bool isEmpty() const { return !mode && !hdr && !scale && !brightness; }
};
// Only what differs from now and the monitor supports. The mode is planned in the current
// orientation; a scale that the monitor does not offer (yet) is still passed on, because
// the options change with the resolution.
Changes plan(const Monitor& now, const GameSettings& want);

// How to undo a game's changes. Written to disk while a game runs, so a crash is undone
// at the next start.
struct Restore
{
    QString             monitorId;
    QString             monitorName;
    bool                mode{false};   // the mode was changed: go back to the saved desktop mode
    std::optional<bool> hdr;
    std::optional<int>  scale;
    std::optional<int>  brightness;
    bool isEmpty() const { return !mode && !hdr && !scale && !brightness; }
};
// What `changes` will alter on `now`, with the values to go back to.
Restore undoFor(const Monitor& now, const Changes& changes);
// Adds a later change (only the first value of each setting is the desktop's).
void merge(Restore& into, const Restore& later);
QJsonObject toJson(const Restore& r);
Restore restoreFromJson(const QJsonObject& o);

} // namespace DisplayModel
