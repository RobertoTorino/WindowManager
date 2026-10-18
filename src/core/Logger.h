#pragma once

#include <filesystem>
#include <mutex>
#include <string>

class Logger
{
public:
    enum class Level { Debug, Info, Warn, Error };

    static Logger& instance();

    void setLogFile(const std::filesystem::path& path);
    void setMaxSize(std::size_t bytes);

    // Per-game logging: while a game is set, every entry is written to the
    // application log and to that game's own file under gameLogDirectory().
    // An empty name stops per-game logging.
    void setGame(const std::string& name);
    std::string currentGame();
    std::filesystem::path gameLogDirectory();
    std::filesystem::path gameLogPath(const std::string& name);
    std::filesystem::path appLogPath();

    void log(Level level, const std::string& message, const std::string& source = {});

    void debug(const std::string& msg, const std::string& src = {});
    void info (const std::string& msg, const std::string& src = {});
    void warn (const std::string& msg, const std::string& src = {});
    void error(const std::string& msg, const std::string& src = {});

private:
    Logger();
    ~Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static std::filesystem::path oldPathFor(const std::filesystem::path& file);
    void writeToFile(const std::filesystem::path& file, const std::string& text);
    void rotateLogs(const std::filesystem::path& file);
    std::filesystem::path gameLogPathLocked(const std::string& name) const;

    std::string m_game;
    std::filesystem::path m_logFile;
    std::size_t m_maxSize{1024u * 1024u};
    std::mutex m_mutex;
};
