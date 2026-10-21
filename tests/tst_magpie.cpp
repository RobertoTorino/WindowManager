#include <QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "magpie/MagpieConfig.h"
#include "magpie/MagpieRelease.h"

using namespace Magpie;

namespace {

// JSON written with single quotes so the sources stay readable; they become double quotes here.
QByteArray json(const char* text)
{
    return QByteArray(text).replace('\'', '"');
}

// Shaped like the config Magpie 0.13.0 writes on its first start (trimmed), plus a profile made in
// Magpie by the user and a big integer like updateCheckDate's tick count.
QByteArray magpieConfig()
{
    return json(
"{'language': '', 'theme': 2, 'updateCheckDate': 134055000000000123, 'autoCheckForUpdates': true,\n"
" 'shortcuts': {'scale': 3137, 'windowedModeScale': 3153},\n"
" 'scalingModes': [\n"
"   {'name': 'Lanczos', 'effects': [{'name': 'Lanczos', 'scalingType': 1}]},\n"
"   {'name': 'FSR', 'effects': [{'name': 'FSR\\\\FSR_EASU', 'scalingType': 1},\n"
"                               {'name': 'FSR\\\\FSR_RCAS', 'parameters': {'sharpness': 0.8700000047683716}}]},\n"
"   {'name': 'Integer Scale 2x', 'effects': [{'name': 'Nearest', 'scale': {'x': 2.0, 'y': 2.0}}]}],\n"
" 'profiles': [\n"
"   {'scalingMode': 0, 'captureMethod': 0, 'multiMonitorUsage': 0, 'maxFrameRate': 60.0, '3DGameMode': false,\n"
"    'cursorScaling': 2, 'graphicsCardId': {'idx': -1, 'vendorId': 0, 'deviceId': 0}},\n"
"   {'name': 'Emulator (mine)', 'packaged': false, 'pathRule': 'C:\\\\Emu\\\\emu.exe', 'classNameRule': 'EmuWnd',\n"
"    'autoScale': 0, 'scalingMode': 2, 'captureMethod': 1}]}\n");
}

QJsonObject root(const QByteArray& config)
{
    return QJsonDocument::fromJson(config).object();
}

QJsonArray profilesOf(const QByteArray& config)
{
    return root(config)["profiles"].toArray();
}

GameProfile tekken()
{
    GameProfile g;
    g.name = "TEKKEN 7";
    g.exePath = "D:\\Games\\TEKKEN 7\\Binaries\\Win64\\TekkenGame-Win64-Shipping.exe";
    g.windowClass = "UnrealWindow";
    g.scalingMode = "FSR";
    return g;
}

} // namespace

class TstMagpie : public QObject
{
    Q_OBJECT
private slots:
    void readsScalingModesInOrder()
    {
        QCOMPARE(scalingModes(magpieConfig()), (QStringList{"Lanczos", "FSR", "Integer Scale 2x"}));
        QVERIFY(scalingModes("").isEmpty());
        QVERIFY(scalingModes("not json").isEmpty());
    }

    void addsAProfileFromTheDefaults()
    {
        const EditResult r = setGameProfile(magpieConfig(), tekken());
        QVERIFY2(r.ok, qPrintable(r.error));
        const QJsonArray profiles = profilesOf(r.config);
        QCOMPARE(profiles.size(), 3);
        const QJsonObject p = profiles[2].toObject();
        QCOMPARE(p["name"].toString(), QString("WindowManager: TEKKEN 7"));
        QCOMPARE(p["packaged"].toBool(true), false);
        QCOMPARE(p["pathRule"].toString(), tekken().exePath);
        QCOMPARE(p["classNameRule"].toString(), QString("UnrealWindow"));
        QCOMPARE(p["autoScale"].toInt(), 1);
        QCOMPARE(p["scalingMode"].toInt(), 1);   // FSR's index
        QCOMPARE(p["3DGameMode"].toBool(true), false);
        // Magpie's defaults come along (it reads these as numbers / objects)
        QCOMPARE(p["cursorScaling"].toInt(), 2);
        QCOMPARE(p["maxFrameRate"].toDouble(), 60.0);
        QCOMPARE(p["graphicsCardId"].toObject()["idx"].toInt(), -1);
        QVERIFY(p.contains("launcherPath") && p.contains("launchParameters"));
    }

    void keepsEverythingElse()
    {
        const EditResult r = setGameProfile(magpieConfig(), tekken());
        const QJsonObject before = root(magpieConfig());
        const QJsonObject after = root(r.config);
        // Magpie reads updateCheckDate as a 64-bit integer: it must stay one, exactly
        QVERIFY(after["updateCheckDate"].isDouble());
        QCOMPARE(after["updateCheckDate"].toInteger(), Q_INT64_C(134055000000000123));
        QVERIFY(r.config.contains("134055000000000123"));
        QCOMPARE(after["scalingModes"], before["scalingModes"]);
        QCOMPARE(after["shortcuts"], before["shortcuts"]);
        QCOMPARE(after["autoCheckForUpdates"], before["autoCheckForUpdates"]);
        QCOMPARE(profilesOf(r.config)[0], before["profiles"].toArray()[0]);   // the default profile
        QCOMPARE(profilesOf(r.config)[1], before["profiles"].toArray()[1]);   // the user's own
    }

    void updatesItsOwnProfileAndKeepsMagpieSideChanges()
    {
        QByteArray config = setGameProfile(magpieConfig(), tekken()).config;
        // the user changed the capture method in Magpie's window
        QJsonObject r = root(config);
        QJsonArray profiles = r["profiles"].toArray();
        QJsonObject p = profiles[2].toObject();
        p["captureMethod"] = 2;
        profiles[2] = p;
        r["profiles"] = profiles;
        config = QJsonDocument(r).toJson();

        GameProfile g = tekken();
        g.scalingMode = "integer scale 2X";   // names match without regard to case
        g.autoScale = AutoScale::Windowed;
        g.game3D = true;
        const EditResult second = setGameProfile(config, g);
        QVERIFY2(second.ok, qPrintable(second.error));
        const QJsonArray after = profilesOf(second.config);
        QCOMPARE(after.size(), 3);   // updated, not added
        QCOMPARE(after[2].toObject()["scalingMode"].toInt(), 2);
        QCOMPARE(after[2].toObject()["autoScale"].toInt(), 2);
        QCOMPARE(after[2].toObject()["3DGameMode"].toBool(), true);
        QCOMPARE(after[2].toObject()["captureMethod"].toInt(), 2);
    }

    void listsOnlyItsOwnProfiles()
    {
        const QByteArray config = setGameProfile(magpieConfig(), tekken()).config;
        const QList<GameProfile> games = gameProfiles(config);
        QCOMPARE(games.size(), 1);
        QCOMPARE(games[0].name, QString("TEKKEN 7"));
        QCOMPARE(games[0].exePath, tekken().exePath);
        QCOMPARE(games[0].windowClass, QString("UnrealWindow"));
        QCOMPARE(games[0].scalingMode, QString("FSR"));
        QCOMPARE(games[0].autoScale, AutoScale::FullScreen);
        QVERIFY(!games[0].game3D);
        QVERIFY(gameProfiles(magpieConfig()).isEmpty());
    }

    void neverTakesOverTheUsersProfile()
    {
        GameProfile emu = tekken();
        emu.name = "Emulator";
        emu.exePath = "C:\\Emu\\emu.exe";
        emu.windowClass = "EmuWnd";
        const EditResult r = setGameProfile(magpieConfig(), emu);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("Emulator (mine)"));

        // another window class of the same exe is a different profile for Magpie
        emu.windowClass = "EmuRenderWnd";
        QVERIFY(setGameProfile(magpieConfig(), emu).ok);
    }

    void rejectsWhatMagpieCannotUse()
    {
        GameProfile g = tekken();
        g.scalingMode = "DLSS";
        EditResult r = setGameProfile(magpieConfig(), g);
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("Lanczos, FSR, Integer Scale 2x"));

        g = tekken();
        g.windowClass.clear();
        QVERIFY(!setGameProfile(magpieConfig(), g).ok);
        g = tekken();
        g.name = "  ";
        QVERIFY(!setGameProfile(magpieConfig(), g).ok);

        // a config Magpie has not filled yet: start it once first
        r = setGameProfile("", tekken());
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains("not written"));
        QVERIFY(!setGameProfile(json("{'profiles': [{}]}"), tekken()).ok);   // no scaling modes
        QVERIFY(!setGameProfile(json("{'scalingModes': [{'name': 'FSR'}]}"), tekken()).ok);   // no default profile
        QVERIFY(!setGameProfile("{ broken", tekken()).ok);
    }

    void removesOnlyItsOwnProfiles()
    {
        QByteArray config = setGameProfile(magpieConfig(), tekken()).config;
        GameProfile other = tekken();
        other.name = "Other";
        other.exePath = "D:\\Games\\Other\\other.exe";
        config = setGameProfile(config, other).config;
        QCOMPARE(profilesOf(config).size(), 4);

        const EditResult one = removeGameProfiles(config, tekken().exePath);
        QVERIFY(one.ok);
        QCOMPARE(gameProfiles(one.config).size(), 1);
        QCOMPARE(gameProfiles(one.config)[0].name, QString("Other"));

        const EditResult all = removeGameProfiles(config);
        QVERIFY(all.ok);
        const QJsonArray left = profilesOf(all.config);
        QCOMPARE(left.size(), 2);   // the default profile and the user's own
        QCOMPARE(left[1].toObject()["name"].toString(), QString("Emulator (mine)"));
        QVERIFY(!removeGameProfiles("").ok);
    }

    // ---- the app's own copy ----

    void latestReleaseIsTheNewestX64ZipWithADigest()
    {
        const QByteArray releases = json(
"[{'tag_name': 'v0.14.0-preview1', 'prerelease': true, 'assets': [\n"
"   {'name': 'Magpie-v0.14.0-x64.zip', 'size': 1, 'digest': 'sha256:1111111111111111111111111111111111111111111111111111111111111111',\n"
"    'browser_download_url': 'https://example.test/pre.zip'}]},\n"
" {'tag_name': 'v0.13.0', 'published_at': '2026-10-17T03:00:00Z', 'assets': [\n"
"   {'name': 'Magpie-v0.13.0-ARM64.zip', 'size': 10062236, 'digest': 'sha256:3f1d4f8fade6da13402ac4ea739867d60f0dee50e4c4fa56aab508eab0109551',\n"
"    'browser_download_url': 'https://example.test/arm.zip'},\n"
"   {'name': 'Magpie-v0.13.0-x64.zip', 'size': 10102378, 'digest': 'sha256:A3F774DCE9D2E050FB0934C362D51D8DB459A2E79E6F937BDB86852312899A9E',\n"
"    'browser_download_url': 'https://example.test/x64.zip'}]},\n"
" {'tag_name': 'v0.12.1', 'assets': [\n"
"   {'name': 'Magpie-v0.12.1-x64.zip', 'size': 5, 'digest': 'sha256:2222222222222222222222222222222222222222222222222222222222222222',\n"
"    'browser_download_url': 'https://example.test/old.zip'}]}]");
        const auto r = latestRelease(releases);
        QVERIFY(r.has_value());
        QCOMPARE(r->version.toString(), QString("0.13.0"));
        QCOMPARE(r->zipName, QString("Magpie-v0.13.0-x64.zip"));
        QCOMPARE(r->zipUrl, QString("https://example.test/x64.zip"));
        QCOMPARE(r->zipSize, Q_INT64_C(10102378));
        QCOMPARE(r->sha256, QString("a3f774dce9d2e050fb0934c362d51d8db459a2e79e6f937bdb86852312899a9e"));
        QCOMPARE(r->tag, QString("v0.13.0"));

        // without a digest there is nothing to check the download against: not offered
        QVERIFY(!latestRelease(json("[{'tag_name': 'v0.13.0', 'assets': [{'name': 'Magpie-v0.13.0-x64.zip',"
                                    " 'browser_download_url': 'https://example.test/x64.zip'}]}]")).has_value());
        QVERIFY(!latestRelease(json("[{'tag_name': 'v0.13.0', 'assets': [{'name': 'Magpie-v0.13.0-x64.zip',"
                                    " 'digest': 'sha256:abc', 'browser_download_url': 'https://example.test/x64.zip'}]}]")).has_value());
        QVERIFY(!latestRelease("not json").has_value());
        QVERIFY(!latestRelease("[]").has_value());
    }

    void installedVersionNeedsTheMarkerAndTheExe()
    {
        QTemporaryDir dir;
        QVERIFY(!installedVersion(dir.path()).has_value());
        QVERIFY(markInstalled(dir.path(), *Helix::parseVersion("0.13.0")));
        QVERIFY(!installedVersion(dir.path()).has_value());   // no Magpie.exe
        QFile exe(exePath(dir.path()));
        QVERIFY(exe.open(QIODevice::WriteOnly));
        exe.close();
        QCOMPARE(installedVersion(dir.path())->toString(), QString("0.13.0"));
        QCOMPARE(configPath(dir.path()), dir.path() + "/config/config.json");
        QVERIFY(installDir().endsWith("/magpie"));
    }

    void updatesKeepSettingsCacheAndLogs()
    {
        for (const char* kept : {"config", "cache", "logs", "Config"})
            QVERIFY2(keptOnUpdate(kept), kept);
        for (const char* replaced : {"Magpie.exe", "effects", "resources.pri", ".windowmanager-version"})
            QVERIFY2(!keptOnUpdate(replaced), replaced);
    }
};

QTEST_MAIN(TstMagpie)
#include "tst_magpie.moc"
