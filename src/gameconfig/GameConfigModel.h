#pragma once

#include "core/IniText.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/**
 * Pure logic: a game's own config settings, written into its files before it starts (Game config tab).
 * First target: Unreal Engine 4 games that read loose config files (Tekken 7's arcade builds), and JConfig,
 * the arcade loader that puts Tekken 7 in a window (Binaries/Win64/monitor_misc.bin).
 *
 * The files are edited in place: only the keys below change, and a setting left at 0 puts the game's
 * original value back (taken from the copy GameConfigFiles keeps of each file).
 */
namespace GameConfig {

// What the app sets. 0 = leave the game's own value.
struct Settings
{
    bool enabled{false};    // written when the game is started from this app
    int  renderScale{0};    // percent of the resolution the 3D scene is drawn at (r.ScreenPercentage):
                            // above 100 supersamples, below 100 renders smaller and scales up
    int  width{0};          // the game's resolution (engine, user settings, r.setres, JConfig's DX resolution)
    int  height{0};
    bool jconfigWindow{false};   // also set JConfig's window (DX Window Mode) to the rectangle below
    int  windowX{0};
    int  windowY{0};
    int  windowWidth{0};
    int  windowHeight{0};

    bool isEmpty() const;   // nothing to change
    bool operator==(const Settings&) const = default;
};

constexpr int kMinScale = 25;
constexpr int kMaxScale = 400;
constexpr int kUpscaleQuality = 3;   // r.Upscale.Quality: 5-tap Catmull-Rom, the sharpest scaling filter

QJsonObject toJson(const Settings& s);
Settings settingsFromJson(const QJsonObject& o);
QString describe(const Settings& s);   // "render scale 150 %, 1920 x 1080, JConfig window 1920 x 1080 at 0,-1080"

// ----- Files ------------------------------------------------------------------------
// Where an Unreal Engine 4 game keeps its config, for an exe in <root>/<Project>/Binaries/Win64 (or Win32).
// The paths are what they would be; GameConfigFiles checks which exist.
struct Layout
{
    QString project;          // <root>/<Project>
    QString engineIni;        // <Project>/Config/DefaultEngine.ini
    QString userSettingsIni;  // <Project>/Config/DefaultGameUserSettings.ini
    QString platformIni;      // <root>/Engine/Config/Windows/WindowsEngine.ini (has r.setres in Tekken 7)
    QString jconfigFile;      // <Project>/Binaries/Win64/monitor_misc.bin

    bool valid() const { return !project.isEmpty(); }
};
Layout layoutFor(const QString& exePath);

// The exe whose folders hold the config: the game's own path, or for a .bat / .cmd game the exe the script
// started last time (AppConfig's childExePath).
QString exeForConfig(const QString& applicationPath, const QString& childExePath);

enum class IniFile { Engine, UserSettings, Platform };

// The keys this app manages in one file, with the value `s` wants; a setting left at 0 gives the value in
// `original` (the file before this app first changed it), or removes the key if the original had none.
QList<IniText::Entry> entriesFor(IniFile file, const Settings& s, const QString& original);

// Does `s` change anything in this file? (A file nothing was written to needs no copy.)
bool touches(IniFile file, const Settings& s);

// ----- JConfig ------------------------------------------------------------------------
// monitor_misc.bin is 61 little-endian int32 values. Known from the file and the game's window
// (2026-10-20): 60/64 window position, 68/72 window size, 76/80 DX resolution.
constexpr qsizetype kJConfigFileSize = 244;

struct JConfigWindow
{
    int x{0};
    int y{0};
    int width{0};
    int height{0};
    int dxWidth{0};
    int dxHeight{0};

    bool operator==(const JConfigWindow&) const = default;
};

std::optional<JConfigWindow> readJConfig(const QByteArray& data);   // nullopt: not the expected size
QByteArray writeJConfig(QByteArray data, const JConfigWindow& w);    // unchanged if not the expected size

// What JConfig should hold: `s` where it sets something, `original` otherwise.
JConfigWindow jconfigFor(const Settings& s, const JConfigWindow& original);
bool touchesJConfig(const Settings& s);

QString describe(const JConfigWindow& w);   // "window 1280 x 720 at 16,16, DX resolution 1920 x 1080"

} // namespace GameConfig
