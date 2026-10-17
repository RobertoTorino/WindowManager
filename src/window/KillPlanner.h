#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

/**
 * Decides which processes belong to "our" game session so Shutdown never
 * touches an unrelated emulator the user started themselves.
 *
 * Pure logic over a process snapshot — no Win32 calls — so it is unit-testable.
 */
namespace KillPlanner {

struct ProcInfo
{
    std::uint32_t pid{0};
    std::uint32_t ppid{0};
    std::uint64_t created{0};   // creation time; only compared between processes
    std::string   exeLower;     // file name, lower-case, e.g. "dolphin.exe"
    std::string   pathLower;    // full image path, lower-case (may be empty)
};

struct Request
{
    std::unordered_set<std::uint32_t> rootPids;   // pids we launched / are managing
    std::string   targetPathLower;                // selected target (may be empty)
    std::string   targetExeLower;
    std::uint64_t launchTime{0};                  // 0 = we did not launch anything
    std::unordered_set<std::string> ignoreExes;   // never killed
    std::unordered_set<std::string> loaderExes;   // killed only when tied to our session
    std::uint32_t selfPid{0};                     // never killed
};

// Returns the pids to terminate:
//  * seeds: processes in rootPids or whose image is the selected target
//  * all their descendants
//  * loader processes (loaderExes) started after launchTime whose parent is
//    in the set or is gone (orphaned) — plus their descendants
// Anything in ignoreExes, and selfPid, is always excluded.
std::vector<std::uint32_t> plan(const std::vector<ProcInfo>& procs, const Request& req);

} // namespace KillPlanner
