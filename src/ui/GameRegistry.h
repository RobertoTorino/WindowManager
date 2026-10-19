#pragma once

#include <optional>

#include <QString>

#include "config/AppConfig.h"
#include "window/SteamLibrary.h"

class QWidget;

/**
 * Helpers for the saved-game list: path normalisation, friendly names,
 * building GameEntry records and locating per-game snapshot folders.
 */
namespace GameRegistry {

QString normalizePath(const QString& path);
QString sanitizeFriendlyName(const QString& text);
QString defaultFriendlyName(const QString& selectedPath);

// Modal dialog; returns nullopt when cancelled.
std::optional<QString> promptForFriendlyName(QWidget* parent,
                                             const QString& defaultName,
                                             const QString& initialName);

AppConfig::GameEntry buildEntry(const QString& selectedPath, const QString& friendlyName);

struct SteamSync
{
    int added{0};
    int removed{0};
};

// Brings the saved games in line with the Steam libraries: a new installed game is added (started through
// Steam from then on), a game added by hand gets its AppID, and a Steam game that was uninstalled is
// removed (its window profile is kept). Names and executables chosen by the user are left alone.
SteamSync syncSteamGames(const SteamLibrary::Scan& scan);

// <app dir>/snapshots[/<game id>]
QString snapshotDirFor(const QString& exePath);

} // namespace GameRegistry
