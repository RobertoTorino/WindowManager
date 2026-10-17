#include <QtTest>

#include <algorithm>

#include "window/KillPlanner.h"

using namespace KillPlanner;

namespace {

ProcInfo proc(uint32_t pid, uint32_t ppid, uint64_t created, const char* exe, const char* path = "")
{
    return ProcInfo{pid, ppid, created, exe, path};
}

Request baseRequest()
{
    Request r;
    r.ignoreExes = {"explorer.exe", "cmd.exe"};
    r.loaderExes = {"dolphin.exe", "teknoparrot.exe"};
    r.selfPid = 1;
    return r;
}

bool has(const std::vector<uint32_t>& v, uint32_t pid)
{
    return std::find(v.begin(), v.end(), pid) != v.end();
}

} // namespace

class TstKillPlanner : public QObject
{
    Q_OBJECT
private slots:
    void unrelatedLoaderIsSpared()
    {
        // The user runs their own Dolphin; we launched game.exe separately.
        std::vector<ProcInfo> procs{
            proc(100, 4, 10, "explorer.exe"),
            proc(200, 100, 20, "dolphin.exe"),
            proc(300, 100, 1000, "game.exe", "c:\\g\\game.exe"),
        };
        Request r = baseRequest();
        r.rootPids = {300};
        r.launchTime = 900;
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 300));
        QVERIFY(!has(kill, 200));
    }

    void descendantsOfLaunchAreKilled()
    {
        std::vector<ProcInfo> procs{
            proc(300, 100, 1000, "game.exe"),
            proc(301, 300, 1001, "helper.exe"),
            proc(302, 301, 1002, "grandchild.exe"),
            proc(999, 100, 1003, "unrelated.exe"),
        };
        Request r = baseRequest();
        r.rootPids = {300};
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 300) && has(kill, 301) && has(kill, 302));
        QVERIFY(!has(kill, 999));
    }

    void loaderInOurTreeIsKilled()
    {
        std::vector<ProcInfo> procs{
            proc(300, 100, 1000, "cmd.exe"),          // our batch launcher
            proc(301, 300, 1001, "teknoparrot.exe"),  // loader spawned by it
            proc(302, 301, 1002, "game.exe"),
        };
        Request r = baseRequest();
        r.rootPids = {300};
        r.launchTime = 1000;
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 301) && has(kill, 302));
        QVERIFY(!has(kill, 300));                     // cmd.exe is ignore-listed
    }

    void orphanedLoaderAfterLaunchIsKilled()
    {
        // The batch file exited; its loader's parent (pid 300) no longer exists.
        std::vector<ProcInfo> procs{
            proc(301, 300, 1001, "dolphin.exe"),
            proc(302, 301, 1002, "game.exe"),
        };
        Request r = baseRequest();
        r.launchTime = 1000;
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 301) && has(kill, 302));
    }

    void loaderBeforeLaunchIsSpared()
    {
        std::vector<ProcInfo> procs{proc(301, 300, 500, "dolphin.exe")};
        Request r = baseRequest();
        r.launchTime = 1000;
        QVERIFY(plan(procs, r).empty());
    }

    void noLaunchMeansLoadersAreSpared()
    {
        std::vector<ProcInfo> procs{proc(301, 300, 5000, "dolphin.exe")};
        Request r = baseRequest(); // launchTime == 0
        QVERIFY(plan(procs, r).empty());
    }

    void targetByPathIsKilled()
    {
        std::vector<ProcInfo> procs{
            proc(400, 4, 10, "game.exe", "c:\\g\\game.exe"),
            proc(401, 4, 10, "game.exe", "c:\\other\\game.exe"), // same name, other install
        };
        Request r = baseRequest();
        r.targetPathLower = "c:\\g\\game.exe";
        r.targetExeLower = "game.exe";
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 400));
        QVERIFY(!has(kill, 401));
    }

    void ignoredAndSelfAreNeverKilled()
    {
        std::vector<ProcInfo> procs{
            proc(1, 4, 10, "windowmanager.exe"),
            proc(500, 1, 20, "explorer.exe"),
        };
        Request r = baseRequest();
        r.rootPids = {1, 500};
        QVERIFY(plan(procs, r).empty());
    }

    void pidReuseDoesNotLinkUnrelatedProcess()
    {
        // pid 700 claims parent 300 but was created before it: a reused pid.
        std::vector<ProcInfo> procs{
            proc(300, 100, 1000, "game.exe"),
            proc(700, 300, 500, "old.exe"),
        };
        Request r = baseRequest();
        r.rootPids = {300};
        const auto kill = plan(procs, r);
        QVERIFY(has(kill, 300));
        QVERIFY(!has(kill, 700));
    }
};

QTEST_APPLESS_MAIN(TstKillPlanner)
#include "tst_killplanner.moc"
