#include "SteamLibrary.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QThreadPool>

#include <algorithm>
#include <filesystem>

namespace SteamLibrary {

namespace {

// Valve's KeyValues text: "key" "value" pairs and "key" { ... } blocks. Flattened to
// ("outer/inner/key" in lower case, value) pairs, which is all the two files need.
QList<QPair<QString, QString>> flatten(const QString& text)
{
    QStringList tokens;   // "{", "}" or a string; a string token is stored with a leading '"'
    for (qsizetype i = 0; i < text.size();) {
        const QChar c = text[i];
        if (c.isSpace()) {
            ++i;
        } else if (c == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n')
                ++i;
        } else if (c == '{' || c == '}') {
            tokens << QString(c);
            ++i;
        } else if (c == '"') {
            QString s("\"");
            for (++i; i < text.size() && text[i] != '"'; ++i) {
                if (text[i] == '\\' && i + 1 < text.size()) {
                    const QChar e = text[++i];
                    s += e == 'n' ? QChar('\n') : e == 't' ? QChar('\t') : e;
                } else {
                    s += text[i];
                }
            }
            ++i;   // the closing quote
            tokens << s;
        } else {
            QString s("\"");
            while (i < text.size() && !text[i].isSpace() && text[i] != '{' && text[i] != '}' && text[i] != '"')
                s += text[i++];
            tokens << s;
        }
    }

    QList<QPair<QString, QString>> out;
    QStringList path;
    for (qsizetype i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "}") {
            if (!path.isEmpty())
                path.removeLast();
            continue;
        }
        if (tokens[i] == "{")
            continue;   // a block without a key: malformed, skip the brace
        const QString key = tokens[i].mid(1).toLower();
        if (i + 1 >= tokens.size())
            break;
        if (tokens[i + 1] == "{") {
            path << key;
            ++i;
        } else if (tokens[i + 1] != "}") {
            out.append({(path + QStringList{key}).join('/'), tokens[i + 1].mid(1)});
            ++i;
        }
    }
    return out;
}

bool isNumber(const QString& s)
{
    bool ok = false;
    s.toUInt(&ok);
    return ok;
}

const QStringList kSkipNameParts = {
    "crash", "unins", "redist", "setup", "install", "easyanticheat", "battleye", "be_service",
    "start_protected_game", "dxwebsetup", "dotnet", "prereq", "report", "helper", "cefprocess",
    "webhelper", "updater", "touchup", "vc_redist", "vcredist", "dxsetup", "ue4prereq",
};

const QStringList kSkipFolders = {
    "_commonredist", "commonredist", "redist", "redistributables", "__installer", "directx",
    "support", "easyanticheat", "battleye", "installers", "prereqs", "_redist",
};

bool isHelperExe(const QString& relativePath)
{
    const QStringList parts = relativePath.toLower().split('/', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return true;
    for (qsizetype i = 0; i + 1 < parts.size(); ++i) {
        if (kSkipFolders.contains(parts[i]))
            return true;
    }
    for (const QString& p : kSkipNameParts) {
        if (parts.last().contains(p))
            return true;
    }
    return false;
}

// Executables in the install folder: .exe (Windows games, also under Proton), and on Linux the files
// a native game starts (*.x86_64, *.x86 and extension-less programs).
QList<ExeCandidate> exeCandidates(const QString& installPath)
{
    QList<ExeCandidate> out;
    const QDir root(installPath);
    QDirIterator it(installPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo f = it.nextFileInfo();
        const QString suffix = f.suffix().toLower();
        bool exe = suffix == "exe";
#ifdef Q_OS_LINUX
        exe = exe || ((suffix == "x86_64" || suffix == "x86" || suffix.isEmpty()) && f.isExecutable());
#endif
        if (exe && !f.isSymLink())
            out.append({root.relativeFilePath(f.absoluteFilePath()), f.size()});
    }
    return out;
}

QString readFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(f.readAll());
}

} // namespace

QStringList parseLibraryFolders(const QString& vdf)
{
    QStringList out;
    for (const auto& [key, value] : flatten(vdf)) {
        const QStringList parts = key.split('/');
        if (parts.size() < 2 || parts[0] != "libraryfolders" || !isNumber(parts[1]))
            continue;
        // current format: "0" { "path" "..." }; old format: "1" "D:\\SteamLibrary"
        if ((parts.size() == 3 && parts[2] == "path") || parts.size() == 2)
            out << QString(value).replace('\\', '/');   // Windows paths, also when read on Linux
    }
    return out;
}

std::optional<Manifest> parseManifest(const QString& acf)
{
    Manifest m;
    for (const auto& [key, value] : flatten(acf)) {
        if (key == "appstate/appid")
            m.appId = value.toUInt();
        else if (key == "appstate/name")
            m.name = value;
        else if (key == "appstate/installdir")
            m.installDir = value;
        else if (key == "appstate/stateflags")
            m.fullyInstalled = (value.toUInt() & 4u) != 0;   // StateFullyInstalled
    }
    if (m.appId == 0)
        return std::nullopt;
    return m;
}

bool isTool(quint32 appId, const QString& name)
{
    static const QSet<quint32> kTools = {
        228980,    // Steamworks Common Redistributables
        1070560,   // Steam Linux Runtime 1.0 (scout)
        1391110,   // Steam Linux Runtime 2.0 (soldier)
        1628350,   // Steam Linux Runtime 3.0 (sniper)
        4183110,   // Steam Linux Runtime 4.0
        1493710,   // Proton Experimental
        2180100,   // Proton Hotfix
        1826330,   // Proton EasyAntiCheat Runtime
        1161040,   // Proton BattlEye Runtime
    };
    return kTools.contains(appId) || name.startsWith("Proton", Qt::CaseInsensitive)
           || name.startsWith("Steam Linux Runtime", Qt::CaseInsensitive)
           || name.startsWith("Steamworks", Qt::CaseInsensitive);
}

int pickMainExe(const QList<ExeCandidate>& candidates)
{
    int best = -1;
    for (int i = 0; i < candidates.size(); ++i) {
        if (isHelperExe(candidates[i].relativePath))
            continue;
        if (best < 0 || candidates[i].size > candidates[best].size)
            best = i;
    }
    return best;
}

QStringList defaultSteamRoots()
{
    QStringList candidates;
#ifdef Q_OS_WIN
    const QSettings reg("HKEY_CURRENT_USER\\Software\\Valve\\Steam", QSettings::NativeFormat);
    candidates << reg.value("SteamPath").toString() << "C:/Program Files (x86)/Steam";
#else
    const QString home = QDir::homePath();
    candidates << home + "/.local/share/Steam" << home + "/.steam/steam"
               << home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam";
#endif
    QStringList out;
    for (const QString& c : candidates) {
        const QString real = QFileInfo(c).canonicalFilePath();   // ~/.steam/steam is usually a link
        if (!real.isEmpty() && !out.contains(real))
            out << real;
    }
    return out;
}

Scan findGames(const QStringList& steamRoots)
{
    QStringList libraries;
    for (const QString& root : steamRoots) {
        QStringList listed = parseLibraryFolders(readFile(root + "/steamapps/libraryfolders.vdf"));
        listed.prepend(root);   // the Steam folder is always a library, even when the file is missing
        for (const QString& lib : listed) {
            // the same folder can have two names (/home and /var/home, a bind mount): keep the first
            const QString real = QFileInfo(lib).canonicalFilePath();
            std::error_code error;
            const bool known = std::any_of(libraries.begin(), libraries.end(), [&](const QString& l) {
                return std::filesystem::equivalent(l.toStdWString(), real.toStdWString(), error);
            });
            if (!real.isEmpty() && !known)
                libraries << real;
        }
    }

    Scan scan;
    scan.libraries = libraries;
    QSet<quint32> seen;
    for (const QString& lib : libraries) {
        const QDir apps(lib + "/steamapps");
        for (const QString& file : apps.entryList({"appmanifest_*.acf"}, QDir::Files, QDir::Name)) {
            const auto m = parseManifest(readFile(apps.filePath(file)));
            if (!m || !m->fullyInstalled || m->installDir.isEmpty() || isTool(m->appId, m->name)
                || seen.contains(m->appId))
                continue;
            Game g;
            g.appId = m->appId;
            g.name = m->name;
            g.library = lib;
            g.installPath = lib + "/steamapps/common/" + m->installDir;
            const auto exes = exeCandidates(g.installPath);
            const int main = pickMainExe(exes);
            if (main < 0)
                continue;
            g.exePath = QDir::cleanPath(g.installPath + '/' + exes[main].relativePath);
            seen.insert(g.appId);
            scan.games.append(g);
        }
    }
    return scan;
}

void findGamesAsync(QObject* context, std::function<void(const Scan&)> done)
{
    QPointer<QObject> target(context);
    QThreadPool::globalInstance()->start([target, done = std::move(done)]() {
        const Scan scan = findGames(defaultSteamRoots());
        if (!target)
            return;
        QMetaObject::invokeMethod(target.data(), [target, scan, done]() {
            if (target)
                done(scan);
        }, Qt::QueuedConnection);
    });
}

} // namespace SteamLibrary
