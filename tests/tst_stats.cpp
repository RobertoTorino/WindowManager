#include <QtTest>

#include "stats/DriverModel.h"
#include "stats/StatsBackend.h"
#include "stats/StatsModel.h"

using namespace Stats;

namespace {

constexpr qint64 kMs = 1'000'000;   // ns
constexpr quint64 kGb = 1024ull * 1024 * 1024;

const quint64 kIntelLuid  = 0x0000F02Full;
const quint64 kNvidiaLuid = 0x0000F45Cull;

std::vector<Adapter> laptopAdapters()
{
    return {{kIntelLuid, "Intel(R) UHD Graphics", 128ull * 1024 * 1024},
            {kNvidiaLuid, "NVIDIA GeForce GTX 1650", 4 * kGb}};
}

QString engine(qint64 pid, quint64 luid, int eng, const QString &type)
{
    return QString("pid_%1_luid_0x00000000_0x%2_phys_0_eng_%3_engtype_%4")
        .arg(pid).arg(luid, 8, 16, QChar('0')).arg(eng).arg(type);
}

// Presents every `frameMs` from `startNs`, `count` of them.
std::vector<qint64> presents(qint64 startNs, int count, double frameMs)
{
    std::vector<qint64> out;
    for (int i = 0; i < count; ++i)
        out.push_back(startNs + qint64(i * frameMs * kMs));
    return out;
}

RawSample alive(qint64 timeNs, qint64 cpu100ns)
{
    RawSample raw;
    raw.processAlive = true;
    raw.timeNs = timeNs;
    raw.processCpu100ns = cpu100ns;
    raw.cpus = 8;
    raw.workingSetBytes = 6 * kGb;
    raw.power.known = true;
    raw.fpsSource = FpsSource::Running;
    return raw;
}

} // namespace

class TstStats : public QObject
{
    Q_OBJECT
private slots:
    // --- GPU counter names ---------------------------------------------------

    void parsesEngineInstance()
    {
        const auto gpu = parseGpuInstance("pid_23412_luid_0x00000000_0x0000F02F_phys_0_eng_0_engtype_3D");
        QVERIFY(gpu.has_value());
        QCOMPARE(gpu->pid, qint64(23412));
        QCOMPARE(gpu->luid, quint64(0xF02F));
        QCOMPARE(gpu->engine, QString("3D"));
    }

    void parsesHighLuidAndEngineWithUnderscore()
    {
        const auto gpu = parseGpuInstance("pid_4_luid_0x00000001_0x00012345_phys_0_eng_7_engtype_Compute_1");
        QVERIFY(gpu.has_value());
        QCOMPARE(gpu->luid, (quint64(1) << 32) | 0x12345);
        QCOMPARE(gpu->engine, QString("Compute_1"));
    }

    void parsesMemoryInstance()
    {
        const auto gpu = parseGpuInstance("pid_23412_luid_0x00000000_0x0000F45C_phys_0");
        QVERIFY(gpu.has_value());
        QCOMPARE(gpu->luid, kNvidiaLuid);
        QVERIFY(gpu->engine.isEmpty());
    }

    void rejectsOtherNames()
    {
        QVERIFY(!parseGpuInstance("_Total").has_value());
        QVERIFY(!parseGpuInstance("luid_0x00000000_0x0000F45C_phys_0").has_value());
        QVERIFY(!parseGpuInstance("pid_x_luid_0x0_0x0_phys_0").has_value());
    }

    // --- GPU use -------------------------------------------------------------

    void gpuUseIsTheBusiestEngineSummedPerType()
    {
        const std::vector<CounterValue> engines = {
            {engine(10, kNvidiaLuid, 0, "3D"), 40},   // two 3D engines of one adapter add up
            {engine(10, kNvidiaLuid, 1, "3D"), 35},
            {engine(10, kNvidiaLuid, 2, "Copy"), 50},
            {engine(10, kIntelLuid, 0, "3D"), 5},
            {engine(99, kNvidiaLuid, 0, "3D"), 90},   // another process
        };
        const GpuUse use = gpuUseFor(10, engines);
        QCOMPARE(use.percent, 75.0);
        QCOMPARE(use.luid, kNvidiaLuid);
    }

    void gpuUseIsCappedAndZeroWithoutCounters()
    {
        QCOMPARE(gpuUseFor(10, {{engine(10, kNvidiaLuid, 0, "3D"), 80}, {engine(10, kNvidiaLuid, 1, "3D"), 70}}).percent,
                 100.0);
        const GpuUse none = gpuUseFor(10, {{engine(11, kNvidiaLuid, 0, "3D"), 50}});
        QCOMPARE(none.percent, 0.0);
        QCOMPARE(none.luid, quint64(0));
    }

    void videoMemorySumsTheProcessAdapters()
    {
        const std::vector<CounterValue> memory = {
            {"pid_10_luid_0x00000000_0x0000F45C_phys_0", 3.0 * kGb},
            {"pid_10_luid_0x00000000_0x0000F02F_phys_0", 0.5 * kGb},
            {"pid_11_luid_0x00000000_0x0000F45C_phys_0", 1.0 * kGb},
        };
        QCOMPARE(videoMemoryFor(10, memory), quint64(3.5 * kGb));
        QCOMPARE(videoMemoryFor(12, memory), quint64(0));
    }

    // --- CPU -----------------------------------------------------------------

    void cpuPercentIsOfAllCores()
    {
        // 2 s of CPU time in 1 s on 8 cores = 25 %
        QCOMPARE(cpuPercent(20'000'000, 10'000'000, 8), 25.0);
        QCOMPARE(cpuPercent(0, 10'000'000, 8), 0.0);
        QCOMPARE(cpuPercent(10'000'000, 0, 8), 0.0);
        QCOMPARE(cpuPercent(200'000'000, 10'000'000, 8), 100.0);
    }

    // --- frame rate ----------------------------------------------------------

    void steadySixtyFps()
    {
        const auto p = presents(0, 121, 1000.0 / 60);   // 2 s
        const FrameRate r = frameRate(p, p.back(), 1000 * kMs);
        QVERIFY(r.valid);
        QVERIFY(qAbs(r.fps - 60.0) < 0.5);
        QVERIFY(qAbs(r.low1Fps - 60.0) < 0.5);
        QVERIFY(qAbs(r.worstFrameMs - 16.67) < 0.1);
    }

    void oneLongFrameIsTheOnePercentLow()
    {
        auto p = presents(0, 100, 10.0);       // 100 fps
        p.push_back(p.back() + 100 * kMs);     // one 100 ms hitch
        const FrameRate r = frameRate(p, p.back(), 5000 * kMs);
        QVERIFY(r.valid);
        QVERIFY(qAbs(r.low1Fps - 10.0) < 0.01);
        QVERIFY(qAbs(r.worstFrameMs - 100.0) < 0.01);
        QVERIFY(r.fps < 100.0 && r.fps > 50.0);
    }

    void windowEndsAtTheNewestPresent()
    {
        // events arrive late: "now" is 900 ms after the last present, the rate is still known
        const auto p = presents(0, 61, 1000.0 / 60);
        const FrameRate r = frameRate(p, p.back() + 900 * kMs, 1000 * kMs);
        QVERIFY(r.valid);
        QVERIFY(qAbs(r.fps - 60.0) < 0.5);
    }

    void noRateWhenStaleOrTooFew()
    {
        const auto p = presents(0, 61, 1000.0 / 60);
        QVERIFY(!frameRate(p, p.back() + 5000 * kMs, 1000 * kMs).valid);
        QVERIFY(!frameRate({100}, 100, 1000 * kMs).valid);
        QVERIFY(!frameRate({}, 0, 1000 * kMs).valid);
    }

    // --- warnings ------------------------------------------------------------

    void dedicatedAdapterNeedsRealVideoMemory()
    {
        const auto adapters = laptopAdapters();
        QCOMPARE(dedicatedAdapter(adapters)->luid, kNvidiaLuid);
        QVERIFY(dedicatedAdapter({{1, "Intel(R) Iris(R) Xe Graphics", 128ull * 1024 * 1024}}) == nullptr);
        QVERIFY(dedicatedAdapter({}) == nullptr);
    }

    void batteryAndIntegratedGpuAreWarnings()
    {
        GameStats s;
        s.running = true;
        s.power.known = true;
        s.power.onBattery = true;
        s.gpu = 98;
        s.gpuName = "Intel(R) UHD Graphics";
        const auto w = warningsFor(s, laptopAdapters());
        QCOMPARE(w.size(), size_t(2));
        QCOMPARE(w[0].text, QString("On battery"));
        QCOMPARE(w[1].text, QString("Integrated GPU"));
        QVERIFY(w[1].detail.contains("NVIDIA GeForce GTX 1650"));
    }

    void noGpuWarningWhileTheGpuIsIdle()
    {
        GameStats s;
        s.running = true;
        s.gpu = 3;   // a splash screen on the integrated GPU
        s.gpuName = "Intel(R) UHD Graphics";
        QVERIFY(warningsFor(s, laptopAdapters()).empty());
    }

    void noWarningsOnTheDedicatedGpuOnAc()
    {
        GameStats s;
        s.running = true;
        s.power.known = true;
        s.gpu = 98;
        s.gpuName = "NVIDIA GeForce GTX 1650";
        s.systemCpu = 60;
        QVERIFY(warningsFor(s, laptopAdapters()).empty());
    }

    void powerModeAndBusyCpuAreWarnings()
    {
        GameStats s;
        s.running = true;
        s.power.known = true;
        s.power.batterySaver = true;
        s.power.efficiencyMode = true;
        s.power.mode = "Best power efficiency";
        s.systemCpu = 99;
        const auto w = warningsFor(s, {});
        QCOMPARE(w.size(), size_t(3));
        QCOMPARE(w[0].text, QString("Battery saver on"));
        QCOMPARE(w[1].text, QString("Power-saving mode"));
        QVERIFY(w[1].detail.contains("Best power efficiency"));
        QCOMPARE(w[2].text, QString("CPU fully used"));
    }

    void noWarningsWithoutAGame()
    {
        GameStats s;
        s.power.known = true;
        s.power.onBattery = true;
        QVERIFY(warningsFor(s, laptopAdapters()).empty());
    }

    // --- text ----------------------------------------------------------------

    void shortAdapterNames()
    {
        QCOMPARE(shortAdapterName("NVIDIA GeForce GTX 1650"), QString("GeForce GTX 1650"));
        QCOMPARE(shortAdapterName("Intel(R) UHD Graphics"), QString("UHD Graphics"));
        QCOMPARE(shortAdapterName("AMD Radeon 780M Graphics"), QString("Radeon 780M Graphics"));
        QCOMPARE(shortAdapterName("Microsoft Basic Render Driver"), QString("Microsoft Basic Render Driver"));
    }

    void formatsBytes()
    {
        QCOMPARE(formatBytes(820ull * 1024 * 1024), QString("820 MB"));
        QCOMPARE(formatBytes(quint64(6.1 * kGb)), QString("6.1 GB"));
        QCOMPARE(formatBytes(0), QString("0 MB"));
    }

    void summaryLineForTheLog()
    {
        GameStats s;
        QCOMPARE(summaryLine(s), QString("game not running"));
        s.running = true;
        s.fps = {true, 58.2, 41.4, 35.0};
        s.cpu = 23;
        s.systemCpu = 40;
        s.gpu = 97;
        s.gpuName = "NVIDIA GeForce GTX 1650";
        s.ram = quint64(6.1 * kGb);
        s.power = {true, true, 72, false, "High performance", {}, false};
        QCOMPARE(summaryLine(s),
                 QString("FPS 58 (1% low 41, worst frame 35.0 ms), CPU 23 % (system 40 %), "
                         "GPU 97 % on NVIDIA GeForce GTX 1650, RAM 6.1 GB, "
                         "on battery (battery 72 %), plan High performance"));
        s.fps = {};
        s.fpsSource = FpsSource::NoPermission;
        QVERIFY(summaryLine(s).startsWith("FPS n/a (needs administrator rights), CPU 23 %"));
    }

    // --- Windows' graphics setting --------------------------------------------

    void readsGpuPreference()
    {
        QCOMPARE(gpuPreferenceFrom("GpuPreference=2;"), GpuPreference::HighPerformance);
        QCOMPARE(gpuPreferenceFrom("AppStatus=4096;GpuPreference=1;"), GpuPreference::PowerSaving);
        QCOMPARE(gpuPreferenceFrom("SwapEffectUpgradeEnable=1;gpupreference = 2 ;"), GpuPreference::HighPerformance);
        QCOMPARE(gpuPreferenceFrom("AppStatus=4096;"), GpuPreference::Auto);
        QCOMPARE(gpuPreferenceFrom("GpuPreference=7;"), GpuPreference::Auto);
        QCOMPARE(gpuPreferenceFrom({}), GpuPreference::Auto);
    }

    void setsGpuPreferenceKeepingOtherEntries()
    {
        QCOMPARE(withGpuPreference("AppStatus=4096;", GpuPreference::HighPerformance),
                 QString("AppStatus=4096;GpuPreference=2;"));
        QCOMPARE(withGpuPreference("GpuPreference=2;SwapEffectUpgradeEnable=1;", GpuPreference::PowerSaving),
                 QString("SwapEffectUpgradeEnable=1;GpuPreference=1;"));
        QCOMPARE(withGpuPreference({}, GpuPreference::Auto), QString("GpuPreference=0;"));
        QCOMPARE(gpuPreferenceFrom(withGpuPreference("AppStatus=4096", GpuPreference::HighPerformance)),
                 GpuPreference::HighPerformance);
    }

    void compactLineForTheFloatingBar()
    {
        GameStats s;
        QCOMPARE(compactLine(s), QString("Game not running"));
        s.running = true;
        QCOMPARE(compactLine(s), QString("- FPS"));
        s.fps = {true, 58.4, 40, 30};
        s.cpu = 23.4;
        s.gpu = 97;
        QCOMPARE(compactLine(s), QString("58 FPS · CPU 23 % · GPU 97 %"));
    }

    // --- graphics driver -----------------------------------------------------

    void matchesTheDriverToTheCard()
    {
        QVERIFY(Driver::matchesDevice("pci\\ven_10de&dev_1f99&subsys_86681558", 0x10DE, 0x1F99));
        QVERIFY(Driver::matchesDevice("PCI\\VEN_8086&DEV_9BC4", 0x8086, 0x9BC4));
        QVERIFY(!Driver::matchesDevice("PCI\\VEN_8086&DEV_9BC4", 0x8086, 0x9BC5));
        QVERIFY(!Driver::matchesDevice("ROOT\\BasicDisplay", 0x8086, 0x9BC4));
    }

    void driverDatesAndVersions()
    {
        QCOMPARE(Driver::parseDriverDate("9-30-2026"), QDate(2026, 9, 30));
        QVERIFY(!Driver::parseDriverDate("2026").isValid());
        QCOMPARE(Driver::nvidiaVersion("32.0.16.1742"), QString("617.42"));
        QCOMPARE(Driver::nvidiaVersion("32.0.15.6094"), QString("560.94"));
        QCOMPARE(Driver::nvidiaVersion("31.0.15.0901"), QString("509.01"));
        QCOMPARE(Driver::displayVersion(Driver::kIntel, "31.0.101.2145"), QString("31.0.101.2145"));
        QCOMPARE(Driver::compareVersions("610.47", "617.42"), -1);
        QCOMPARE(Driver::compareVersions("617.42", "617.42"), 0);
        QCOMPARE(Driver::compareVersions("617.5", "617.42"), -1);
        QCOMPARE(Driver::compareVersions("618", "617.42"), 1);
    }

    void nvidiaProductNames()
    {
        QCOMPARE(Driver::nvidiaProduct("NVIDIA GeForce GTX 1650"), QString("GeForce GTX 1650"));
        QCOMPARE(Driver::nvidiaProduct("NVIDIA GeForce RTX 4060 Laptop GPU"), QString("GeForce RTX 4060"));
        QCOMPARE(Driver::nvidiaProduct("NVIDIA GeForce RTX 2070 with Max-Q Design"), QString("GeForce RTX 2070"));
    }

    void findsNvidiaIdsForDesktopAndNotebook()
    {
        // trimmed from lookupValueSearch.aspx?TypeID=2 / 3
        const QByteArray series =
            "<?xml version=\"1.0\"?><LookupValueSearch><LookupValues>"
            "<LookupValue RequiresProduct=\"True\" ParentID=\"1\"><Name>GeForce GTX 16 Series (Notebooks)</Name><Value>115</Value></LookupValue>"
            "<LookupValue RequiresProduct=\"True\" ParentID=\"1\"><Name>GeForce 16 Series</Name><Value>112</Value></LookupValue>"
            "</LookupValues></LookupValueSearch>";
        const QByteArray products =
            "<?xml version=\"1.0\"?><LookupValueSearch><LookupValues>"
            "<LookupValue ParentID=\"112\"><Name>GeForce GTX 1650 SUPER</Name><Value>895</Value></LookupValue>"
            "<LookupValue ParentID=\"112\"><Name>GeForce GTX 1650</Name><Value>897</Value></LookupValue>"
            "<LookupValue ParentID=\"115\"><Name>GeForce GTX 1650</Name><Value>898</Value></LookupValue>"
            "<LookupValue ParentID=\"115\"><Name>GeForce GTX 1660 Ti</Name><Value>899</Value></LookupValue>"
            "</LookupValues></LookupValueSearch>";
        const auto notebook = Driver::findNvidiaIds(series, products, "GeForce GTX 1650", true);
        QVERIFY(notebook.has_value());
        QCOMPARE(notebook->series, 115);
        QCOMPARE(notebook->product, 898);
        const auto desktop = Driver::findNvidiaIds(series, products, "geforce gtx 1650", false);
        QCOMPARE(desktop->product, 897);
        // only listed for desktops: still found
        QCOMPARE(Driver::findNvidiaIds(series, products, "GeForce GTX 1650 SUPER", true)->product, 895);
        QVERIFY(!Driver::findNvidiaIds(series, products, "GeForce RTX 9999", false).has_value());
        QVERIFY(Driver::nvidiaLookupUrl({115, 898}).contains("psid=115&pfid=898"));
    }

    void parsesNvidiaLookup()
    {
        const QByteArray json =
            R"({ "Success" : "1", "IDS" : [{ "downloadInfo": { "Success" : "1", "ID" : "280104",
                 "Name" : "GeForce%20Game%20Ready%20Driver", "Version" : "617.42", "IsBeta" : "0",
                 "ReleaseDateTime" : "Tue Oct 06, 2026",
                 "DetailsURL" : "https://www.nvidia.com/en-us/drivers/details/280104/" } }] })";
        const auto latest = Driver::parseNvidiaLookup(json);
        QVERIFY(latest.has_value());
        QCOMPARE(latest->version, QString("617.42"));
        QCOMPARE(latest->date, QDate(2026, 10, 6));
        QCOMPARE(latest->name, QString("GeForce Game Ready Driver"));
        QCOMPARE(latest->detailsUrl, QString("https://www.nvidia.com/en-us/drivers/details/280104/"));
        QVERIFY(!Driver::parseNvidiaLookup(R"({ "Success" : "0", "IDS" : [] })").has_value());
        QVERIFY(!Driver::parseNvidiaLookup("not json").has_value());
    }

    // --- sampler -------------------------------------------------------------

    void samplerNeedsTwoSamplesForCpu()
    {
        Sampler sampler;
        const auto adapters = laptopAdapters();
        RawSample first = alive(0, 0);
        GameStats s = sampler.update(10, first, adapters);
        QVERIFY(s.running);
        QVERIFY(!s.cpu.has_value());
        QCOMPARE(*s.ram, 6 * kGb);

        RawSample second = alive(1000 * kMs, 20'000'000);   // 2 s CPU in 1 s on 8 cores
        second.systemTotal100ns = 80'000'000;
        second.systemBusy100ns  = 40'000'000;
        second.gpuEngines = {{engine(10, kNvidiaLuid, 0, "3D"), 90}};
        s = sampler.update(10, second, adapters);
        QCOMPARE(*s.cpu, 25.0);
        QCOMPARE(*s.systemCpu, 50.0);
        QCOMPARE(*s.gpu, 90.0);
        QCOMPARE(s.gpuName, QString("NVIDIA GeForce GTX 1650"));
        QVERIFY(!s.magpieGpu.has_value());   // no Magpie running

        // Magpie's FSR work is in its own process: read from the same counters, kept apart from the game's
        RawSample third = alive(2000 * kMs, 30'000'000);
        third.gpuEngines = {{engine(10, kNvidiaLuid, 0, "3D"), 60}, {engine(77, kNvidiaLuid, 0, "3D"), 7}};
        s = sampler.update(10, third, adapters, 77);
        QCOMPARE(*s.gpu, 60.0);
        QCOMPARE(*s.magpieGpu, 7.0);
        QVERIFY(summaryLine(s).contains("GPU 60 % on NVIDIA GeForce GTX 1650 + Magpie 7 %"));
    }

    void samplerCollectsPresentsAcrossSamples()
    {
        Sampler sampler;
        const auto all = presents(0, 121, 1000.0 / 60);
        RawSample a = alive(1000 * kMs, 0);
        a.presentsNs.assign(all.begin(), all.begin() + 61);
        RawSample b = alive(2000 * kMs, 0);
        b.presentsNs.assign(all.begin() + 61, all.end());
        std::reverse(b.presentsNs.begin(), b.presentsNs.end());   // batches may come out of order
        sampler.update(10, a, {});
        const GameStats s = sampler.update(10, b, {});
        QVERIFY(s.fps.valid);
        QVERIFY(qAbs(s.fps.fps - 60.0) < 0.5);
    }

    void samplerStartsOverForAnotherProcess()
    {
        Sampler sampler;
        sampler.update(10, alive(0, 0), {});
        const GameStats s = sampler.update(11, alive(1000 * kMs, 50'000'000), {});
        QVERIFY(!s.cpu.has_value());   // not a delta against process 10
    }

    void samplerReportsAnExitedGame()
    {
        Sampler sampler;
        sampler.update(10, alive(0, 0), {});
        RawSample gone;
        gone.power.known = true;
        gone.power.onBattery = true;
        const GameStats s = sampler.update(10, gone, laptopAdapters());
        QVERIFY(!s.running);
        QVERIFY(s.power.onBattery);
        QVERIFY(s.warnings.empty());
        QCOMPARE(sampler.update(0, gone, {}).running, false);
    }

    // --- this PC (read-only) -------------------------------------------------

    void liveSampleOfThisProcess()
    {
        StatsBackend &backend = statsBackend();
        if (!backend.supported())
            QSKIP("No stats backend on this platform");
        for (const auto &a : backend.adapters())
            qInfo().noquote() << "adapter" << a.name << QString::number(a.luid, 16)
                              << formatBytes(a.dedicatedBytes) << "driver"
                              << Driver::displayVersion(a.vendorId, a.driverVersion) << a.driverDate.toString(Qt::ISODate);
        for (const auto &plan : backend.powerPlans())
            qInfo().noquote() << "power plan" << plan.name << plan.id << (plan.active ? "(active)" : "");

        const qint64 pid = QCoreApplication::applicationPid();
        Sampler sampler;
        sampler.update(pid, backend.sample(pid), backend.adapters());
        QThread::msleep(300);
        const GameStats s = sampler.update(pid, backend.sample(pid), backend.adapters());
        backend.shutdown();
        for (auto p : {GpuPreference::Auto, GpuPreference::PowerSaving, GpuPreference::HighPerformance})
            qInfo().noquote() << gpuPreferenceName(p) << "->" << backend.adapterFor(p);
        QVERIFY(s.running);
        QVERIFY(s.cpu.has_value());
        QVERIFY(*s.ram > 0);
        qInfo().noquote() << summaryLine(s);
        qInfo().noquote() << "frame counter:" << int(s.fpsSource) << s.fpsError;
    }
};

QTEST_GUILESS_MAIN(TstStats)
#include "tst_stats.moc"
