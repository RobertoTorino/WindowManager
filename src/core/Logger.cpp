#include "Logger.h"

#include <cctype>
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

// "<name>.log" -> "<name>_old.log" in the same folder
std::filesystem::path Logger::oldPathFor(const std::filesystem::path& file)
{
    return file.parent_path() / (file.stem().string() + "_old" + file.extension().string());
}

// Game names come from user input; keep the file name safe on Windows.
static std::string safeFileName(const std::string& name)
{
    std::string out;
    for (const unsigned char c : name)
        out += (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == ' ') ? char(c) : '_';
    return out.empty() ? std::string("game") : out;
}

Logger::Logger()
    : m_logFile("windowmanager.log")
{}

void Logger::setLogFile(const std::filesystem::path& path)
{
    std::lock_guard lock(m_mutex);
    m_logFile = path;
}

void Logger::setGame(const std::string& name)
{
    std::lock_guard lock(m_mutex);
    if (name == m_game)
        return;
    m_game = name;
    if (!m_game.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(gameLogPathLocked(m_game).parent_path(), ec);
    }
}

std::string Logger::currentGame()
{
    std::lock_guard lock(m_mutex);
    return m_game;
}

std::filesystem::path Logger::gameLogPathLocked(const std::string& name) const
{
    return m_logFile.parent_path() / "logs" / (safeFileName(name) + ".log");
}

std::filesystem::path Logger::gameLogDirectory()
{
    std::lock_guard lock(m_mutex);
    return m_logFile.parent_path() / "logs";
}

std::filesystem::path Logger::gameLogPath(const std::string& name)
{
    std::lock_guard lock(m_mutex);
    return gameLogPathLocked(name);
}

std::filesystem::path Logger::appLogPath()
{
    std::lock_guard lock(m_mutex);
    return m_logFile;
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
    writeToFile(m_logFile, entry);
    if (!m_game.empty())
        writeToFile(gameLogPathLocked(m_game), entry);
}

void Logger::writeToFile(const std::filesystem::path& file, const std::string& text)
{
    std::ofstream ofs(file, std::ios::app);
    if (ofs)
        ofs << text;
    ofs.close();
    rotateLogs(file);
}

void Logger::rotateLogs(const std::filesystem::path& file)
{
    std::error_code ec;
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size <= m_maxSize)
        return;
    const auto old = oldPathFor(file);
    std::filesystem::rename(file, old, ec);
}

void Logger::debug(const std::string& msg, const std::string& src) { log(Level::Debug, msg, src); }
void Logger::info (const std::string& msg, const std::string& src) { log(Level::Info,  msg, src); }
void Logger::warn (const std::string& msg, const std::string& src) { log(Level::Warn,  msg, src); }
void Logger::error(const std::string& msg, const std::string& src) { log(Level::Error, msg, src); }
