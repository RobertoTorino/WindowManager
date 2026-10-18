#pragma once

#include <optional>

#include <QByteArray>
#include <QString>

/**
 * Pure logic for the HelixSR integration: versions and GitHub release metadata.
 * No network, no widgets, so it is unit-tested directly (tst_helix).
 */
namespace Helix {

struct Version
{
    int major{0};
    int minor{0};
    int patch{0};

    bool operator==(const Version&) const = default;
    auto operator<=>(const Version&) const = default;
    QString toString() const;
};

// "1.9.3", "v1.9.3" or "HelixSR 1.9.3" (anything before the first digit group is ignored).
std::optional<Version> parseVersion(const QString& text);

// The version a HelixSR DLL announces about itself ("HelixSR 1.9.3" in its first 4 MB).
std::optional<Version> versionInDll(const QByteArray& bytes);

struct Release
{
    Version version;
    QString tag;
    QString zipName;
    QString zipUrl;
    qint64  zipSize{0};
    QString sha256;        // from the asset's "digest"; empty if GitHub gave none
    QString sumsUrl;       // SHA256SUMS.txt, the fallback when sha256 is empty
    QString published;     // ISO 8601, for display only
};

// Newest non-draft, non-prerelease release of the GitHub "releases" listing that carries a
// HelixSR-<version>.zip. nullopt for malformed JSON or when nothing qualifies.
std::optional<Release> latestRelease(const QByteArray& releasesJson);

// Hash for `fileName` from the text of a SHA256SUMS file ("<hash>  <name>" per line).
QString hashFromSums(const QString& sumsText, const QString& fileName);

// True when `latest` is newer than what is installed (nothing installed counts as newer).
bool isUpdate(const std::optional<Version>& installed, const Version& latest);

} // namespace Helix
