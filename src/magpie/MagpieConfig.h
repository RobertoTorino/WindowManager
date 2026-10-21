#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

/**
 * Pure logic for Magpie (github.com/Blinue/Magpie, a window upscaler): reading and editing its
 * config.json so that Magpie scales a game by itself when the game's window is in front.
 * No processes, no Win32, so it is unit-tested directly (tst_magpie). See docs/MAGPIE.md.
 *
 * Magpie reads its config only when it starts and writes it back from memory, so the file must be
 * edited while Magpie is not running.
 */
namespace Magpie {

// Profiles this app writes are named "WindowManager: <game>"; Magpie's other profiles are left alone.
inline const QString kProfilePrefix = QStringLiteral("WindowManager: ");

// The config of a portable Magpie: config/config.json next to Magpie.exe (its existence makes Magpie portable).
inline const QString kPortableConfig = QStringLiteral("config/config.json");

// A game's Magpie choice (Magpie tab), saved with the game in windowmanager.json.
struct GameSettings
{
    QString mode;        // a scaling mode name ("FSR"); empty: Magpie is not used for this game
    bool    game3D{false};

    bool enabled() const { return !mode.isEmpty(); }
    bool operator==(const GameSettings&) const = default;
};
QJsonObject toJson(const GameSettings& settings);
GameSettings gameSettingsFromJson(const QJsonObject& obj);

// Magpie's "autoScale": when the matched window comes to the front.
enum class AutoScale { Off = 0, FullScreen = 1, Windowed = 2 };

struct GameProfile
{
    QString name;          // the game's name (shown in Magpie with kProfilePrefix)
    QString exePath;       // exactly as Windows reports it for the game window's process (backslashes)
    QString windowClass;   // the game window's class name, exactly
    QString scalingMode;   // a name from Magpie's scaling modes, e.g. "FSR"
    AutoScale autoScale{AutoScale::FullScreen};
    // Magpie's 3D game mode keeps the cursor inside the game every frame and never lets it go, also
    // while another window is in front, until Magpie quits. Off unless the user asks for it per game.
    bool game3D{false};
};

// The names of Magpie's scaling modes, in order (a profile refers to one by its index).
QStringList scalingModes(const QByteArray& config);

// The profiles this app wrote (those named with kProfilePrefix; name without the prefix).
QList<GameProfile> gameProfiles(const QByteArray& config);

struct EditResult
{
    bool       ok{false};
    QByteArray config;   // the new file content when ok
    QString    error;
};

// Adds the game's profile, or updates the one this app wrote for the same exe and window class (the
// fields above are set; anything else changed in Magpie's window is kept). A new profile starts from
// Magpie's default profile. Fails on a config Magpie has not filled yet (no scaling modes), an unknown
// scaling mode, missing fields, or when a profile the user made already covers this exe and window class.
EditResult setGameProfile(const QByteArray& config, const GameProfile& game);

// Removes the profiles this app wrote: for `exePath` only, or all of them when it is empty.
EditResult removeGameProfiles(const QByteArray& config, const QString& exePath = {});

} // namespace Magpie
