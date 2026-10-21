#include <QtTest>

#include "config/AppConfig.h"

class TstAppConfig : public QObject
{
    Q_OBJECT
private slots:
    void roundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("cfg.json");

        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(path);

        AppConfig::GameEntry g;
        g.applicationPath = "C:/g/game.exe";
        g.savedName = "MY_GAME";
        g.id = "GAME_MY_GAME";
        g.helixConfirmed = true;
        g.steamAppId = 1593500;
        g.display.enabled = true;
        g.display.width = 1280;
        g.display.height = 720;
        g.display.hdr = 1;
        g.magpie.mode = "FSR";
        g.magpie.game3D = true;
        cfg.saveGame(g);
        cfg.setCurrentExePath(g.applicationPath);
        cfg.saveWindowProfile(g.applicationPath, 10, 20, 1280, 720);
        QVERIFY(cfg.save());
        QVERIFY(QFile::exists(path));

        // Reload from disk.
        QVERIFY(cfg.load());
        QCOMPARE(cfg.currentExePath(), g.applicationPath);

        const auto game = cfg.gameByPath(g.applicationPath);
        QVERIFY(game.has_value());
        QCOMPARE(game->savedName, QString("MY_GAME"));
        QCOMPARE(game->id, QString("GAME_MY_GAME"));
        QVERIFY(game->helixConfirmed);
        QCOMPARE(game->steamAppId, 1593500u);
        QCOMPARE(game->display, g.display);
        QCOMPARE(game->magpie, g.magpie);

        const auto prof = cfg.getWindowProfile(g.applicationPath);
        QVERIFY(prof.has_value());
        QVERIFY(prof->hasProfile);
        QCOMPARE(prof->x, 10);
        QCOMPARE(prof->y, 20);
        QCOMPARE(prof->w, 1280);
        QCOMPARE(prof->h, 720);
    }

    void helixFlagDefaultsToOff()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("old.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"games\":[{\"ApplicationPath\":\"C:/a.exe\",\"SavedName\":\"A\"}]}");
        f.close();

        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(path);
        QVERIFY(cfg.load());
        QVERIFY(!cfg.gameByPath("C:/a.exe")->helixConfirmed);
        QVERIFY(!cfg.gameByPath("C:/a.exe")->display.enabled);   // no display settings either
        QVERIFY(!cfg.gameByPath("C:/a.exe")->magpie.enabled());   // and no Magpie
    }

    void clearProfile()
    {
        QTemporaryDir dir;
        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(dir.filePath("c.json"));
        cfg.saveWindowProfile("C:/a.exe", 1, 2, 3, 4);
        QVERIFY(cfg.getWindowProfile("C:/a.exe").has_value());
        cfg.clearWindowProfile("C:/a.exe");
        QVERIFY(!cfg.getWindowProfile("C:/a.exe").has_value());
    }

    void corruptFileFailsLoad()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("bad.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{ not json");
        f.close();

        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(path);
        QVERIFY(!cfg.load());
    }

    void missingFileIsFreshStart()
    {
        QTemporaryDir dir;
        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(dir.filePath("nope.json"));
        QVERIFY(cfg.load());
    }

    void legacyGhostWindowsKeyIsIgnored()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("old.json");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("{\"lastExePath\":\"C:/a.exe\","
                "\"ghostWindows\":{\"C:/a.exe\":[{\"titleContains\":\"x\",\"className\":\"y\"}]}}");
        f.close();

        auto& cfg = AppConfig::instance();
        cfg.setConfigPathOverride(path);
        QVERIFY(cfg.load());
        QCOMPARE(cfg.currentExePath(), QString("C:/a.exe"));
    }
};

QTEST_MAIN(TstAppConfig)
#include "tst_appconfig.moc"
