#pragma once

#include <optional>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "helix/HelixRelease.h"

/**
 * Pure logic for the app's own copy of Magpie: which GitHub release to download and where the copy
 * lives. No network, no processes (tst_magpie).
 */
namespace Magpie {

using Version = Helix::Version;

struct Release
{
    Version version;
    QString tag;
    QString zipName;     // Magpie-v<version>-x64.zip
    QString zipUrl;
    qint64  zipSize{0};
    QString sha256;      // from the asset's "digest" (Magpie publishes no checksum file)
    QString published;   // ISO 8601, for display only
};

// Newest non-draft, non-prerelease release of the GitHub "releases" listing that carries
// Magpie-v<version>-x64.zip with a SHA-256 digest. nullopt for malformed JSON or when nothing qualifies.
std::optional<Release> latestRelease(const QByteArray& releasesJson);

// <app dir>/magpie: the app's portable Magpie (its own config, separate from a Magpie the user installed).
QString installDir();
QString exePath(const QString& dir);      // <dir>/Magpie.exe
QString configPath(const QString& dir);   // <dir>/config/config.json (its existence makes Magpie portable)

// The version recorded by markInstalled(), when Magpie.exe is there too; nullopt otherwise.
std::optional<Version> installedVersion(const QString& dir);
bool markInstalled(const QString& dir, const Version& version);

// What an update keeps of the installed copy: Magpie's settings, its shader cache and its logs.
bool keptOnUpdate(const QString& name);

} // namespace Magpie
