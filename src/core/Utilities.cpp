#include "Utilities.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

std::string Utilities::sanitizeName(std::string str)
{
    constexpr std::string_view kIllegal = R"("\/:<>*?|)";
    str.erase(std::remove_if(str.begin(), str.end(),
                  [&](char c) { return kIllegal.find(c) != std::string_view::npos; }),
              str.end());
    return str;
}

std::string Utilities::joinStrings(const std::vector<std::string>& parts,
                                    std::string_view sep)
{
    if (parts.empty()) return {};
    std::string result;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) result += sep;
        result += parts[i];
    }
    return result;
}

std::string Utilities::trimQuotesAndSpaces(std::string str)
{
    auto isJunk = [](char c) { return c == ' ' || c == '"'; };
    str.erase(str.begin(), std::find_if_not(str.begin(), str.end(), isJunk));
    str.erase(std::find_if_not(str.rbegin(), str.rend(), isJunk).base(), str.end());
    return str;
}

std::string Utilities::fileNameNoExt(const std::filesystem::path& path)
{
    return path.stem().string();
}

std::string Utilities::fileExtension(const std::filesystem::path& path)
{
    auto ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.')
        ext = ext.substr(1);
    return ext;
}

bool Utilities::isValidExePath(const std::filesystem::path& path)
{
    if (!std::filesystem::exists(path)) return false;
    auto ext = fileExtension(path);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext == "exe";
}

std::string Utilities::formatFileSize(std::uintmax_t bytes)
{
    static constexpr std::array<std::string_view, 5> kUnits{ "B", "KB", "MB", "GB", "TB" };
    double size = static_cast<double>(bytes);
    std::size_t idx = 0;
    while (size >= 1024.0 && idx + 1 < kUnits.size()) {
        size /= 1024.0;
        ++idx;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << size << ' ' << kUnits[idx];
    return oss.str();
}

static std::string makeTimestamp(const char* fmt)
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
    oss << std::put_time(&tm, fmt);
    return oss.str();
}

std::string Utilities::currentTimestamp() { return makeTimestamp("%Y-%m-%d %H:%M:%S"); }
std::string Utilities::shortTimestamp()   { return makeTimestamp("%Y%m%d_%H%M%S"); }

std::string Utilities::generateUniqueId(const std::string& friendlyName,
                                         const std::vector<std::string>& existingIds)
{
    std::string base = sanitizeName(friendlyName).substr(0, 20);
    if (std::find(existingIds.begin(), existingIds.end(), base) == existingIds.end())
        return base;

    for (int counter = 1; counter < 10000; ++counter) {
        std::string candidate = base + "_" + std::to_string(counter);
        if (std::find(existingIds.begin(), existingIds.end(), candidate) == existingIds.end())
            return candidate;
    }
    return base + "_" +
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
}
