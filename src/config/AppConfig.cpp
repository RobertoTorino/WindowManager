#include "AppConfig.h"

#include "../core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

AppConfig& AppConfig::instance()
{
    static AppConfig s;
    return s;
}

AppConfig::AppConfig() = default;

void AppConfig::setConfigPathOverride(const QString& path)
{
    m_configPathOverride = path;
}

QString AppConfig::configFilePath() const
{
    if (!m_configPathOverride.isEmpty())
        return m_configPathOverride;
    const QString dir = QCoreApplication::applicationDirPath();
    QDir{}.mkpath(dir);
    return dir + "/windowmanager.json";
}

// ---------------------------------------------------------------------------
// Load / Save
// ---------------------------------------------------------------------------

bool AppConfig::load()
{
    QFile f(configFilePath());
    if (!f.exists()) {
        Logger::instance().info("No config file found, starting fresh.", "AppConfig");
        return true;
    }
    if (!f.open(QIODevice::ReadOnly)) {
        Logger::instance().error(
            "Failed to open config: " + f.fileName().toStdString(), "AppConfig");
        return false;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError) {
        Logger::instance().error(
            "JSON parse error: " + err.errorString().toStdString(), "AppConfig");
        return false;
    }

    const QJsonObject root = doc.object();
    m_currentExePath = root["lastExePath"].toString();

    m_games.clear();
    for (const auto& v : root["games"].toArray()) {
        const QJsonObject obj = v.toObject();
        const QString path = obj["ApplicationPath"].toString();
        if (path.isEmpty()) continue;

        GameEntry game;
        game.applicationPath = path;
        game.captureDir      = obj["CaptureDir"].toString();
        game.ebootIsoPath    = obj["EbootIsoPath"].toString();
        game.gameApplication = obj["GameApplication"].toString();
        game.id              = obj["Id"].toString();
        game.isPatchable     = obj["IsPatchable"].toString("false");
        game.launcherType    = obj["LauncherType"].toString("STANDARD");
        game.savedName       = obj["SavedName"].toString();
        game.snapshotDir     = obj["SnapshotDir"].toString();
        game.helixConfirmed  = obj["HelixConfirmed"].toBool(false);
        game.childExePath    = obj["ChildExePath"].toString();
        m_games.insert(path, game);
    }

    m_profiles.clear();
    for (const auto& v : root["profiles"].toArray()) {
        const QJsonObject obj = v.toObject();
        const QString path    = obj["exePath"].toString();
        if (path.isEmpty()) continue;

        WindowProfile wp;
        wp.x          = obj["x"].toInt(0);
        wp.y          = obj["y"].toInt(0);
        wp.w          = obj["w"].toInt(1920);
        wp.h          = obj["h"].toInt(1080);
        wp.hasProfile = obj["hasProfile"].toBool(false);
        m_profiles.insert(path, wp);
    }

    Logger::instance().info(
        "Config loaded. Profiles: " + std::to_string(m_profiles.size()), "AppConfig");
    return true;
}

bool AppConfig::save()
{
    QJsonArray games;
    for (auto it = m_games.constBegin(); it != m_games.constEnd(); ++it) {
        QJsonObject obj;
        obj["ApplicationPath"] = it.value().applicationPath;
        obj["CaptureDir"]      = it.value().captureDir;
        obj["EbootIsoPath"]    = it.value().ebootIsoPath;
        obj["GameApplication"] = it.value().gameApplication;
        obj["Id"]              = it.value().id;
        obj["IsPatchable"]     = it.value().isPatchable;
        obj["LauncherType"]    = it.value().launcherType;
        obj["SavedName"]       = it.value().savedName;
        obj["SnapshotDir"]     = it.value().snapshotDir;
        obj["HelixConfirmed"]  = it.value().helixConfirmed;
        obj["ChildExePath"]    = it.value().childExePath;
        games.append(obj);
    }

    QJsonArray profiles;
    for (auto it = m_profiles.constBegin(); it != m_profiles.constEnd(); ++it) {
        QJsonObject obj;
        obj["exePath"]    = it.key();
        obj["x"]          = it.value().x;
        obj["y"]          = it.value().y;
        obj["w"]          = it.value().w;
        obj["h"]          = it.value().h;
        obj["hasProfile"] = it.value().hasProfile;
        profiles.append(obj);
    }

    QJsonObject root;
    root["lastExePath"] = m_currentExePath;
    root["games"]       = games;
    root["profiles"]    = profiles;

    // QSaveFile writes to a temp file and renames on commit, so a crash
    // mid-write can no longer leave a truncated config behind.
    QSaveFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly)) {
        Logger::instance().error(
            "Failed to write config: " + f.fileName().toStdString(), "AppConfig");
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        Logger::instance().error(
            "Failed to commit config: " + f.fileName().toStdString(), "AppConfig");
        return false;
    }

    Logger::instance().info("Config saved.", "AppConfig");
    return true;
}

QString AppConfig::configPath() const
{
    return configFilePath();
}

// ---------------------------------------------------------------------------
// Target exe
// ---------------------------------------------------------------------------

QString AppConfig::currentExePath() const { return m_currentExePath; }

void AppConfig::setCurrentExePath(const QString& path)
{
    m_currentExePath = path;
}

QList<AppConfig::GameEntry> AppConfig::games() const
{
    return m_games.values();
}

std::optional<AppConfig::GameEntry> AppConfig::gameByPath(const QString& applicationPath) const
{
    auto it = m_games.constFind(applicationPath);
    if (it == m_games.constEnd()) return std::nullopt;
    return *it;
}

std::optional<AppConfig::GameEntry> AppConfig::currentGame() const
{
    if (m_currentExePath.isEmpty()) return std::nullopt;
    return gameByPath(m_currentExePath);
}

void AppConfig::saveGame(const GameEntry& game)
{
    if (game.applicationPath.isEmpty())
        return;

    m_games.insert(game.applicationPath, game);
    save();
}

// ---------------------------------------------------------------------------
// Per-exe window profiles
// ---------------------------------------------------------------------------

std::optional<AppConfig::WindowProfile>
AppConfig::getWindowProfile(const QString& exePath) const
{
    auto it = m_profiles.constFind(exePath);
    if (it == m_profiles.constEnd()) return std::nullopt;
    return *it;
}

void AppConfig::saveWindowProfile(const QString& exePath, int x, int y, int w, int h)
{
    m_profiles.insert(exePath, WindowProfile{ x, y, w, h, true });
    Logger::instance().info(
        "Window profile saved for " + exePath.toStdString() +
        " [" + std::to_string(w) + "x" + std::to_string(h) + "]",
        "AppConfig");
    save();
}

void AppConfig::clearWindowProfile(const QString& exePath)
{
    m_profiles.remove(exePath);
}

std::optional<AppConfig::WindowProfile> AppConfig::currentWindowProfile() const
{
    if (m_currentExePath.isEmpty()) return std::nullopt;
    return getWindowProfile(m_currentExePath);
}

void AppConfig::saveCurrentWindowProfile(int x, int y, int w, int h)
{
    if (m_currentExePath.isEmpty()) return;
    saveWindowProfile(m_currentExePath, x, y, w, h);
}
