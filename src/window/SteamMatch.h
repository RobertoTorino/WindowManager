#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/**
 * Picks out a Steam game's window by its AppID instead of its exe path.
 *
 * Under Proton the process that owns a game window can have a different pid than the host sees
 * (the game runs in a container), and its image is a Wine loader, so the exe path is a weak key.
 * Steam marks game windows with the AppID: an X property `STEAM_GAME` and the window class
 * `steam_app_<appid>`. A game that is not from Steam (AppID 0) is matched by path or exe name.
 *
 * Pure logic, no Qt and no OS calls, so it is unit-tested.
 */
namespace SteamMatch {

// 12345 for "steam_app_12345" (any case); 0 when the class is not a Steam game class.
std::uint32_t appIdFromWindowClass(std::string_view windowClass);

// What the user may type or paste for a game: "12345", "steam_app_12345", "steam://rungameid/12345"
// or a store page ".../app/12345/Name". 0 when no AppID can be read.
std::uint32_t parseAppId(std::string_view text);

// "steam://rungameid/12345", the URL that makes Steam start the game.
std::string launchUrl(std::uint32_t appId);

struct Target
{
    std::uint32_t appId{0};      // 0 = not a Steam game
    std::string   pathLower;     // full image path, lower-case (may be empty)
    std::string   exeLower;      // file name, lower-case (may be empty)
};

struct WindowFacts
{
    std::uint32_t appId{0};      // from STEAM_GAME or the window class; 0 = unknown
    std::string   pathLower;     // image path of the owning process (may be empty)
    std::string   exeLower;
};

enum class How { None, AppId, Path, ExeName };

// Strongest reason the window belongs to the target. A window that Steam says is another game
// never matches, even when the exe name is the same.
How match(const Target& target, const WindowFacts& window);

inline bool matches(const Target& target, const WindowFacts& window)
{
    return match(target, window) != How::None;
}

} // namespace SteamMatch
