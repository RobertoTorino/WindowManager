#include "Logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

static std::string levelToString(Logger::Level level)
{
    switch (level) {
    case Logger::Level::Debug: return "DEBUG";
    case Logger::Level::Info:  return "INFO ";
    case Logger::Level::Warn:  return "WARN ";
    case Logger::Level::Error: return "ERROR";
    }
    return "?????";
}

static std::string currentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    std::time_t t  = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

Logger& Logger::instance()
{
    static Logger s;
    return s;
}

Logger::Logger()
    : m_logFile("windowmanager.log")
    , m_oldLogFile("windowmanager_old.log")
{}

void Logger::setLogFile(const std::filesystem::path& path)
{
    std::lock_guard lock(m_mutex);
    m_logFile    = path;
    m_oldLogFile = path.parent_path() /
                   (path.stem().string() + "_old" + path.extension().string());
}

void Logger::setMaxSize(std::size_t bytes)
{
    std::lock_guard lock(m_mutex);
    m_maxSize = bytes;
}

void Logger::log(Level level, const std::string& message, const std::string& source)
{
    std::lock_guard lock(m_mutex);
    const std::string entry =
        "[" + currentTimestamp() + "] [" +
        levelToString(level)     + "] [" +
        (source.empty() ? "?" : source) + "] " +
        message + "\n";
    writeToFile(entry);
}

void Logger::writeToFile(const std::string& text)
{
    std::ofstream ofs(m_logFile, std::ios::app);
    if (ofs)
        ofs << text;
    ofs.close();
    rotateLogs();
}

void Logger::rotateLogs()
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(m_logFile, ec);
    if (!ec && size > m_maxSize)
        std::filesystem::rename(m_logFile, m_oldLogFile, ec);
}

void Logger::debug(const std::string& msg, const std::string& src) { log(Level::Debug, msg, src); }
void Logger::info (const std::string& msg, const std::string& src) { log(Level::Info,  msg, src); }
void Logger::warn (const std::string& msg, const std::string& src) { log(Level::Warn,  msg, src); }
void Logger::error(const std::string& msg, const std::string& src) { log(Level::Error, msg, src); }
