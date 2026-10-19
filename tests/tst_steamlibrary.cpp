#include <QtTest>

#include "config/AppConfig.h"
#include "ui/GameRegistry.h"
#include "window/SteamLibrary.h"

using namespace SteamLibrary;

namespace {

void writeFile(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

QByteArray manifest(quint32 appId, const QString& name, const QString& dir, int flags = 4)
{
    return QString("\"AppState\"\n{\n\t\"appid\"\t\t\"%1\"\n\t\"universe\"\t\t\"1\"\n\t\"name\"\t\t\"%2\"\n"
                   "\t\"StateFlags\"\t\t\"%3\"\n\t\"installdir\"\t\t\"%4\"\n"
                   "\t\"InstalledDepots\"\n\t{\n\t\t\"%5\"\n\t\t{\n\t\t\t\"manifest\"\t\t\"123\"\n\t\t}\n\t}\n}\n")
        .arg(appId).arg(name).arg(flags).arg(dir).arg(appId + 1)
        .toUtf8();
}

} // namespace

class TstSteamLibrary : public QObject
{
    Q_OBJECT
private slots:
    void libraryFolders()
    {
        const QString vdf = "\"libraryfolders\"\n{\n"
                            "\t\"0\"\n\t{\n"
                            "\t\t\"path\"\t\t\"/home/me/.local/share/Steam\"\n"
                            "\t\t\"label\"\t\t\"\"\n"
                            "\t\t\"apps\"\n\t\t{\n\t\t\t\"228980\"\t\t\"0\"\n\t\t\t\"1593500\"\t\t\"68973454991\"\n\t\t}\n"
                            "\t}\n"
                            "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"D:\\\\SteamLibrary\"\n\t}\n"
                            "}\n";
        QCOMPARE(parseLibraryFolders(vdf), (QStringList{"/home/me/.local/share/Steam", "D:/SteamLibrary"}));
    }

    void libraryFoldersOldFormat()
    {
        const QString vdf = "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\t\"1700000000\"\n"
                            "\t\"ContentStatsID\"\t\t\"-123\"\n\t\"1\"\t\t\"E:\\\\Games\\\\Steam\"\n}\n";
        QCOMPARE(parseLibraryFolders(vdf), QStringList{"E:/Games/Steam"});
    }

    void libraryFoldersGarbage()
    {
        QVERIFY(parseLibraryFolders("").isEmpty());
        QVERIFY(parseLibraryFolders("{ } } \"x").isEmpty());
    }

    void manifestFields()
    {
        const auto m = parseManifest(QString::fromUtf8(manifest(1593500, "God of War", "GodOfWar")));
        QVERIFY(m.has_value());
        QCOMPARE(m->appId, 1593500u);
        QCOMPARE(m->name, QString("God of War"));
        QCOMPARE(m->installDir, QString("GodOfWar"));
        QVERIFY(m->fullyInstalled);
    }

    void manifestStates()
    {
        QVERIFY(parseManifest(QString::fromUtf8(manifest(1, "A", "A", 6)))->fullyInstalled);    // update required
        QVERIFY(!parseManifest(QString::fromUtf8(manifest(1, "A", "A", 1026)))->fullyInstalled); // downloading
        QVERIFY(!parseManifest("\"AppState\" { \"name\" \"no id\" }").has_value());
    }

    void manifestEscapes()
    {
        const auto m = parseManifest("\"AppState\" { \"appid\" \"7\" \"name\" \"Say \\\"Hi\\\"\" "
                                     "\"installdir\" \"a\\\\b\" }");
        QVERIFY(m.has_value());
        QCOMPARE(m->name, QString("Say \"Hi\""));
        QCOMPARE(m->installDir, QString("a\\b"));
    }

    void tools_data()
    {
        QTest::addColumn<uint>("appId");
        QTest::addColumn<QString>("name");
        QTest::addColumn<bool>("tool");
        QTest::newRow("redist")       << 228980u  << "Steamworks Common Redistributables" << true;
        QTest::newRow("runtime")      << 4183110u << "Steam Linux Runtime 4.0" << true;
        QTest::newRow("experimental") << 1493710u << "Proton Experimental" << true;
        QTest::newRow("new proton")   << 9999999u << "Proton 11.0" << true;
        QTest::newRow("game")         << 1593500u << "God of War" << false;
        QTest::newRow("prototype")    << 10150u   << "Prototype" << false;
    }
    void tools()
    {
        QFETCH(uint, appId);
        QFETCH(QString, name);
        QFETCH(bool, tool);
        QCOMPARE(isTool(appId, name), tool);
    }

    void mainExe()
    {
        const QList<ExeCandidate> exes = {
            {"_CommonRedist/vcredist/vc_redist.x64.exe", 25'000'000},
            {"GoW.exe", 90'000'000},
            {"UnityCrashHandler64.exe", 2'000'000},
            {"EasyAntiCheat/EasyAntiCheat_EOS_Setup.exe", 120'000'000},
            {"tools/small.exe", 1'000},
        };
        QCOMPARE(pickMainExe(exes), 1);
    }

    void mainExeUnreal()
    {
        // The stub in the root starts the shipping exe, which owns the window: the bigger one wins.
        const QList<ExeCandidate> exes = {
            {"TEKKEN 7.exe", 300'000},
            {"TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe", 60'000'000},
            {"Engine/Binaries/ThirdParty/CEF3/Win64/UnrealCEFSubProcess.exe", 500'000},
        };
        QCOMPARE(pickMainExe(exes), 1);
    }

    void mainExeNone()
    {
        QCOMPARE(pickMainExe({}), -1);
        QCOMPARE(pickMainExe({{"unins000.exe", 1'000'000}, {"Redist/setup.exe", 5'000}}), -1);
    }

    void findsGamesInAFakeSteamFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString steam = dir.filePath("Steam");
        const QString other = dir.filePath("Library2");
        writeFile(steam + "/steamapps/libraryfolders.vdf",
                  QString("\"libraryfolders\" { \"0\" { \"path\" \"%1\" } \"1\" { \"path\" \"%2\" } "
                          "\"2\" { \"path\" \"%3\" } }")
                      .arg(steam, other, dir.filePath("unplugged"))
                      .toUtf8());

        writeFile(steam + "/steamapps/appmanifest_1593500.acf", manifest(1593500, "God of War", "GodOfWar"));
        writeFile(steam + "/steamapps/common/GodOfWar/GoW.exe", QByteArray(2000, 'x'));
        writeFile(steam + "/steamapps/common/GodOfWar/CrashReporter.exe", QByteArray(9000, 'x'));

        writeFile(steam + "/steamapps/appmanifest_1493710.acf", manifest(1493710, "Proton Experimental", "Proton"));
        writeFile(steam + "/steamapps/common/Proton/files/bin/wine64.exe", QByteArray(10, 'x'));

        writeFile(other + "/steamapps/appmanifest_389730.acf", manifest(389730, "TEKKEN 7", "TEKKEN 7"));
        writeFile(other + "/steamapps/common/TEKKEN 7/TEKKEN 7.exe", QByteArray(100, 'x'));
        writeFile(other + "/steamapps/common/TEKKEN 7/TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe",
                  QByteArray(5000, 'x'));

        // still downloading, and one whose folder has no executable
        writeFile(other + "/steamapps/appmanifest_5.acf", manifest(5, "Half Done", "HalfDone", 1026));
        writeFile(other + "/steamapps/common/HalfDone/half.exe", QByteArray(10, 'x'));
        writeFile(other + "/steamapps/appmanifest_6.acf", manifest(6, "Empty", "Empty"));
        QDir().mkpath(other + "/steamapps/common/Empty");

        const Scan scan = findGames({QFileInfo(steam).canonicalFilePath()});
        QCOMPARE(scan.libraries.size(), 2);   // "unplugged" does not exist
        QCOMPARE(scan.games.size(), 2);

        const auto gow = std::find_if(scan.games.begin(), scan.games.end(), [](const Game& g) {
            return g.appId == 1593500;
        });
        QVERIFY(gow != scan.games.end());
        QCOMPARE(gow->name, QString("God of War"));
        QVERIFY(gow->exePath.endsWith("/steamapps/common/GodOfWar/GoW.exe"));

        const auto tekken = std::find_if(scan.games.begin(), scan.games.end(), [](const Game& g) {
            return g.appId == 389730;
        });
        QVERIFY(tekken != scan.games.end());
        QVERIFY(tekken->exePath.endsWith("/TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe"));
    }

    void syncAddsKeepsAndRemoves()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(dir.filePath("cfg.json"));
        QVERIFY(cfg.load());

        const QString lib = dir.path();
        const QString gowExe = lib + "/steamapps/common/GodOfWar/GoW.exe";
        const QString tekkenExe = lib + "/steamapps/common/TEKKEN 7/TEKKEN 7.exe";
        const QString goneExe = lib + "/steamapps/common/Gone/gone.exe";
        writeFile(gowExe, "x");
        writeFile(tekkenExe, "x");

        // added by hand earlier, with the user's own name
        cfg.saveGame(GameRegistry::buildEntry(tekkenExe, "MY_TEKKEN"));
        // a Steam game that has been uninstalled since
        auto gone = GameRegistry::buildEntry(goneExe, "GONE");
        gone.steamAppId = 42;
        cfg.saveGame(gone);
        // a Steam game on a drive that is not there: left alone
        auto away = GameRegistry::buildEntry("/nowhere/steamapps/common/Away/away.exe", "AWAY");
        away.steamAppId = 43;
        cfg.saveGame(away);

        Scan scan;
        scan.libraries = {lib};
        scan.games = {Game{1593500, "God of War", lib, lib + "/steamapps/common/GodOfWar", gowExe},
                      Game{389730, "TEKKEN 7", lib, lib + "/steamapps/common/TEKKEN 7", tekkenExe}};

        const auto sync = GameRegistry::syncSteamGames(scan);
        QCOMPARE(sync.added, 1);
        QCOMPARE(sync.removed, 1);

        const auto gow = cfg.gameByPath(gowExe);
        QVERIFY(gow.has_value());
        QCOMPARE(gow->steamAppId, 1593500u);
        QCOMPARE(gow->savedName, QString("GOD_OF_WAR"));

        const auto tekken = cfg.gameByPath(tekkenExe);
        QVERIFY(tekken.has_value());
        QCOMPARE(tekken->steamAppId, 389730u);
        QCOMPARE(tekken->savedName, QString("MY_TEKKEN"));

        QVERIFY(!cfg.gameByPath(goneExe).has_value());
        QVERIFY(cfg.gameByPath("/nowhere/steamapps/common/Away/away.exe").has_value());

        // a second run changes nothing
        const auto again = GameRegistry::syncSteamGames(scan);
        QCOMPARE(again.added, 0);
        QCOMPARE(again.removed, 0);
        QCOMPARE(cfg.games().size(), 3);
    }

    void syncFollowsAMovedExecutable()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(dir.filePath("cfg.json"));
        QVERIFY(cfg.load());

        const QString lib = dir.path();
        const QString newExe = lib + "/steamapps/common/G/bin/game.exe";
        writeFile(newExe, "x");
        auto old = GameRegistry::buildEntry(lib + "/steamapps/common/G/game.exe", "RENAMED_BY_ME");
        old.steamAppId = 7;
        cfg.saveGame(old);

        Scan scan;
        scan.libraries = {lib};
        scan.games = {Game{7, "G", lib, lib + "/steamapps/common/G", newExe}};
        const auto sync = GameRegistry::syncSteamGames(scan);
        QCOMPARE(sync.added, 0);
        QCOMPARE(sync.removed, 0);
        QCOMPARE(cfg.games().size(), 1);
        const auto moved = cfg.gameByPath(newExe);
        QVERIFY(moved.has_value());
        QCOMPARE(moved->savedName, QString("RENAMED_BY_ME"));
        QCOMPARE(moved->id, old.id);
    }
};

QTEST_GUILESS_MAIN(TstSteamLibrary)
#include "tst_steamlibrary.moc"
