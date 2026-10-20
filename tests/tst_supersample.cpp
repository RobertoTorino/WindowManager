#include <QtTest>

#include "window/Supersample.h"

using namespace Supersample;
using Fit::Rect;

class TstSupersample : public QObject
{
    Q_OBJECT
private slots:
    // --- letterbox ----------------------------------------------------------

    void sameAspectFillsArea()
    {
        QCOMPARE(letterbox(3840, 2160, 1920, 1080), Rect::fromXYWH(0, 0, 1920, 1080));
    }

    void narrowerPictureGetsSideBars()
    {
        // 4:3 on 16:9 -> 1440x1080 centred
        QCOMPARE(letterbox(4, 3, 1920, 1080), Rect::fromXYWH(240, 0, 1440, 1080));
    }

    void widerPictureGetsTopAndBottomBars()
    {
        // 21:9-ish on 16:9
        const Rect r = letterbox(2560, 1080, 1920, 1080);
        QCOMPARE(r.width(), 1920);
        QCOMPARE(r.height(), 810);
        QCOMPARE(r.left, 0);
        QCOMPARE(r.top, (1080 - 810) / 2);
    }

    void letterboxNeverExceedsArea()
    {
        const int sizes[][2] = {{1, 1}, {7, 3}, {3, 7}, {1920, 1080}, {4096, 2160}, {640, 480}};
        for (const auto& s : sizes) {
            const Rect r = letterbox(s[0], s[1], 1920, 1080);
            QVERIFY(r.left >= 0 && r.top >= 0);
            QVERIFY(r.right <= 1920 && r.bottom <= 1080);
            QVERIFY(r.width() > 0 && r.height() > 0);
        }
    }

    void degenerateInputFillsArea()
    {
        QCOMPARE(letterbox(0, 0, 1920, 1080), Rect::fromXYWH(0, 0, 1920, 1080));
    }

    // --- render size --------------------------------------------------------

    void twoTimesOnMonitorAspect()
    {
        const Size s = renderSize(16, 9, 1920, 1080, 2.0);
        QCOMPARE(s.w, 3840);
        QCOMPARE(s.h, 2160);
    }

    void fourByThreeGameOnWidescreenMonitor()
    {
        // shown as 1440x1080, so 2x is 2880x2160 (not 3840x2160)
        const Size s = renderSize(4, 3, 1920, 1080, 2.0);
        QCOMPARE(s.w, 2880);
        QCOMPARE(s.h, 2160);
    }

    void fractionalFactor()
    {
        const Size s = renderSize(16, 9, 2560, 1440, 1.5);
        QCOMPARE(s.w, 3840);
        QCOMPARE(s.h, 2160);
    }

    void factorBelowOneIsClampedToOne()
    {
        const Size s = renderSize(16, 9, 1920, 1080, 0.5);
        QCOMPARE(s.w, 1920);
        QCOMPARE(s.h, 1080);
    }

    void renderedPictureKeepsItsAspect()
    {
        const Size s = renderSize(4, 3, 2400, 1350, 3.0);
        // 4:3 within 2400x1350 -> 1800x1350; x3 = 5400x4050
        QCOMPARE(s.w, 5400);
        QCOMPARE(s.h, 4050);
        QCOMPARE(s.w * 3, s.h * 4);
    }

    // --- parking ------------------------------------------------------------

    void parksBehindOverlayWhenNothingElseIsHit()
    {
        // Monitor 1 at the origin, monitor 2 above it. A 3840x2160 window at
        // monitor 1's origin reaches right/down only, so it stays hidden.
        const Rect m1 = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect m2 = Rect::fromXYWH(0, -1350, 2400, 1350);
        const Point p = parkingOrigin({m1, m2}, m1, 3840, 2160);
        QCOMPARE(p.x, 0);
        QCOMPARE(p.y, 0);
    }

    void parksOffDesktopWhenItWouldShowOnAnotherMonitor()
    {
        // Same layout, target monitor 2: a tall window would hang down onto monitor 1.
        const Rect m1 = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect m2 = Rect::fromXYWH(0, -1350, 2400, 1350);
        const Point p = parkingOrigin({m1, m2}, m2, 4800, 2700);
        QVERIFY(p.x >= 2400);                 // beyond the right edge of the desktop
        QCOMPARE(p.y, -1350);
    }

    void sideBySideMonitorsParkOffDesktop()
    {
        const Rect left  = Rect::fromXYWH(0, 0, 1920, 1080);
        const Rect right = Rect::fromXYWH(1920, 0, 1920, 1080);
        const Point p = parkingOrigin({left, right}, left, 3840, 2160);
        QVERIFY(p.x >= 3840);
    }

    void singleMonitorStaysBehindOverlay()
    {
        const Rect m = Rect::fromXYWH(0, 0, 1920, 1080);
        const Point p = parkingOrigin({m}, m, 5760, 3240);
        QCOMPARE(p.x, 0);
        QCOMPARE(p.y, 0);
    }

    // --- mouse mapping --------------------------------------------------------

    void mapsTopLeftAndBottomRightOfThePicture()
    {
        // 3160x2160 game shown as 1580x1080 at x=170 on a 1920x1080 overlay
        const Rect dest = Rect::fromXYWH(170, 0, 1580, 1080);
        Point p;
        QVERIFY(mapPoint(dest, 3160, 2160, 170, 0, false, p));
        QCOMPARE(p.x, 1);                    // centre of the first 2x2 block
        QCOMPARE(p.y, 1);
        QVERIFY(mapPoint(dest, 3160, 2160, 1749, 1079, false, p));
        QCOMPARE(p.x, 3159);                 // last game pixel, never past it
        QCOMPARE(p.y, 2159);
    }

    void mapsAnInteriorPoint()
    {
        const Rect dest = Rect::fromXYWH(170, 0, 1580, 1080);
        Point p;
        QVERIFY(mapPoint(dest, 3160, 2160, 250, 600, false, p));
        QCOMPARE(p.x, 161);                  // (250-170)*2 + 1
        QCOMPARE(p.y, 1201);                 // 600*2 + 1
    }

    void blackBarsAreIgnoredUnlessClamped()
    {
        const Rect dest = Rect::fromXYWH(170, 0, 1580, 1080);
        Point p;
        QVERIFY(!mapPoint(dest, 3160, 2160, 100, 500, false, p));    // left bar
        QVERIFY(!mapPoint(dest, 3160, 2160, 1800, 500, false, p));   // right bar
        QVERIFY(mapPoint(dest, 3160, 2160, 100, 500, true, p));      // dragging out of the picture
        QCOMPARE(p.x, 1);
        QVERIFY(mapPoint(dest, 3160, 2160, 1900, 500, true, p));
        QCOMPARE(p.x, 3159);
    }

    void oneToOneScaleKeepsCoordinates()
    {
        const Rect dest = Rect::fromXYWH(0, 0, 1920, 1080);
        Point p;
        QVERIFY(mapPoint(dest, 1920, 1080, 777, 333, false, p));
        QCOMPARE(p.x, 777);
        QCOMPARE(p.y, 333);
    }

    void fractionalScaleStaysInRange()
    {
        const Rect dest = Rect::fromXYWH(0, 0, 1920, 1080);
        Point p;
        for (int x = 0; x < 1920; x += 37) {
            QVERIFY(mapPoint(dest, 2880, 1620, x, 500, false, p));
            QVERIFY(p.x >= 0 && p.x < 2880);
        }
    }

    void degenerateInputIsRejected()
    {
        Point p;
        QVERIFY(!mapPoint(Rect{}, 3160, 2160, 5, 5, true, p));
        QVERIFY(!mapPoint(Rect::fromXYWH(0, 0, 100, 100), 0, 0, 5, 5, true, p));
    }

    // --- putting the game window back ------------------------------------------------------

    void reassertsEveryFocusChange()
    {
        // Tekken 7: back to 1280x720 each time it gets the focus, seconds apart. Always answered.
        Supersample::ReassertPolicy p;
        for (int i = 0; i < 20; ++i)
            QVERIFY(p.allow(qint64(i) * 3000));
        QVERIFY(!p.gaveUp());
    }

    void stopsAFastLoopThenTriesAgain()
    {
        Supersample::ReassertPolicy p;
        for (int i = 0; i < Supersample::ReassertPolicy::kMax; ++i)
            QVERIFY(p.allow(1000 + i * 100));
        QVERIFY(!p.allow(1600));   // the sixth within 10 s: follow the game
        QVERIFY(p.gaveUp());
        QVERIFY(!p.allow(5000));
        // quiet for 10 s after the last correction: try again
        QVERIFY(p.allow(1400 + Supersample::ReassertPolicy::kWindowMs + 1));
        QVERIFY(!p.gaveUp());
    }
};

QTEST_APPLESS_MAIN(TstSupersample)
#include "tst_supersample.moc"
