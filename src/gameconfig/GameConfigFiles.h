#pragma once

#include "GameConfigModel.h"

#include <QString>

/**
 * Writes a game's config settings (GameConfig::Settings) into its files, and puts them back.
 * Before a file is first changed, a copy is kept next to it as <file>.wm-original; the keys this app
 * manages get their original values from that copy, everything else in the file stays as it is.
 */
namespace GameConfig {

constexpr const char* kOriginalSuffix = ".wm-original";

struct Status
{
    Layout  layout;
    bool    engineIni{false};      // the files that exist
    bool    userSettingsIni{false};
    bool    platformIni{false};
    bool    jconfig{false};        // monitor_misc.bin of the expected size
    bool    changed{false};        // a copy of an original exists: this app has changed the files
    QString summary;               // what the files hold now, e.g. "render scale 100 % (not set), ..."

    bool supported() const { return engineIni; }
};

Status status(const QString& exePath);

// Writes `s` into the game's files. False (with a reason in `message`) if a file could not be written.
bool apply(const QString& exePath, const Settings& s, QString* message);

// Puts the managed keys back to the originals and removes the copies.
bool restore(const QString& exePath, QString* message);

} // namespace GameConfig
