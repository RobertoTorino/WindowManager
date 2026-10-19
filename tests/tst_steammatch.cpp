#include <QtTest>

#include "window/SteamMatch.h"

using namespace SteamMatch;

class TstSteamMatch : public QObject
{
    Q_OBJECT
private slots:
    void appIdFromClass_data()
    {
        QTest::addColumn<QString>("cls");
        QTest::addColumn<uint>("expected");
        QTest::newRow("proton game")      << "steam_app_1245620" << 1245620u;
        QTest::newRow("upper case")       << "STEAM_APP_730" << 730u;
        QTest::newRow("padded")           << "  steam_app_730 " << 730u;
        QTest::newRow("steam client")     << "steam" << 0u;
        QTest::newRow("no number")        << "steam_app_" << 0u;
        QTest::newRow("zero")             << "steam_app_0" << 0u;
        QTest::newRow("trailing junk")    << "steam_app_12x" << 0u;
        QTest::newRow("leading zeros")    << "steam_app_007" << 0u;
        QTest::newRow("too big")          << "steam_app_99999999999" << 0u;
        QTest::newRow("other prefix")     << "xsteam_app_5" << 0u;
        QTest::newRow("empty")            << "" << 0u;
    }
    void appIdFromClass()
    {
        QFETCH(QString, cls);
        QFETCH(uint, expected);
        QCOMPARE(appIdFromWindowClass(cls.toStdString()), expected);
    }

    void parseAppId_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<uint>("expected");
        QTest::newRow("number")             << "1245620" << 1245620u;
        QTest::newRow("padded number")      << "  730\n" << 730u;
        QTest::newRow("class")              << "steam_app_730" << 730u;
        QTest::newRow("run url")            << "steam://rungameid/1245620" << 1245620u;
        QTest::newRow("store page")         << "https://store.steampowered.com/app/1245620/ELDEN_RING/" << 1245620u;
        QTest::newRow("store page no name") << "https://store.steampowered.com/app/730" << 730u;
        QTest::newRow("words")              << "elden ring" << 0u;
        QTest::newRow("number and words")   << "730 cs" << 0u;
        QTest::newRow("empty")              << "" << 0u;
        QTest::newRow("zero")               << "0" << 0u;
        QTest::newRow("negative")           << "-5" << 0u;
    }
    void parseAppId()
    {
        QFETCH(QString, text);
        QFETCH(uint, expected);
        QCOMPARE(SteamMatch::parseAppId(text.toStdString()), expected);
    }

    void launchUrlHasTheAppId()
    {
        QCOMPARE(QString::fromStdString(launchUrl(1245620)), QString("steam://rungameid/1245620"));
        QCOMPARE(SteamMatch::parseAppId(launchUrl(42)), 42u);
    }

    void steamWindowMatchesByAppIdEvenWhenTheImageIsAWineLoader()
    {
        Target t{1245620, "c:/games/eldenring.exe", "eldenring.exe"};
        WindowFacts w{1245620, "/usr/lib/wine/wine64-preloader", "wine64-preloader"};
        QCOMPARE(match(t, w), How::AppId);
    }

    void anotherSteamGameNeverMatchesEvenWithTheSameExe()
    {
        Target t{1245620, "", "game.exe"};
        WindowFacts w{730, "", "game.exe"};
        QCOMPARE(match(t, w), How::None);
    }

    void steamTargetFallsBackToThePathWhenTheWindowHasNoAppId()
    {
        Target t{1245620, "c:/games/a.exe", "a.exe"};
        QCOMPARE(match(t, WindowFacts{0, "c:/games/a.exe", "a.exe"}), How::Path);
        QCOMPARE(match(t, WindowFacts{0, "d:/other/a.exe", "a.exe"}), How::ExeName);
        QCOMPARE(match(t, WindowFacts{0, "d:/other/b.exe", "b.exe"}), How::None);
    }

    void nonSteamTargetIgnoresTheWindowsAppId()
    {
        Target t{0, "c:/emu/dolphin.exe", "dolphin.exe"};
        QCOMPARE(match(t, WindowFacts{0, "c:/emu/dolphin.exe", "dolphin.exe"}), How::Path);
        QCOMPARE(match(t, WindowFacts{730, "x", "dolphin.exe"}), How::ExeName);
        QVERIFY(!matches(t, WindowFacts{730, "x", "other.exe"}));
    }

    void emptyTargetMatchesNothing()
    {
        QVERIFY(!matches(Target{}, WindowFacts{}));
        QVERIFY(!matches(Target{}, WindowFacts{730, "c:/a.exe", "a.exe"}));
    }
};

QTEST_APPLESS_MAIN(TstSteamMatch)
#include "tst_steammatch.moc"
