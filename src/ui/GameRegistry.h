#pragma once

#include <optional>

#include <QString>

#include "config/AppConfig.h"

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

// <app dir>/snapshots[/<game id>]
QString snapshotDirFor(const QString& exePath);

} // namespace GameRegistry
