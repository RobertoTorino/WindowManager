#include <QtTest>

#include "display/DisplayBackend.h"
#include "display/DisplayModel.h"

using namespace DisplayModel;

namespace {

Monitor monitor1080()
{
    Monitor m;
    m.id   = "\\\\?\\DISPLAY#DEL41A8#5&2b3c&0&UID4357#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}";
    m.name = "DELL U2720Q";
    m.primary = true;
    m.mode = {1920, 1080, 144};
    m.modes = {{1920, 1080, 144}, {1920, 1080, 120}, {1920, 1080, 60}, {1280, 720, 60}, {1280, 720, 120},
               {2560, 1440, 60}, {800, 600, 60}};
    m.hdrSupported = true;
    m.hdrOn = false;
    m.scaleKnown = true;
    m.scale = 125;
    m.recommendedScale = 125;
    m.scaleOptions = {100, 125, 150, 175};
    m.brightnessSupported = true;
    m.brightness = 80;
    return m;
}

} // namespace

class TstDisplay : public QObject
{
    Q_OBJECT
private slots:
    // --- modes ---------------------------------------------------------------

    void sizesAreDistinctLargestFirst()
    {
        const auto s = sizes(monitor1080().modes);
        QCOMPARE(s.size(), size_t(4));
        QCOMPARE(s[0], (Size{2560, 1440}));
        QCOMPARE(s[1], (Size{1920, 1080}));
        QCOMPARE(s[2], (Size{1280, 720}));
        QCOMPARE(s[3], (Size{800, 600}));
    }

    void refreshRatesForOneSizeHighestFirst()
    {
        QCOMPARE(refreshRates(monitor1080().modes, {1920, 1080}), (std::vector<int>{144, 120, 60}));
        QCOMPARE(refreshRates(monitor1080().modes, {1024, 768}), std::vector<int>{});
    }

    void bestModePrefersTheAskedThenThePreferredRate()
    {
        const auto modes = monitor1080().modes;
        QCOMPARE(*bestMode(modes, {1280, 720}, 120, 144), (Mode{1280, 720, 120}));
        QCOMPARE(*bestMode(modes, {1280, 720}, 0, 60), (Mode{1280, 720, 60}));       // keep the rate
        QCOMPARE(*bestMode(modes, {1280, 720}, 75, 144), (Mode{1280, 720, 120}));    // neither: highest
        QVERIFY(!bestMode(modes, {1024, 768}, 60, 60));
    }

    void portraitSwapsTheShownSizes()
    {
        Monitor m = monitor1080();
        m.orientation = Orientation::Portrait;
        m.mode = {1080, 1920, 144};
        const auto shown = orientedModes(m);
        QCOMPARE(shown.front(), (Mode{1080, 1920, 144}));
        QCOMPARE(oriented({1920, 1080}, Orientation::PortraitFlipped), (Size{1080, 1920}));
        QCOMPARE(oriented({1920, 1080}, Orientation::LandscapeFlipped), (Size{1920, 1080}));
    }

    // --- scale -----------------------------------------------------------------

    void scaleStepsAreRelativeToTheRecommendedOne()
    {
        // recommended 125 % (one step above 100), current 150 %, up to 175 %
        const ScaleInfo info = scaleFromRelative(-1, 1, 2);
        QVERIFY(info.valid);
        QCOMPARE(info.recommended, 125);
        QCOMPARE(info.current, 150);
        QCOMPARE(info.options, (std::vector<int>{100, 125, 150, 175}));
        QCOMPARE(*relativeScale(info, 100), -1);
        QCOMPARE(*relativeScale(info, 175), 2);
        QVERIFY(!relativeScale(info, 200));   // not offered at this resolution
        QVERIFY(!relativeScale(info, 110));
    }

    void scaleAt100RecommendedAndBadInput()
    {
        const ScaleInfo info = scaleFromRelative(0, 0, 3);
        QCOMPARE(info.recommended, 100);
        QCOMPARE(info.options, (std::vector<int>{100, 125, 150, 175}));
        QVERIFY(!scaleFromRelative(1, 0, 2).valid);     // min above the recommended one
        QVERIFY(!scaleFromRelative(-1, 5, 2).valid);    // current out of range
        QVERIFY(!scaleFromRelative(-20, 0, 2).valid);   // beyond the step table
        // the table ends at 500 %; a larger max is clipped
        QCOMPARE(scaleFromRelative(0, 0, 40).options.back(), 500);
    }

    // --- brightness ----------------------------------------------------------

    void brightnessMapsToPercent()
    {
        QCOMPARE(brightnessPercent(50, 0, 100), 50);
        QCOMPARE(brightnessPercent(30, 10, 50), 50);
        QCOMPARE(brightnessPercent(200, 0, 100), 100);   // clamped
        QCOMPARE(brightnessPercent(5, 5, 5), 0);         // no range
        QCOMPARE(brightnessValue(50, 10, 50), 30);
        QCOMPARE(brightnessValue(150, 0, 100), 100);
        QCOMPARE(brightnessValue(brightnessPercent(37, 0, 255), 0, 255), 38);   // rounding stays close
    }

    void wmiInstanceBelongsToTheMonitor()
    {
        const QString id = "\\\\?\\DISPLAY#BOE0867#4&1a2b3c&0&UID8388688#{e6f07b5f-ee97-4a90-b076-33f57bf4eaa7}";
        QVERIFY(wmiInstanceMatches(id, "DISPLAY\\BOE0867\\4&1a2b3c&0&UID8388688_0"));
        QVERIFY(wmiInstanceMatches(id, "display\\boe0867\\4&1a2b3c&0&uid8388688_0"));
        QVERIFY(!wmiInstanceMatches(id, "DISPLAY\\BOE0867\\4&1a2b3c&0&UID8388689_0"));
        QVERIFY(!wmiInstanceMatches(id, "DISPLAY\\BOE0867\\4&1a2b3c&0&UID83886880_0"));
        QVERIFY(!wmiInstanceMatches(QString(), "DISPLAY\\X\\1_0"));
    }

    // --- per game ------------------------------------------------------------

    void gameSettingsRoundTripAndSanitise()
    {
        GameSettings s;
        s.enabled = true;
        s.monitorId = "abc";
        s.width = 1280;
        s.height = 720;
        s.refresh = 60;
        s.hdr = 1;
        s.scale = 100;
        s.brightness = 40;
        QCOMPARE(gameSettingsFromJson(toJson(s)), s);

        QJsonObject bad;
        bad["Width"] = 1280;   // no height: no resolution
        bad["Hdr"] = 7;
        bad["Brightness"] = 300;
        const GameSettings b = gameSettingsFromJson(bad);
        QCOMPARE(b.width, 0);
        QCOMPARE(b.hdr, 1);
        QCOMPARE(b.brightness, 100);
        QVERIFY(gameSettingsFromJson({}).isEmpty());
        QVERIFY(!gameSettingsFromJson({}).enabled);
    }

    void describeNamesOnlyWhatChanges()
    {
        GameSettings s;
        QCOMPARE(describe(s), QString("no changes"));
        s.width = 1280;
        s.height = 720;
        s.refresh = 60;
        s.hdr = 0;
        QCOMPARE(describe(s), QString("1280 x 720 @ 60 Hz, HDR off"));
        s = {};
        s.refresh = 120;
        s.brightness = 0;
        QCOMPARE(describe(s), QString("120 Hz, brightness 0 %"));
    }

    void pickMonitorByIdThenPrimary()
    {
        Monitor a = monitor1080();
        a.id = "A";
        a.primary = false;
        Monitor b = monitor1080();
        b.id = "B";
        b.primary = true;
        const std::vector<Monitor> list = {a, b};
        QCOMPARE(pickMonitor(list, "a"), 0);
        QCOMPARE(pickMonitor(list, "gone"), 1);
        QCOMPARE(pickMonitor(list, QString()), 1);
        QCOMPARE(pickMonitor({}, "A"), -1);
    }

    void planChangesOnlyWhatDiffers()
    {
        const Monitor now = monitor1080();
        GameSettings want;
        want.width = 1920;
        want.height = 1080;   // same size, rate kept: no mode change
        want.hdr = 0;         // already off
        want.scale = 125;     // already
        want.brightness = 80; // already
        QVERIFY(plan(now, want).isEmpty());

        want.width = 1280;
        want.height = 720;
        want.hdr = 1;
        want.scale = 100;
        want.brightness = 30;
        const Changes c = plan(now, want);
        QCOMPARE(*c.mode, (Mode{1280, 720, 120}));   // 144 Hz not offered: the highest
        QCOMPARE(*c.hdr, true);
        QCOMPARE(*c.scale, 100);
        QCOMPARE(*c.brightness, 30);
    }

    void planSkipsWhatTheMonitorCannotDo()
    {
        Monitor now = monitor1080();
        now.hdrSupported = false;
        now.brightnessSupported = false;
        now.scaleKnown = false;
        GameSettings want;
        want.width = 1024;   // no such mode
        want.height = 768;
        want.hdr = 1;
        want.scale = 100;
        want.brightness = 10;
        QVERIFY(plan(now, want).isEmpty());

        want = {};
        want.refresh = 60;   // only the rate
        QCOMPARE(*plan(monitor1080(), want).mode, (Mode{1920, 1080, 60}));
    }

    void undoKeepsTheDesktopValues()
    {
        const Monitor now = monitor1080();
        Changes c;
        c.mode = Mode{1280, 720, 60};
        c.scale = 100;
        Restore r = undoFor(now, c);
        QVERIFY(r.mode);
        QCOMPARE(*r.scale, 125);
        QVERIFY(!r.hdr);
        QVERIFY(!r.brightness);

        // a second step (after the mode change) adds HDR; a later scale value must not win
        Monitor later = now;
        later.scale = 100;
        Changes c2;
        c2.hdr = true;
        c2.scale = 150;
        merge(r, undoFor(later, c2));
        QCOMPARE(*r.scale, 125);
        QCOMPARE(*r.hdr, false);
        QCOMPARE(r.monitorId, now.id);

        const Restore back = restoreFromJson(toJson(r));
        QCOMPARE(back.mode, true);
        QCOMPARE(*back.scale, 125);
        QCOMPARE(*back.hdr, false);
        QVERIFY(!back.brightness);
        QVERIFY(restoreFromJson({}).isEmpty());
    }

    // --- this PC (read-only) -----------------------------------------------------

    void probeThisPc()
    {
        DisplayBackend& backend = displayBackend();
        if (!backend.supported())
            QSKIP("No display backend on this platform");
        const auto monitors = backend.monitors();
        qInfo().noquote() << "Displays:" << monitors.size() << "active," << backend.connectedDisplays()
                          << "connected, arrangement:" << topologyName(backend.topology());
        for (const auto& m : monitors) {
            qInfo().noquote() << QString("  %1%2 [%3] %4 x %5 @ %6 Hz, %7, %8 modes")
                                     .arg(m.name, m.primary ? " (main)" : "", m.deviceName)
                                     .arg(m.mode.width).arg(m.mode.height).arg(m.mode.refresh)
                                     .arg(orientationName(m.orientation)).arg(m.modes.size());
            qInfo().noquote() << QString("    scale %1 (recommended %2, %3 steps), HDR %4, brightness %5, profile %6")
                                     .arg(m.scaleKnown ? QString("%1 %").arg(m.scale) : QString("?"))
                                     .arg(m.recommendedScale).arg(m.scaleOptions.size())
                                     .arg(!m.hdrSupported ? "n/a" : m.hdrOn ? "on" : "off")
                                     .arg(m.brightnessSupported ? QString("%1 %").arg(m.brightness) : QString("n/a"))
                                     .arg(m.colorProfile.isEmpty() ? QString("-") : m.colorProfile);
            QVERIFY(!m.id.isEmpty());
            QVERIFY(m.mode.width > 0 && m.mode.height > 0);
            // the current mode is one of the listed ones
            const auto shown = orientedModes(m);
            QVERIFY(std::any_of(shown.begin(), shown.end(),
                                [&](const Mode& x) { return x.size() == m.mode.size(); }));
        }
    }
};

QTEST_GUILESS_MAIN(TstDisplay)
#include "tst_display.moc"
