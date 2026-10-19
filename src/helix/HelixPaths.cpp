#include "HelixPaths.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Helix {

namespace {

const QString kBuiltMarker = QStringLiteral(".windowmanager-built");
const QString kPackagePrefix = QStringLiteral("HelixSR-");

} // namespace

QString dataDir()
{
    return QCoreApplication::applicationDirPath() + "/helixsr";
}

QString setupDataDir()
{
    return qEnvironmentVariable("LOCALAPPDATA") + "/HelixSR";
}

QString pythonExe(const QString& setupData)
{
    return setupData + "/python/python.exe";
}

std::optional<Package> builtPackage(const QString& data)
{
    std::optional<Package> best;
    const auto dirs = QDir(data).entryInfoList({kPackagePrefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& d : dirs) {
        QFile marker(d.absoluteFilePath() + '/' + kBuiltMarker);
        if (!marker.open(QIODevice::ReadOnly))
            continue;
        // line 1: the version; an optional "driver=<fingerprint>" line: the helper script that passed the self-test
        const QStringList lines = QString::fromUtf8(marker.readAll()).split('\n', Qt::SkipEmptyParts);
        const auto version = lines.isEmpty() ? std::nullopt : parseVersion(lines.first());
        if (!version)
            continue;
        QString driver;
        for (const QString& line : lines) {
            if (line.startsWith("driver="))
                driver = line.mid(7).trimmed();
        }
        if (!best || *version > best->version)
            best = Package{d.absoluteFilePath(), *version, driver};
    }
    return best;
}

QString driverFingerprint(const QByteArray& script)
{
    return QString::fromLatin1(QCryptographicHash::hash(script, QCryptographicHash::Sha256).toHex().left(16));
}

bool markBuilt(const QString& packageDir, const Version& version, const QString& driver)
{
    QFile marker(packageDir + '/' + kBuiltMarker);
    if (!marker.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    QString text = version.toString() + '\n';
    if (!driver.isEmpty())
        text += "driver=" + driver + '\n';
    marker.write(text.toUtf8());
    return true;
}

QStringList otherPackages(const QString& data, const QString& keepDir)
{
    QStringList out;
    const QString keep = QFileInfo(keepDir).absoluteFilePath();
    for (const QFileInfo& d : QDir(data).entryInfoList({kPackagePrefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (d.absoluteFilePath() != keep)
            out << d.absoluteFilePath();
    }
    return out;
}

QStringList buildLeftovers(const QString& packageDir, const QString& setupData)
{
    QStringList candidates = {
        packageDir + "/helixsr-scaler",
        packageDir + "/manual",
        setupData + "/dxc",
        setupData + "/dxc-unpack",
        setupData + "/dxc.zip",
        setupData + "/python.zip",
        setupData + "/numpy.zip",
        setupData + "/python/numpy",
        setupData + "/python/numpy.libs",
    };
    for (const QFileInfo& d : QDir(setupData + "/python").entryInfoList({"numpy-*.dist-info"}, QDir::Dirs))
        candidates << d.absoluteFilePath();

    QStringList out;
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c))
            out << c;
    }
    return out;
}

} // namespace Helix
