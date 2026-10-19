#include "SteamMatch.h"

#include <algorithm>
#include <cctype>
#include <limits>

namespace SteamMatch {

namespace {

constexpr std::string_view kClassPrefix = "steam_app_";

std::string lower(std::string_view s)
{
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string_view trim(std::string_view s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())))
        s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())))
        s.remove_suffix(1);
    return s;
}

// The digits at the start of s as an AppID. 0 when there are none, the number has a leading
// zero, or it does not fit. `rest` gets what follows the digits.
std::uint32_t leadingId(std::string_view s, std::string_view* rest = nullptr)
{
    std::size_t n = 0;
    std::uint64_t value = 0;
    while (n < s.size() && s[n] >= '0' && s[n] <= '9') {
        value = value * 10 + static_cast<std::uint64_t>(s[n] - '0');
        if (value > std::numeric_limits<std::uint32_t>::max())
            return 0;
        ++n;
    }
    if (rest)
        *rest = s.substr(n);
    if (n == 0 || s[0] == '0')
        return 0;
    return static_cast<std::uint32_t>(value);
}

// An AppID that is the whole of s.
std::uint32_t wholeId(std::string_view s)
{
    std::string_view rest;
    const std::uint32_t id = leadingId(s, &rest);
    return rest.empty() ? id : 0;
}

} // namespace

std::uint32_t appIdFromWindowClass(std::string_view windowClass)
{
    const std::string cls = lower(trim(windowClass));
    if (cls.compare(0, kClassPrefix.size(), kClassPrefix) != 0)
        return 0;
    return wholeId(std::string_view(cls).substr(kClassPrefix.size()));
}

std::uint32_t parseAppId(std::string_view text)
{
    const std::string t = lower(trim(text));
    if (t.empty())
        return 0;

    if (const std::uint32_t id = appIdFromWindowClass(t))
        return id;

    for (const std::string_view marker : {std::string_view("rungameid/"), std::string_view("/app/")}) {
        const std::size_t at = t.find(marker);
        if (at != std::string::npos)
            return leadingId(std::string_view(t).substr(at + marker.size()));
    }
    return wholeId(t);   // a bare number
}

std::string launchUrl(std::uint32_t appId)
{
    return "steam://rungameid/" + std::to_string(appId);
}

How match(const Target& target, const WindowFacts& window)
{
    if (target.appId != 0) {
        if (window.appId == target.appId)
            return How::AppId;
        if (window.appId != 0)
            return How::None;   // Steam says this window is another game
    }

    if (!target.pathLower.empty() && target.pathLower == window.pathLower)
        return How::Path;
    if (!target.exeLower.empty() && target.exeLower == window.exeLower)
        return How::ExeName;
    return How::None;
}

} // namespace SteamMatch
