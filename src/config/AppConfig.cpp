#include "AppConfig.h"

#include "../core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

AppConfig& AppConfig::instance()
{
    static AppConfig s;
    return s;
}

AppConfig::AppConfig() = default;

QString AppConfig::configFilePath() const
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
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
    root["profiles"]    = profiles;

    QFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::instance().error(
            "Failed to write config: " + f.fileName().toStdString(), "AppConfig");
        return false;
    }
    f.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    f.close();

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
