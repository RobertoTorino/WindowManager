#include <QtTest>

#include "window/FitCalculator.h"

using namespace Fit;

class TstFit : public QObject
{
    Q_OBJECT
private slots:
    // --- geometry -----------------------------------------------------------

    void insetsFromFramedWindow()
    {
        // 1280x720 client inside a window with a 31px title bar and 8px borders
        const Rect outer  = Rect::fromXYWH(100, 100, 1296, 759);
        const Rect client = Rect::fromXYWH(108, 131, 1280, 720);
        const Insets i = measureInsets(outer, client);
        QCOMPARE(i.left, 8);
        QCOMPARE(i.top, 31);
        QCOMPARE(i.right, 8);
        QCOMPARE(i.bottom, 8);
    }

    void borderlessWindowAlreadyFits()
    {
        const Rect mon = Rect::fromXYWH(0, 0, 2560, 1440);
        const Plan p = compute({mon, mon, mon, false});
        QVERIFY(!p.needsChange());
    }

    void borderlessWindowTooSmallAndOffset()
    {
        const Rect mon   = Rect::fromXYWH(0, 0, 2560, 1440);
        const Rect win   = Rect::fromXYWH(320, 180, 1920, 1080);
        const Plan p = compute({win, win, mon, false});
        QCOMPARE(p.dx, -320);
        QCOMPARE(p.dy, -180);
        QCOMPARE(p.dw, 640);
        QCOMPARE(p.dh, 360);
        QCOMPARE(p.desiredOuter, mon);
    }

    void framedWindowKeepingFrameOvershootsOuterRect()
    {
        // Keep the frame: the *picture* must cover the monitor, so the outer
        // rectangle grows by the insets (title bar ends up above the monitor).
        const Rect mon    = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect outer  = Rect::fromXYWH(100, 100, 1296, 759);
        const Rect client = Rect::fromXYWH(108, 131, 1280, 720);
        const Plan p = compute({outer, client, mon, false});
        QCOMPARE(p.desiredOuter, (Rect{-8, -31, 1928, 1088}));
        QCOMPARE(p.dx, -108);
        QCOMPARE(p.dy, -131);
        QCOMPARE(p.dw, 1936 - 1296);
        QCOMPARE(p.dh, 1119 - 759);
    }

    void strippingTheFrameFitsOuterRectToTarget()
    {
        const Rect mon    = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect outer  = Rect::fromXYWH(100, 100, 1296, 759);
        const Rect client = Rect::fromXYWH(108, 131, 1280, 720);
        const Plan p = compute({outer, client, mon, true});
        QCOMPARE(p.insets.left, 0);
        QCOMPARE(p.desiredOuter, mon);
        QCOMPARE(p.dx, -100);
        QCOMPARE(p.dy, -100);
        QCOMPARE(p.dw, 1920 - 1296);
        QCOMPARE(p.dh, 1080 - 759);
    }

    void secondMonitorWithNegativeOrigin()
    {
        // Monitor 2 sits left of the primary: negative X origin
        const Rect mon = Rect::fromXYWH(-1920, 0, 1920, 1080);
        const Rect win = Rect::fromXYWH(-1900, 20, 1900, 1060);
        const Plan p = compute({win, win, mon, false});
        QCOMPARE(p.dx, -20);
        QCOMPARE(p.dy, -20);
        QCOMPARE(p.dw, 20);
        QCOMPARE(p.dh, 20);
    }

    void overshootingWindowShrinks()
    {
        const Rect mon = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect win = Rect::fromXYWH(-10, -10, 1940, 1100); // overscan
        const Plan p = compute({win, win, mon, false});
        QCOMPARE(p.dx, 10);
        QCOMPARE(p.dy, 10);
        QCOMPARE(p.dw, -20);
        QCOMPARE(p.dh, -20);
    }

    void appliedPlanProducesNoFurtherCorrection()
    {
        const Rect mon    = Rect::fromXYWH(50, 30, 2560, 1440);
        const Rect outer  = Rect::fromXYWH(0, 0, 1000, 800);
        const Rect client = Rect::fromXYWH(7, 38, 986, 755);
        const Plan p = compute({outer, client, mon, false});

        // Simulate the window being moved to desiredOuter with the same insets
        const Insets i = p.insets;
        const Rect newOuter  = p.desiredOuter;
        const Rect newClient = {newOuter.left + i.left, newOuter.top + i.top,
                                newOuter.right - i.right, newOuter.bottom - i.bottom};
        QCOMPARE(newClient, mon);
        QVERIFY(!compute({newOuter, newClient, mon, false}).needsChange());
    }

    // --- settle trigger ------------------------------------------------------

    void triggerIdleWhenNotArmed()
    {
        Trigger t;
        QCOMPARE(t.update(0, 1, true, Rect::fromXYWH(0, 0, 10, 10)), Trigger::Result::Idle);
    }

    void triggerFiresOnceAfterWindowSettles()
    {
        Trigger t;
        const Rect r = Rect::fromXYWH(0, 0, 800, 600);
        t.arm(0, 120000, 1800);
        QCOMPARE(t.update(500, 7, true, r), Trigger::Result::Waiting);   // first sighting
        QCOMPARE(t.update(1500, 7, true, r), Trigger::Result::Waiting);  // 1000ms stable
        QCOMPARE(t.update(2400, 7, true, r), Trigger::Result::Fire);     // 1900ms stable
        QVERIFY(!t.armed());
        QCOMPARE(t.update(3000, 7, true, r), Trigger::Result::Idle);     // never twice
    }

    void triggerRestartsWhenWindowMoves()
    {
        Trigger t;
        t.arm(0, 120000, 1800);
        const Rect a = Rect::fromXYWH(0, 0, 800, 600);
        const Rect b = Rect::fromXYWH(0, 0, 1920, 1080);   // game resized itself
        QCOMPARE(t.update(500, 7, true, a), Trigger::Result::Waiting);
        QCOMPARE(t.update(2000, 7, true, b), Trigger::Result::Waiting);  // changed: clock restarts
        QCOMPARE(t.update(3000, 7, true, b), Trigger::Result::Waiting);
        QCOMPARE(t.update(3900, 7, true, b), Trigger::Result::Fire);
    }

    void triggerRestartsWhenWindowIsReplaced()
    {
        Trigger t;
        t.arm(0, 120000, 1800);
        const Rect r = Rect::fromXYWH(0, 0, 800, 600);
        QCOMPARE(t.update(500, 7, true, r), Trigger::Result::Waiting);   // splash screen
        QCOMPARE(t.update(2000, 8, true, r), Trigger::Result::Waiting);  // real window, new hwnd
        QCOMPARE(t.update(3900, 8, true, r), Trigger::Result::Fire);
    }

    void triggerIgnoresNotReadyWindow()
    {
        Trigger t;
        t.arm(0, 120000, 1800);
        const Rect r = Rect::fromXYWH(0, 0, 800, 600);
        QCOMPARE(t.update(500, 7, false, r), Trigger::Result::Waiting);   // minimized / hidden
        QCOMPARE(t.update(5000, 7, false, r), Trigger::Result::Waiting);
        QCOMPARE(t.update(5100, 7, true, r), Trigger::Result::Waiting);   // now usable: clock starts
        QCOMPARE(t.update(7000, 7, true, r), Trigger::Result::Fire);
    }

    void triggerTimesOutWhenNoWindowAppears()
    {
        Trigger t;
        t.arm(0, 10000, 1800);
        QCOMPARE(t.update(5000, 0, false, Rect{}), Trigger::Result::Waiting);
        QCOMPARE(t.update(10001, 0, false, Rect{}), Trigger::Result::TimedOut);
        QVERIFY(!t.armed());
    }

    void triggerCanBeRearmed()
    {
        Trigger t;
        const Rect r = Rect::fromXYWH(0, 0, 800, 600);
        t.arm(0, 120000, 1000);
        t.update(0, 7, true, r);
        QCOMPARE(t.update(1500, 7, true, r), Trigger::Result::Fire);
        t.arm(2000, 120000, 1000);
        QCOMPARE(t.update(2000, 9, true, r), Trigger::Result::Waiting);
        QCOMPARE(t.update(3100, 9, true, r), Trigger::Result::Fire);
    }
};

QTEST_APPLESS_MAIN(TstFit)
#include "tst_fit.moc"
