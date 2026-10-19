#include "HelixGame.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QThreadPool>

namespace Helix {

namespace {

const QString kMarkerFile = QStringLiteral("helixsr-install.json");

void collect(const QDir& dir, int depth, QStringList& out)
{
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    const QFileInfo* upscaler = nullptr;
    const QFileInfo* generic = nullptr;
    for (const QFileInfo& e : entries) {
        if (!e.isFile())
            continue;
        if (e.fileName().compare(kFsrUpscalerDll, Qt::CaseInsensitive) == 0)
            upscaler = &e;
        else if (e.fileName().compare(kFsrGenericDll, Qt::CaseInsensitive) == 0)
            generic = &e;
    }
    if (upscaler)
        out << upscaler->absoluteFilePath();
    else if (generic)
        out << generic->absoluteFilePath();

    if (depth <= 0)
        return;
    for (const QFileInfo& e : entries) {
        if (e.isDir() && !e.isSymLink())
            collect(QDir(e.absoluteFilePath()), depth - 1, out);
    }
}

std::optional<Version> markerVersion(const QString& dllPath)
{
    QFile f(QFileInfo(dllPath).absolutePath() + '/' + kMarkerFile);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const auto obj = QJsonDocument::fromJson(f.readAll()).object();
    if (obj["route"].toString() != "direct")
        return std::nullopt;
    return parseVersion(obj["helixsr"].toString());
}

} // namespace

QString gameRootFor(const QString& exePath)
{
    static const QStringList binDirs = {"bin", "bin64", "bin32", "binaries", "x64", "x86",
                                        "x86_64", "win64", "win32", "retail"};
    // Never climb into a library's "common"/"steamapps" folder, a drive root or the home folder.
    auto up = [](QDir& d) {
        const QDir parent = QFileInfo(d.absolutePath()).dir();   // path-based: needs no existing folder
        if (parent == d || parent.isRoot() || parent.absolutePath() == QDir::homePath())
            return false;
        const QString name = parent.dirName().toLower();
        if (name == "common" || name == "steamapps")
            return false;
        d = parent;
        return true;
    };

    QDir dir = QFileInfo(exePath).absoluteDir();
    bool unreal = false;
    while (binDirs.contains(dir.dirName().toLower())) {
        unreal = unreal || dir.dirName().compare("binaries", Qt::CaseInsensitive) == 0;
        if (!up(dir))
            break;
    }
    if (unreal)
        up(dir);   // <game>/<project>/Binaries/Win64: the game is one folder above the project
    return dir.absolutePath();
}

QString originalNameFor(const QString& dllPath)
{
    const QFileInfo info(dllPath);
    return info.absolutePath() + '/' + info.completeBaseName() + ".original.dll";
}

QStringList findFsrTargets(const QString& folder, int maxDepth)
{
    QStringList out;
    collect(QDir(folder), maxDepth, out);
    out.sort(Qt::CaseInsensitive);
    return out;
}

bool isHelixDll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    return versionInDll(f.read(4 << 20)).has_value();
}

GameScan scanGame(const QString& exePath)
{
    GameScan scan;
    scan.root = gameRootFor(exePath);
    scan.targets = findFsrTargets(scan.root);
    if (scan.targets.isEmpty())
        return scan;   // Unsupported

    scan.status = GameScan::Status::Ready;
    bool any = false;
    bool all = true;
    for (const QString& t : scan.targets) {
        const bool helix = isHelixDll(t);
        any = any || helix;
        all = all && helix;
        if (helix && !scan.installedVersion)
            scan.installedVersion = markerVersion(t);
    }
    if (all)
        scan.status = GameScan::Status::Installed;
    else if (!any) {
        // HelixSR left its record and the game's backup, but the DLL is the game's again
        for (const QString& t : scan.targets) {
            if (markerVersion(t) && QFile::exists(originalNameFor(t))) {
                scan.status = GameScan::Status::Replaced;
                break;
            }
        }
    }
    return scan;
}

void scanGameAsync(QObject* context, const QString& exePath, std::function<void(const GameScan&)> done)
{
    QPointer<QObject> target(context);
    QThreadPool::globalInstance()->start([target, exePath, done = std::move(done)]() {
        const GameScan scan = scanGame(exePath);
        if (!target)
            return;
        QMetaObject::invokeMethod(target.data(), [target, scan, done]() {
            if (target)
                done(scan);
        }, Qt::QueuedConnection);
    });
}

} // namespace Helix
