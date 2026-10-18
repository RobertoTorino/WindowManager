#pragma once

#include <optional>
#include <QHash>
#include <QList>
#include <QString>

/**
 * Replaces AHK ConfigManager.
 *
 * In the standalone C++ version there is no "game library" — the user picks
 * a target executable via a file picker.  AppConfig stores and retrieves a
 * window profile (position + size) keyed by the absolute path of that exe.
 *
 * Persistence: JSON file written to the OS app-data directory.
 */
class AppConfig
{
public:
    struct GameEntry
    {
        QString applicationPath;
        QString captureDir;
        QString ebootIsoPath;
        QString gameApplication;
        QString id;
        QString isPatchable{"false"};
        QString launcherType{"STANDARD"};
        QString savedName;
        QString snapshotDir;
        bool    helixConfirmed{false};   // checked for an FSR 3.1 DLL: listed on the HelixSR tab
    };

    struct WindowProfile
    {
        int  x{0};
        int  y{0};
        int  w{1920};
        int  h{1080};
        bool hasProfile{false};
    };

    static AppConfig& instance();

    bool load();
    bool save();
    QString configPath() const;

    // Redirects the config file (used by tests); empty restores the default
    // location next to the executable. Does not reload.
    void setConfigPathOverride(const QString& path);

    // Currently selected target executable
    QString currentExePath() const;
    void    setCurrentExePath(const QString& path);

    QList<GameEntry> games() const;
    std::optional<GameEntry> gameByPath(const QString& applicationPath) const;
    std::optional<GameEntry> currentGame() const;
    void saveGame(const GameEntry& game);

    // Per-exe window profiles
    std::optional<WindowProfile> getWindowProfile(const QString& exePath) const;
    void saveWindowProfile(const QString& exePath, int x, int y, int w, int h);
    void clearWindowProfile(const QString& exePath);

    // Convenience: operate on the currently selected exe
    std::optional<WindowProfile> currentWindowProfile() const;
    void saveCurrentWindowProfile(int x, int y, int w, int h);

private:
    AppConfig();
    AppConfig(const AppConfig&) = delete;
    AppConfig& operator=(const AppConfig&) = delete;

    QString configFilePath() const;

    QString                       m_configPathOverride;
    QString                       m_currentExePath;
    QHash<QString, GameEntry>     m_games;
    QHash<QString, WindowProfile> m_profiles;
};
