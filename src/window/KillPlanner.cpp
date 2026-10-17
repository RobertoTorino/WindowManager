#include "KillPlanner.h"

#include <unordered_map>

namespace KillPlanner {

std::vector<std::uint32_t> plan(const std::vector<ProcInfo>& procs, const Request& req)
{
    std::unordered_map<std::uint32_t, const ProcInfo*> byPid;
    byPid.reserve(procs.size());
    for (const auto& p : procs)
        byPid.emplace(p.pid, &p);

    std::unordered_set<std::uint32_t> set;

    for (const auto& p : procs) {
        const bool isRoot = req.rootPids.count(p.pid) != 0;
        const bool isTarget =
            (!req.targetPathLower.empty() && p.pathLower == req.targetPathLower) ||
            (p.pathLower.empty() && !req.targetExeLower.empty() && p.exeLower == req.targetExeLower);
        if (isRoot || isTarget)
            set.insert(p.pid);
    }

    // A child only counts as a descendant if it was created after its parent;
    // this guards against pid reuse linking unrelated processes.
    auto addDescendants = [&]() {
        bool grew = true;
        while (grew) {
            grew = false;
            for (const auto& p : procs) {
                if (set.count(p.pid) || !set.count(p.ppid))
                    continue;
                const auto parent = byPid.find(p.ppid);
                if (parent != byPid.end() && p.created < parent->second->created)
                    continue;
                set.insert(p.pid);
                grew = true;
            }
        }
    };

    addDescendants();

    if (req.launchTime != 0) {
        for (const auto& p : procs) {
            if (set.count(p.pid) || !req.loaderExes.count(p.exeLower))
                continue;
            if (p.created < req.launchTime)
                continue;
            if (set.count(p.ppid) || !byPid.count(p.ppid))
                set.insert(p.pid);
        }
        addDescendants();
    }

    std::vector<std::uint32_t> result;
    for (const auto& p : procs) {
        if (!set.count(p.pid) || p.pid == req.selfPid || p.pid == 0)
            continue;
        if (req.ignoreExes.count(p.exeLower))
            continue;
        result.push_back(p.pid);
    }
    return result;
}

} // namespace KillPlanner
