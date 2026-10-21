#include "MagpieRelease.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Magpie {

namespace {

const QString kMarker = QStringLiteral(".windowmanager-version");

} // namespace

std::optional<Release> latestRelease(const QByteArray& releasesJson)
{
    const auto doc = QJsonDocument::fromJson(releasesJson);
    if (!doc.isArray())
        return std::nullopt;

    static const QRegularExpression zipName(R"(^Magpie-v(\d+\.\d+\.\d+)-x64\.zip$)");
    std::optional<Release> best;
    for (const auto& entry : doc.array()) {
        const auto obj = entry.toObject();
        if (obj["draft"].toBool() || obj["prerelease"].toBool())
            continue;
        for (const auto& a : obj["assets"].toArray()) {
            const auto asset = a.toObject();
            const auto m = zipName.match(asset["name"].toString());
            const QString digest = asset["digest"].toString();
            if (!m.hasMatch() || !digest.startsWith("sha256:", Qt::CaseInsensitive) || digest.size() != 7 + 64)
                continue;
            Release r;
            r.version = *Helix::parseVersion(m.captured(1));
            r.tag = obj["tag_name"].toString();
            r.published = obj["published_at"].toString();
            r.zipName = asset["name"].toString();
            r.zipUrl = asset["browser_download_url"].toString();
            r.zipSize = static_cast<qint64>(asset["size"].toDouble());
            r.sha256 = digest.mid(7).toLower();
            if (r.zipUrl.startsWith("https://") && (!best || r.version > best->version))
                best = r;
        }
    }
    return best;
}

QString installDir()
{
    return QCoreApplication::applicationDirPath() + "/magpie";
}

QString exePath(const QString& dir)
{
    return dir + "/Magpie.exe";
}

QString configPath(const QString& dir)
{
    return dir + "/config/config.json";
}

std::optional<Version> installedVersion(const QString& dir)
{
    QFile marker(dir + '/' + kMarker);
    if (!QFile::exists(exePath(dir)) || !marker.open(QIODevice::ReadOnly))
        return std::nullopt;
    return Helix::parseVersion(QString::fromUtf8(marker.readAll()).trimmed());
}

bool markInstalled(const QString& dir, const Version& version)
{
    QFile marker(dir + '/' + kMarker);
    return marker.open(QIODevice::WriteOnly | QIODevice::Truncate)
           && marker.write((version.toString() + '\n').toUtf8()) > 0;
}

bool keptOnUpdate(const QString& name)
{
    return name.compare("config", Qt::CaseInsensitive) == 0 || name.compare("cache", Qt::CaseInsensitive) == 0
           || name.compare("logs", Qt::CaseInsensitive) == 0;
}

} // namespace Magpie
