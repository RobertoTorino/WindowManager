#pragma once

#include <optional>
#include <QHash>
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

    // Currently selected target executable
    QString currentExePath() const;
    void    setCurrentExePath(const QString& path);

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

    QString                    m_currentExePath;
    QHash<QString, WindowProfile> m_profiles;
};
