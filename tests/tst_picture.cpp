#include <QtTest>

#include "stats/PictureInfo.h"

using namespace Picture;

namespace {

// Tekken 7 in a window on a 1920 x 1080 monitor, Magpie set to FSR.
Facts tekken()
{
    Facts f;
    f.gameRunning = true;
    f.windowFound = true;
    f.window = {1280, 720};
    f.monitor = {1920, 1080};
    f.monitorIndex = 2;
    f.monitorHz = 60;
    f.gameInFront = true;
    f.magpieMode = "FSR";
    f.magpieRunning = true;
    return f;
}

bool hasProblem(const Result& r, const QString& part)
{
    return std::any_of(r.problems.begin(), r.problems.end(), [&](const QString& p) { return p.contains(part); });
}

} // namespace

class TstPicture : public QObject
{
    Q_OBJECT
private slots:
    void percentAndDirection()
    {
        QCOMPARE(scalePercent({1280, 720}, {1920, 1080}), 150);
        QCOMPARE(scalePercent({3840, 2160}, {1920, 1080}), 50);
        QCOMPARE(scalePercent({1926, 2160}, {963, 1080}), 50);
        QCOMPARE(scalePercent({1280, 720}, {1920, 1200}), 150);   // letterboxed: the smaller ratio
        QCOMPARE(scalePercent({}, {1920, 1080}), 100);
        QCOMPARE(direction(150), QString("upscaling"));
        QCOMPARE(direction(50), QString("downscaling"));
        QCOMPARE(direction(100), QString("1:1"));
    }

    void notRunning()
    {
        const Result r = evaluate(Facts{});
        QCOMPARE(r.scaler, Scaler::None);
        QCOMPARE(r.picture, QString("Game not running"));
        QCOMPARE(overlayLines(Facts{}, r, {}).size(), std::size_t(2));
    }

    void magpieUpscales()
    {
        Facts f = tekken();
        f.magpieScaling = true;
        f.magpieSource = {1280, 720};
        f.magpieDest = {1920, 1080};
        const Result r = evaluate(f);
        QCOMPARE(r.scaler, Scaler::Magpie);
        QCOMPARE(r.scalerName, QString("Magpie FSR"));
        QCOMPARE(r.percent, 150);
        QVERIFY(r.picture.contains("150 %"));
        QVERIFY(r.picture.contains("upscaling"));
        QVERIFY(r.status.startsWith("Magpie FSR: 1280"));
        QVERIFY(r.problems.empty());
    }

    // What the user saw (2026-10-21): the window clamped to 1926 x 2160, Magpie fitting it into one monitor.
    void magpieShrinksAWindowBiggerThanTheMonitor()
    {
        Facts f = tekken();
        f.window = {1926, 2160};
        f.magpieScaling = true;
        f.magpieSource = {1926, 2160};
        f.magpieDest = {963, 1080};
        const Result r = evaluate(f);
        QCOMPARE(r.percent, 50);
        QVERIFY(r.picture.contains("downscaling"));
        QVERIFY(hasProblem(r, "bigger than its monitor"));
        QVERIFY(hasProblem(r, "Magpie only scales up"));
    }

    void magpieNotScaling()
    {
        Facts f = tekken();
        f.magpieRunning = false;
        QVERIFY(hasProblem(evaluate(f), "Magpie is not running"));

        f = tekken();
        f.fullScreen = true;
        f.window = {1920, 1080};
        const Result full = evaluate(f);
        QCOMPARE(full.scaler, Scaler::None);
        QCOMPARE(full.picture, QString("not scaling"));
        QVERIFY(full.status.startsWith("Magpie FSR: not scaling"));
        QVERIFY(hasProblem(full, "maximized or borderless"));

        f = tekken();
        f.gameInFront = false;
        f.foreground = "JConfigTK7R2.exe";
        const Result away = evaluate(f);
        QVERIFY(away.problems.empty());
        QVERIFY(away.picture.startsWith("waiting"));
        QVERIFY(away.picture.contains("JConfigTK7R2.exe is in front"));
        f.foreground.clear();
        QVERIFY(evaluate(f).picture.contains("no window is in front"));

        f = tekken();
        QVERIFY(hasProblem(evaluate(f), "magpie.log"));

        // the moment between Start and Magpie's scaling window is not a problem
        f = tekken();
        f.magpieRunning = false;
        f.magpieStarting = true;
        const Result starting = evaluate(f);
        QVERIFY(starting.problems.empty());
        QVERIFY(starting.starting);
        QCOMPARE(starting.picture, QString("starting"));
    }

    void magpiePausedWhileTheGameIsNotInFront()
    {
        Facts f = tekken();
        f.gameInFront = false;
        f.magpieScaling = true;
        f.magpieSource = {1280, 720};
        f.magpieDest = {1920, 1080};
        const Result r = evaluate(f);
        QCOMPARE(r.scaler, Scaler::Magpie);
        QVERIFY(r.picture.contains("paused"));
        QVERIFY(r.status.endsWith(", paused (no window is in front)"));
        QVERIFY(r.paused);
    }

    void downscaledViewWinsOverMagpie()
    {
        Facts f = tekken();
        f.viewRunning = true;
        f.viewRender = {3840, 2160};
        f.viewShown = {1920, 1080};
        const Result r = evaluate(f);
        QCOMPARE(r.scaler, Scaler::DownscaledView);
        QCOMPARE(r.percent, 50);
        QVERIFY(r.picture.contains("downscaling"));
    }

    void noScaler()
    {
        Facts f = tekken();
        f.magpieMode.clear();
        f.fullScreen = true;
        f.window = {1920, 1080};
        const Result r = evaluate(f);
        QCOMPARE(r.scaler, Scaler::None);
        QVERIFY(r.picture.startsWith("full screen 1920"));
        QVERIFY(r.status.startsWith("No scaler: "));

        f.fullScreen = false;
        f.window = {1926, 2160};
        QVERIFY(hasProblem(evaluate(f), "off the screen"));

        f.windowFound = false;
        QCOMPARE(evaluate(f).picture, QString("game window not found"));
    }

    void overlayShowsGameFilesAndStats()
    {
        Facts f = tekken();
        f.window = {1926, 2160};
        f.filesChanged = true;
        f.fileResolution = {3840, 2160};
        f.renderScale = 300;
        Stats::GameStats stats;
        stats.running = true;
        stats.fps.valid = true;
        stats.fps.fps = 27.4;
        stats.fps.low1Fps = 18;
        stats.gpu = 97;
        stats.gpuName = "NVIDIA GeForce GTX 1650";
        stats.power.known = true;
        stats.power.onBattery = true;
        stats.power.batteryPercent = 80;
        stats.warnings.push_back({"Integrated GPU", "The game renders on Intel."});

        const auto lines = overlayLines(f, evaluate(f), stats);
        const auto find = [&](const QString& label) -> Line {
            for (const Line& l : lines)
                if (l.label == label)
                    return l;
            return {};
        };
        QVERIFY(find("Game files").value.contains("3840"));
        QVERIFY(find("Game files").value.contains("300 %"));
        QVERIFY(find("Render scale").value.contains("11520"));
        QVERIFY(find("Render scale").value.contains("scaled down"));
        QVERIFY(find("Monitor").value.contains("60 Hz"));
        QVERIFY(find("FPS").value.startsWith("27"));
        QVERIFY(find("Load").value.contains("GeForce GTX 1650"));
        QVERIFY(find("Power").warn);
        const auto warnings = std::count_if(lines.begin(), lines.end(), [](const Line& l) { return l.warn && l.label.isEmpty(); });
        QCOMPARE(warnings, 3);   // Magpie not scaling (log), the window too big, the integrated GPU
    }
};

QTEST_APPLESS_MAIN(TstPicture)
#include "tst_picture.moc"
