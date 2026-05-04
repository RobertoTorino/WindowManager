#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

class Utilities
{
public:
    Utilities() = delete;

    // String helpers
    static std::string sanitizeName(std::string str);
    static std::string joinStrings(const std::vector<std::string>& parts,
                                   std::string_view sep = ",");
    static std::string trimQuotesAndSpaces(std::string str);

    // Path helpers
    static std::string fileNameNoExt(const std::filesystem::path& path);
    static std::string fileExtension(const std::filesystem::path& path);
    static bool        isValidExePath(const std::filesystem::path& path);

    // Size formatting
    static std::string formatFileSize(std::uintmax_t bytes);

    // Timestamps
    static std::string currentTimestamp();
    static std::string shortTimestamp();

    // ID generation (collision-safe)
    static std::string generateUniqueId(const std::string& friendlyName,
                                        const std::vector<std::string>& existingIds);
};
