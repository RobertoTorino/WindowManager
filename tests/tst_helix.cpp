#include <QtTest>

#include "helix/HelixGame.h"
#include "helix/HelixPaths.h"
#include "helix/HelixSystem.h"
#include "helix/SystemProbe.h"
#include "helix/HelixRelease.h"

using namespace Helix;

namespace {

// JSON written with single quotes so the sources stay readable; they become double quotes here.
QByteArray json(const char* text)
{
    return QByteArray(text).replace('\'', '"');
}

QByteArray releasesJson()
{
    return json("[\n"
"      {'tag_name':'v1.9.2','prerelease':true,'draft':false,'published_at':'2026-10-16T13:59:27Z',\n"
"       'assets':[{'name':'HelixSR-1.9.2.zip','size':10,'digest':'sha256:aaaa',\n"
"                  'browser_download_url':'https://example.test/1.9.2.zip'}]},\n"
"      {'tag_name':'v1.9.3','prerelease':false,'draft':false,'published_at':'2026-10-16T14:22:56Z',\n"
"       'assets':[{'name':'HelixSR-Scaler-1.0.6.zip','size':5,'browser_download_url':'https://example.test/scaler.zip'},\n"
"                 {'name':'HelixSR-1.9.3.zip','size':136076974,\n"
"                  'digest':'sha256:7094B7AFD006AB56231C880FCD358F115B114EE0734BAA19D497A3E42DA0A1D1',\n"
"                  'browser_download_url':'https://example.test/1.9.3.zip'},\n"
"                 {'name':'SHA256SUMS.txt','size':578,'browser_download_url':'https://example.test/sums.txt'}]},\n"
"      {'tag_name':'v1.9.0','prerelease':false,'draft':false,\n"
"       'assets':[{'name':'HelixSR-1.9.0.zip','size':3,'browser_download_url':'https://example.test/1.9.0.zip'}]}\n"
"    ]");
}

void touch(const QString& path, const QByteArray& content = "x")
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(content);
}

QByteArray helixDll(const QString& version)
{
    return QByteArray("MZ\0\0 some code ", 16) + QByteArray("\0HelixSR ", 9) + version.toLatin1() + '\0';
}

GpuInfo gpu(const char* name, quint32 vendor, int shaderModel, bool native16, int waveMin, int waveMax)
{
    GpuInfo g;
    g.name = name;
    g.vendorId = vendor;
    g.videoMemoryMB = 8192;
    g.d3d12 = true;
    g.shaderModel = shaderModel;
    g.native16Bit = native16;
    g.waveMin = waveMin;
    g.waveMax = waveMax;
    return g;
}

SystemInfo pcWith(const QList<GpuInfo>& gpus)
{
    SystemInfo s;
    s.gpus = gpus;
    s.freeDiskMB = 100000;
    return s;
}

} // namespace

class TstHelix : public QObject
{
    Q_OBJECT
private slots:
    void parseVersions()
    {
        QCOMPARE(parseVersion("1.9.3")->toString(), QString("1.9.3"));
        QCOMPARE(parseVersion("v1.10.0")->toString(), QString("1.10.0"));
        QCOMPARE(parseVersion("HelixSR 2.0.1")->toString(), QString("2.0.1"));
        QVERIFY(!parseVersion("latest").has_value());
        QVERIFY(!parseVersion("1.9").has_value());
    }

    void versionOrdering()
    {
        // numeric, not textual: 1.10.0 is newer than 1.9.3
        QVERIFY(*parseVersion("1.10.0") > *parseVersion("1.9.3"));
        QVERIFY(*parseVersion("1.9.3") > *parseVersion("1.9.2"));
        QVERIFY(*parseVersion("2.0.0") > *parseVersion("1.99.99"));
    }

    void updateDecision()
    {
        const Version latest = *parseVersion("1.9.3");
        QVERIFY(isUpdate(std::nullopt, latest));
        QVERIFY(isUpdate(parseVersion("1.9.2"), latest));
        QVERIFY(!isUpdate(parseVersion("1.9.3"), latest));
        QVERIFY(!isUpdate(parseVersion("1.9.4"), latest));
    }

    void latestSkipsPrereleasesAndPicksTheZip()
    {
        const auto r = latestRelease(releasesJson());
        QVERIFY(r.has_value());
        QCOMPARE(r->version.toString(), QString("1.9.3"));
        QCOMPARE(r->zipName, QString("HelixSR-1.9.3.zip"));       // not the Scaler zip
        QCOMPARE(r->zipUrl, QString("https://example.test/1.9.3.zip"));
        QCOMPARE(r->zipSize, qint64(136076974));
        QCOMPARE(r->sha256, QString("7094b7afd006ab56231c880fcd358f115b114ee0734baa19d497a3e42da0a1d1"));
        QCOMPARE(r->sumsUrl, QString("https://example.test/sums.txt"));
    }

    void latestHandlesBadInput()
    {
        QVERIFY(!latestRelease("not json").has_value());
        QVERIFY(!latestRelease("{}").has_value());
        QVERIFY(!latestRelease("[]").has_value());
        // only a prerelease: nothing qualifies
        QVERIFY(!latestRelease(json("[{'tag_name':'v1','prerelease':true,\n"
"            'assets':[{'name':'HelixSR-1.0.0.zip','browser_download_url':'u'}]}]")).has_value());
    }

    void sumsLookup()
    {
        const QString h1(64, 'a');
        const QString h2(64, 'B');
        const QString text = h1 + "  HelixSR-1.9.3.zip\n" + h2 + " *other.zip\n\n";
        QCOMPARE(hashFromSums(text, "HelixSR-1.9.3.zip"), h1);
        QCOMPARE(hashFromSums(text, "other.zip"), h2.toLower());
        QVERIFY(hashFromSums(text, "missing.zip").isEmpty());
        QVERIFY(hashFromSums("short  HelixSR-1.9.3.zip", "HelixSR-1.9.3.zip").isEmpty());
    }

    void versionInsideDll()
    {
        QCOMPARE(versionInDll(helixDll("1.9.3"))->toString(), QString("1.9.3"));
        QVERIFY(!versionInDll("MZ just the game's own FSR dll").has_value());
    }

    void gameRoot()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString base = dir.path() + "/steamapps/common";
        QCOMPARE(gameRootFor(base + "/Game/game.exe"), base + "/Game");
        QCOMPARE(gameRootFor(base + "/Game/bin/x64/game.exe"), base + "/Game");
        // Unreal Engine: <game>/<project>/Binaries/Win64
        QDir().mkpath(base + "/Ue/Proj/Binaries/Win64");
        QCOMPARE(gameRootFor(base + "/Ue/Proj/Binaries/Win64/Proj-Win64-Shipping.exe"), base + "/Ue");
        // never climbs into the library folder
        QCOMPARE(gameRootFor(base + "/bin/game.exe"), base + "/bin");
    }

    void unsupportedWithoutFsrDll()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/nvngx_dlss.dll"));
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.status, GameScan::Status::Unsupported);
        QVERIFY(scan.targets.isEmpty());
    }

    void readyWithGameFsrDll()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.dll"), "MZ the game's own");
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.status, GameScan::Status::Ready);
        QCOMPARE(scan.targets.size(), 1);
        QVERIFY(scan.targets[0].endsWith("amd_fidelityfx_dx12.dll"));
    }

    void upscalerDllWinsOverGenericOneInTheSameFolder()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.dll"));
        touch(dir.filePath("Game/amd_fidelityfx_upscaler_dx12.dll"));
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.targets.size(), 1);
        QVERIFY(scan.targets[0].endsWith("amd_fidelityfx_upscaler_dx12.dll"));
    }

    void findsDllsInSubfoldersAndIgnoresBackups()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/Plugins/FSR/amd_fidelityfx_upscaler_dx12.dll"));
        touch(dir.filePath("Game/Plugins/FSR/amd_fidelityfx_upscaler_dx12.original.dll"));
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.targets.size(), 1);
        QVERIFY(!scan.targets[0].contains(".original."));
    }

    void installedWhenTheDllIsHelixSR()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.dll"), helixDll("1.9.3"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.original.dll"), "MZ game's own");
        touch(dir.filePath("Game/helixsr-install.json"),
              json("{'helixsr':'1.9.3','files':['amd_fidelityfx_dx12.dll'],'route':'direct'}"));
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.status, GameScan::Status::Installed);
        QCOMPARE(scan.installedVersion->toString(), QString("1.9.3"));
    }

    void replacedWhenTheGameRestoredItsDll()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.dll"), "MZ game's new own");
        touch(dir.filePath("Game/amd_fidelityfx_dx12.original.dll"), "MZ game's old own");
        touch(dir.filePath("Game/helixsr-install.json"),
              json("{'helixsr':'1.9.3','files':['amd_fidelityfx_dx12.dll'],'route':'direct'}"));
        QCOMPARE(scanGame(dir.filePath("Game/game.exe")).status, GameScan::Status::Replaced);
    }

    void aHelixDllWithoutRecordIsStillInstalled()
    {
        QTemporaryDir dir;
        touch(dir.filePath("Game/game.exe"));
        touch(dir.filePath("Game/amd_fidelityfx_dx12.dll"), helixDll("1.8.1"));
        const auto scan = scanGame(dir.filePath("Game/game.exe"));
        QCOMPARE(scan.status, GameScan::Status::Installed);
        QVERIFY(!scan.installedVersion.has_value());
    }

    void builtPackageIsTheNewestFinishedOne()
    {
        QTemporaryDir dir;
        QVERIFY(!builtPackage(dir.path()).has_value());

        QDir().mkpath(dir.filePath("HelixSR-1.9.2"));
        QDir().mkpath(dir.filePath("HelixSR-1.9.3"));   // extracted but never finished building
        QDir().mkpath(dir.filePath("HelixSR-1.10.0"));
        QVERIFY(markBuilt(dir.filePath("HelixSR-1.9.2"), *parseVersion("1.9.2")));
        QVERIFY(markBuilt(dir.filePath("HelixSR-1.10.0"), *parseVersion("1.10.0")));

        const auto pkg = builtPackage(dir.path());
        QVERIFY(pkg.has_value());
        QCOMPARE(pkg->version.toString(), QString("1.10.0"));
        QVERIFY(pkg->dir.endsWith("HelixSR-1.10.0"));

        const QStringList others = otherPackages(dir.path(), pkg->dir);
        QCOMPARE(others.size(), 2);
        QVERIFY(!others.join('|').contains("1.10.0"));
    }

    void leftoversAreOnlyBuildToolsAndExistingOnly()
    {
        QTemporaryDir pkg, setup;
        touch(pkg.filePath("amd_fidelityfx_dx12.dll"));
        touch(pkg.filePath("setup/helixsr_install.py"));
        touch(pkg.filePath("helixsr-scaler/OptiScaler.dll"));
        touch(pkg.filePath("manual/x.dll"));
        touch(setup.filePath("python/python.exe"));
        touch(setup.filePath("python/numpy/__init__.py"));
        touch(setup.filePath("python/numpy.libs/a.dll"));
        touch(setup.filePath("python/numpy-2.5.3.dist-info/METADATA"));
        touch(setup.filePath("dxc/dxc.exe"));

        const QStringList gone = buildLeftovers(pkg.path(), setup.path());
        const QString joined = gone.join('|');
        for (const char* name : {"helixsr-scaler", "manual", "/dxc", "python/numpy", "numpy.libs", "numpy-2.5.3.dist-info"})
            QVERIFY2(joined.contains(name), name);
        // what the game install needs stays
        QVERIFY(!joined.contains("python.exe"));
        QVERIFY(!joined.contains("setup/helixsr_install.py"));
        QVERIFY(!joined.contains("amd_fidelityfx_dx12.dll"));
        // nothing listed that does not exist (no dxc.zip, python.zip, ...)
        for (const QString& g : gone)
            QVERIFY2(QFileInfo::exists(g), qPrintable(g));
    }

    void gpuFamiliesFromNames()
    {
        auto family = [](const char* name, quint32 vendor = 0x1002, int wave = 0) {
            GpuInfo g;
            g.name = name;
            g.vendorId = vendor;
            g.waveMin = wave;
            return familyOf(g);
        };
        QCOMPARE(family("AMD Radeon RX 5700 XT"), Family::Rdna1);
        QCOMPARE(family("AMD Radeon RX 6800 XT"), Family::Rdna2);
        QCOMPARE(family("AMD Radeon RX 7900 XTX"), Family::Rdna3);
        QCOMPARE(family("AMD Radeon 780M Graphics"), Family::Rdna3);
        QCOMPARE(family("AMD Radeon RX 9070 XT"), Family::Rdna4);
        QCOMPARE(family("AMD Radeon RX Vega 64"), Family::Vega);
        QCOMPARE(family("AMD Radeon VII"), Family::Vega);
        // three-digit RX cards are Polaris, not RX 5000
        QCOMPARE(family("AMD Radeon RX 580 Series"), Family::Gcn);
        // a generic name is told apart by SIMD width
        QCOMPARE(family("AMD Radeon(TM) Graphics", 0x1002, 32), Family::RdnaOther);
        QCOMPARE(family("AMD Radeon(TM) Graphics", 0x1002, 64), Family::Gcn);
        QCOMPARE(family("NVIDIA GeForce GTX 1080", 0x10DE), Family::Nvidia);
        QCOMPARE(family("Intel(R) Arc(TM) A770", 0x8086), Family::Intel);
    }

    void rdnaCardWithEverythingIsSupported()
    {
        const auto v = evaluate(pcWith({gpu("AMD Radeon RX 7900 XTX", 0x1002, 66, true, 32, 64)}));
        QCOMPARE(v.level, Level::Supported);
        QCOMPARE(v.gpuName, QString("AMD Radeon RX 7900 XTX"));
        QVERIFY(v.summary.contains("can run"));
    }

    void oldShaderModelBlocks()
    {
        const auto v = evaluate(pcWith({gpu("AMD Radeon RX 6600", 0x1002, 65, true, 32, 64)}));
        QCOMPARE(v.level, Level::Unsupported);
        QVERIFY(v.summary.contains("6.6"));
    }

    void vegaAndNvidiaAreExperimental()
    {
        QCOMPARE(evaluate(pcWith({gpu("AMD Radeon RX Vega 56", 0x1002, 66, true, 64, 64)})).level, Level::Experimental);
        QCOMPARE(evaluate(pcWith({gpu("NVIDIA GeForce GTX 1070", 0x10DE, 66, true, 32, 32)})).level, Level::Experimental);
    }

    void nvidiaMessageDependsOnDlssSupport()
    {
        const auto gtx = evaluate(pcWith({gpu("NVIDIA GeForce GTX 1650", 0x10DE, 68, true, 32, 32)}));
        QCOMPARE(gtx.level, Level::Experimental);
        QVERIFY2(gtx.summary.contains("no DLSS"), qPrintable(gtx.summary));
        const auto rtx = evaluate(pcWith({gpu("NVIDIA GeForce RTX 4070", 0x10DE, 68, true, 32, 32)}));
        QCOMPARE(rtx.level, Level::Experimental);
        QVERIFY2(rtx.summary.contains("own DLSS"), qPrintable(rtx.summary));
    }

    void noD3d12IsUnsupported()
    {
        GpuInfo g;
        g.name = "Old Card";
        g.vendorId = 0x1002;
        g.error = "D3D12CreateDevice failed";
        QCOMPARE(evaluate(pcWith({g})).level, Level::Unsupported);
    }

    void softwareAdaptersAreIgnoredAndTheBestCardWins()
    {
        GpuInfo warp = gpu("Microsoft Basic Render Driver", 0x1414, 66, true, 4, 128);
        warp.software = true;
        const auto v = evaluate(pcWith({warp, gpu("AMD Radeon RX 6700 XT", 0x1002, 66, true, 32, 64),
                                        gpu("AMD Radeon(TM) Graphics", 0x1002, 66, true, 64, 64)}));
        QCOMPARE(v.level, Level::Supported);
        QCOMPARE(v.gpuName, QString("AMD Radeon RX 6700 XT"));

        // only a software adapter: nothing to run on
        QCOMPARE(evaluate(pcWith({warp})).level, Level::Unsupported);
        QCOMPARE(evaluate(pcWith({})).level, Level::Unsupported);
    }

    void machineChecksCanBlockAGoodCard()
    {
        auto s = pcWith({gpu("AMD Radeon RX 7800 XT", 0x1002, 66, true, 32, 64)});
        s.freeDiskMB = 200;
        auto v = evaluate(s);
        QCOMPARE(v.level, Level::Unsupported);
        QVERIFY(v.summary.contains("disk"));

        s.freeDiskMB = 50000;
        s.hasTar = false;
        QCOMPARE(evaluate(s).level, Level::Unsupported);

        s.hasTar = true;
        s.hasPowerShell = false;
        QCOMPARE(evaluate(s).level, Level::Unsupported);

        s.hasPowerShell = true;
        QCOMPARE(evaluate(s).level, Level::Supported);
    }

    void noNative16BitIsOnlyExperimental()
    {
        QCOMPARE(evaluate(pcWith({gpu("AMD Radeon RX 5500 XT", 0x1002, 66, false, 32, 64)})).level, Level::Experimental);
    }

    // Not an assertion about the hardware: it prints what this PC reports, for whoever runs the tests.
    void liveProbeReportsThisMachine()
    {
        const SystemInfo info = probeSystem(QDir::tempPath());
        for (const GpuInfo& g : info.gpus) {
            qInfo().noquote() << QString("adapter: %1 | vendor 0x%2 | %3 MB | d3d12=%4 | SM %5 | native16=%6 | wave %7-%8 %9")
                                     .arg(g.name).arg(g.vendorId, 4, 16, QChar('0')).arg(g.videoMemoryMB).arg(g.d3d12)
                                     .arg(g.shaderModel).arg(g.native16Bit).arg(g.waveMin).arg(g.waveMax).arg(g.error);
        }
        const Verdict v = evaluate(info);
        qInfo().noquote() << "verdict:" << levelName(v.level) << "-" << v.summary;
        qInfo().noquote() << v.details();
        QVERIFY(!v.summary.isEmpty());
    }
};

QTEST_MAIN(TstHelix)
#include "tst_helix.moc"
