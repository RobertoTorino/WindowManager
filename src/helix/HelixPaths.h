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

// The shader compiler the HelixSR setup builds with (removed again after the build).
QString dxcExe(const QString& setupData);

// HelixSR's DLL in a package: in its HelixSR-fsrfg folder (HelixSR 2 on), else in the package root (1.x).
QString packageDll(const QString& packageDir);

// True for a package installed through HelixSR's JSON bridge (HelixSR 2 on), not resources/helixsr_driver.py.
bool hasBridge(const QString& packageDir);

struct Package
{
    QString dir;       // <data dir>/HelixSR-<version>
    Version version;
    QString driver;    // fingerprint of the helper script that passed the self-test; empty if none yet
};

// The newest package in `data` that finished building (see markBuilt); nullopt if none.
std::optional<Package> builtPackage(const QString& data);

// Short fingerprint of the install helper script (resources/helixsr_driver.py).
QString driverFingerprint(const QByteArray& script);

// Records that the network in `packageDir` was built completely, and (optionally) which helper
// script was checked against this package.
bool markBuilt(const QString& packageDir, const Version& version, const QString& driver = {});

// Folders of other versions in `data` (built or half-built) that are not `keepDir`.
QStringList otherPackages(const QString& data, const QString& keepDir);

// Existing files and folders that are only needed while building: the Scaler download, the
// ready-to-copy "manual" / "fsr3-drop-in" folders, the shader compiler, numpy and the archives.
// Python itself stays.
QStringList buildLeftovers(const QString& packageDir, const QString& setupData);

} // namespace Helix
