#pragma once

#include <optional>

#include <QString>
#include <QStringList>

#include "HelixRelease.h"

namespace Helix {

/** Where HelixSR lives on disk, and what can be thrown away once it is built. */

// <app dir>/helixsr: one folder per built version (HelixSR-<version>) plus the download.
QString dataDir();

// %LOCALAPPDATA%\HelixSR: where the HelixSR setup keeps its Python, numpy and shader compiler.
QString setupDataDir();

// Python of the HelixSR setup (stdlib is enough for installing into games).
QString pythonExe(const QString& setupData);

struct Package
{
    QString dir;       // <data dir>/HelixSR-<version>
    Version version;
};

// The newest package in `data` that finished building (see markBuilt); nullopt if none.
std::optional<Package> builtPackage(const QString& data);

// Records that the network in `packageDir` was built completely.
bool markBuilt(const QString& packageDir, const Version& version);

// Folders of other versions in `data` (built or half-built) that are not `keepDir`.
QStringList otherPackages(const QString& data, const QString& keepDir);

// Existing files and folders that are only needed while building: the Scaler download, the
// ready-to-copy "manual" folder, the shader compiler, numpy and the archives. Python itself stays.
QStringList buildLeftovers(const QString& packageDir, const QString& setupData);

} // namespace Helix
