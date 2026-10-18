#include "HelixRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Helix {

QString Version::toString() const
{
    return QString("%1.%2.%3").arg(major).arg(minor).arg(patch);
}

std::optional<Version> parseVersion(const QString& text)
{
    static const QRegularExpression re(R"((\d+)\.(\d+)\.(\d+))");
    const auto m = re.match(text);
    if (!m.hasMatch())
        return std::nullopt;
    return Version{m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()};
}

std::optional<Version> versionInDll(const QByteArray& bytes)
{
    static const QRegularExpression re(R"(HelixSR (\d+\.\d+\.\d+))");
    // Latin-1 keeps one character per byte, so the binary search is exact.
    const auto m = re.match(QString::fromLatin1(bytes.left(4 << 20)));
    if (!m.hasMatch())
        return std::nullopt;
    return parseVersion(m.captured(1));
}

std::optional<Release> latestRelease(const QByteArray& releasesJson)
{
    const auto doc = QJsonDocument::fromJson(releasesJson);
    if (!doc.isArray())
        return std::nullopt;

    static const QRegularExpression zipName(R"(^HelixSR-(\d+\.\d+\.\d+)\.zip$)");
    std::optional<Release> best;

    for (const auto& entry : doc.array()) {
        const auto obj = entry.toObject();
        if (obj["draft"].toBool() || obj["prerelease"].toBool())
            continue;

        Release r;
        r.tag = obj["tag_name"].toString();
        r.published = obj["published_at"].toString();
        QString sumsUrl;
        for (const auto& a : obj["assets"].toArray()) {
            const auto asset = a.toObject();
            const QString name = asset["name"].toString();
            if (name == "SHA256SUMS.txt")
                sumsUrl = asset["browser_download_url"].toString();
            const auto m = zipName.match(name);
            if (!m.hasMatch())
                continue;
            r.version = *parseVersion(m.captured(1));
            r.zipName = name;
            r.zipUrl = asset["browser_download_url"].toString();
            r.zipSize = static_cast<qint64>(asset["size"].toDouble());
            const QString digest = asset["digest"].toString();
            if (digest.startsWith("sha256:", Qt::CaseInsensitive))
                r.sha256 = digest.mid(7).toLower();
        }
        if (r.zipUrl.isEmpty())
            continue;
        r.sumsUrl = sumsUrl;
        if (!best || r.version > best->version)
            best = r;
    }
    return best;
}

QString hashFromSums(const QString& sumsText, const QString& fileName)
{
    for (const QString& line : sumsText.split('\n', Qt::SkipEmptyParts)) {
        const QStringList parts = line.simplified().split(' ');
        if (parts.size() < 2)
            continue;
        // sha256sum marks binary files with a leading '*' on the name
        QString name = parts.last();
        if (name.startsWith('*'))
            name.remove(0, 1);
        if (name == fileName && parts.first().size() == 64)
            return parts.first().toLower();
    }
    return {};
}

bool isUpdate(const std::optional<Version>& installed, const Version& latest)
{
    return !installed || latest > *installed;
}

} // namespace Helix
