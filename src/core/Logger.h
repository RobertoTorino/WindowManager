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

    void writeToFile(const std::string& text);
    void rotateLogs();

    std::filesystem::path m_logFile;
    std::filesystem::path m_oldLogFile;
    std::size_t m_maxSize{1024u * 1024u};
    std::mutex m_mutex;
};
