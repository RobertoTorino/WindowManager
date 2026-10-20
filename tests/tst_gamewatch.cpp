#include <QtTest>

#include "window/GameWatchModel.h"

using namespace Watch;

namespace {

WindowState startScreen()
{
    WindowState s;
    s.rect = {320, 180, 1280, 720};
    s.client = {320, 180, 1280, 720};
    s.visible = true;
    s.framed = false;
    s.title = "TEKKEN 7";
    s.className = "UnrealWindow";
    return s;
}

} // namespace

class TstGameWatch : public QObject
{
    Q_OBJECT
private slots:
    void describesRects()
    {
        QCOMPARE(describe({0, 0, 1920, 1080}), QString("1920x1080 at 0,0"));
        QCOMPARE(describe({-1920, -1350, 2400, 1350}), QString("2400x1350 at -1920,-1350"));
    }

    void firstSightIsAFullLine()
    {
        WindowTracker t;
        const auto line = t.update(0x1a2b, startScreen());
        QVERIFY(line.has_value());
        QCOMPARE(*line, QString("Game window 0x1a2b 'TEKKEN 7' (UnrealWindow): window 1280x720 at 320,180, "
                                "drawn area 1280x720 at 320,180, borderless"));
        QCOMPARE(t.count(), size_t(1));
    }

    void hiddenWindowsWaitUntilShown()
    {
        WindowTracker t;
        WindowState helper;
        helper.className = "IME";
        QVERIFY(!t.update(0x10, helper).has_value());
        QCOMPARE(t.count(), size_t(0));
        helper.visible = true;
        helper.rect = helper.client = {0, 0, 800, 600};
        QVERIFY(t.update(0x10, helper)->startsWith("Game window 0x10 '' (IME): window 800x600"));
        helper.visible = false;
        QCOMPARE(*t.update(0x10, helper), QString("Game window 0x10: hidden"));
    }

    void sameStateIsNotLoggedTwice()
    {
        WindowTracker t;
        t.update(0x1a2b, startScreen());
        QVERIFY(!t.update(0x1a2b, startScreen()).has_value());
    }

    void sizeChangeListsOldAndNew()
    {
        WindowTracker t;
        t.update(0x1a2b, startScreen());
        WindowState game = startScreen();
        game.rect = game.client = {0, 0, 1920, 1080};
        QCOMPARE(*t.update(0x1a2b, game),
                 QString("Game window 0x1a2b: window 1280x720 at 320,180 -> 1920x1080 at 0,0, "
                         "drawn area 1280x720 at 320,180 -> 1920x1080 at 0,0"));
    }

    void frameVisibilityAndTitleChanges()
    {
        WindowTracker t;
        t.update(0x1a2b, startScreen());
        WindowState s = startScreen();
        s.framed = true;
        s.visible = false;
        s.title = "TEKKEN 7 - Loading";
        QCOMPARE(*t.update(0x1a2b, s),
                 QString("Game window 0x1a2b: hidden, frame added, title 'TEKKEN 7 - Loading'"));
    }

    void classChangeAloneIsNotWorthALine()
    {
        WindowTracker t;
        t.update(0x1a2b, startScreen());
        WindowState s = startScreen();
        s.className = "Other";
        QVERIFY(!t.update(0x1a2b, s).has_value());
    }

    void destroyedOnlyForKnownWindows()
    {
        WindowTracker t;
        QVERIFY(!t.destroyed(0x99).has_value());
        t.update(0x1a2b, startScreen());
        QCOMPARE(*t.destroyed(0x1a2b), QString("Game window 0x1a2b closed"));
        QCOMPARE(t.count(), size_t(0));
        // seen again after closing: a full line again
        QVERIFY(t.update(0x1a2b, startScreen())->startsWith("Game window 0x1a2b 'TEKKEN 7'"));
    }

    void foregroundKeepsOtherProgramsPrivate()
    {
        QCOMPARE(foregroundLine(Owner::Game, "TekkenGame-Win64-Shipping.exe", "TEKKEN 7"),
                 QString("Foreground: TekkenGame-Win64-Shipping.exe 'TEKKEN 7' (the game)"));
        QCOMPARE(foregroundLine(Owner::ThisApp, "WindowManager.exe", "WindowManager 0.1.6"),
                 QString("Foreground: WindowManager 'WindowManager 0.1.6' (this app)"));
        QCOMPARE(foregroundLine(Owner::Other, "chrome.exe", "My bank - Chrome"),
                 QString("Foreground: chrome.exe (another program)"));
        QCOMPARE(foregroundLine(Owner::Other, {}, {}), QString("Foreground: unknown program (another program)"));
    }

    void mouseClip()
    {
        const Rect desktop{-1920, -1350, 4320, 2430};
        QCOMPARE(clipLine(std::nullopt, desktop), QString("Mouse: free"));
        QCOMPARE(clipLine(desktop, desktop), QString("Mouse: free"));
        QCOMPARE(clipLine(Rect{0, 0, 1920, 1080}, desktop), QString("Mouse: held inside 1920x1080 at 0,0"));
    }
};

QTEST_GUILESS_MAIN(TstGameWatch)
#include "tst_gamewatch.moc"
