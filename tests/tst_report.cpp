#include <QtTest>

#include "stats/SessionReport.h"

using namespace Report;

namespace {

Sample sample(int t, double fps, double gpu, const QString& setup, double magpie = -1)
{
    Sample s;
    s.t = t;
    s.fps = fps;
    s.low1 = fps - 10;
    s.cpu = 5;
    s.gpu = gpu;
    s.magpieGpu = magpie;
    s.ramMb = 1000 + t;
    s.vramMb = 200;
    s.setup = setup;
    return s;
}

const QString kMagpie = QString::fromUtf8("Magpie FSR 1600 × 900 → 1920 × 1080");
const QString kNative = QString::fromUtf8("No scaler 1920 × 1080");

} // namespace

class TstReport : public QObject
{
    Q_OBJECT
private slots:
    void csvRoundTrip()
    {
        Sample s = sample(12, 59.6, 29.04, kMagpie, 6);
        s.percent = 120;
        s.onBattery = true;
        const auto back = sampleFromCsv(toCsv(s));
        QVERIFY(back);
        QCOMPARE(back->t, 12);
        QCOMPARE(back->fps, 59.6);
        QCOMPARE(back->gpu, 29.0);   // one decimal
        QCOMPARE(back->magpieGpu, 6.0);
        QCOMPARE(back->setup, kMagpie);
        QCOMPARE(back->percent, 120);
        QVERIFY(back->onBattery);

        Sample unknown;   // nothing measured: empty fields, back as -1
        unknown.setup = "a \"quoted\", setup";
        const auto u = sampleFromCsv(toCsv(unknown));
        QVERIFY(u);
        QCOMPARE(u->fps, -1.0);
        QCOMPARE(u->setup, unknown.setup);

        QVERIFY(!sampleFromCsv(samplesHeader()));
        QVERIFY(!sampleFromCsv("1,2,3"));
        const auto all = readSamples(samplesHeader() + "\n" + toCsv(s) + "\r\nbroken\n" + toCsv(s) + "\n");
        QCOMPARE(all.size(), std::size_t(2));
    }

    void setupNames()
    {
        using S = SetupState;
        QCOMPARE(setupName("Magpie FSR", S::Scaling, 1600, 900, 1920, 1080, 1600, 900), kMagpie);
        QCOMPARE(setupName("Magpie FSR", S::Paused, 0, 0, 0, 0, 1600, 900), QString("Magpie FSR paused"));
        QCOMPARE(setupName("Magpie FSR", S::Starting, 0, 0, 0, 0, 0, 0), QString("Magpie FSR starting"));
        QCOMPARE(setupName("None", S::Idle, 0, 0, 0, 0, 1920, 1080), kNative);
        QCOMPARE(setupName("Magpie FSR", S::Idle, 0, 0, 0, 0, 1920, 1080),
                 QString::fromUtf8("Magpie FSR not scaling 1920 × 1080"));
        QCOMPARE(setupName("None", S::Idle, 0, 0, 0, 0, 0, 0), QString("No scaler"));
    }

    void segmentsPerSetup()
    {
        std::vector<Sample> samples{sample(0, -1, 50, kNative), sample(1, 60, 30, kNative),
                                    sample(2, 58, 28, kMagpie, 6), sample(3, 60, 30, kMagpie, 8),
                                    sample(4, 62, 32, kNative)};
        const auto segs = segmentsOf(samples);
        QCOMPARE(segs.size(), std::size_t(2));
        QCOMPARE(segs[0].setup, kNative);   // first seen first
        QCOMPARE(segs[0].seconds, 3);
        QCOMPARE(segs[0].fpsAvg, 61.0);     // the unknown FPS is left out
        QCOMPARE(segs[0].gpuPeak, 50.0);
        QCOMPARE(segs[0].magpieGpuAvg, -1.0);
        QCOMPARE(segs[1].fpsAvg, 59.0);
        QCOMPARE(segs[1].magpieGpuAvg, 7.0);
        QCOMPARE(segs[1].ramPeakMb, 1003.0);

        const Segment all = overallOf(samples);
        QCOMPARE(all.seconds, 5);
        QCOMPARE(all.fpsAvg, 60.0);
    }

    void sessionJsonAndCsv()
    {
        Session meta;
        meta.game = "TEKKEN7FRR2";
        meta.exeName = "TekkenGame-Win64-Shipping.exe";
        meta.start = QDateTime(QDate(2026, 10, 21), QTime(1, 2, 47));
        meta.end = meta.start.addSecs(302);
        meta.gpuName = "NVIDIA GeForce GTX 1650";
        meta.samplesFile = "20261021-010247-samples.csv";
        meta.warnings << "Integrated GPU";
        const Session s = summarize(meta, {sample(0, 60, 30, kNative), sample(1, 58, 28, kMagpie, 6)});

        const QJsonObject o = toJson(s);
        QCOMPARE(o["schemaVersion"].toInt(), kSchemaVersion);
        QCOMPARE(o["secondsPlayed"].toInt(), 302);
        QVERIFY(!o.contains("steamAppId"));
        const QString text = QString::fromUtf8(QJsonDocument(o).toJson());
        QVERIFY(!text.contains("Users"));   // nothing private: no paths at all
        const auto back = sessionFromJson(o);
        QVERIFY(back);
        QCOMPARE(back->game, s.game);
        QCOMPARE(back->start, s.start);
        QCOMPARE(back->segments.size(), std::size_t(2));
        QCOMPARE(back->segments[1].magpieGpuAvg, 6.0);
        QCOMPARE(back->segments[0].magpieGpuAvg, -1.0);   // not written, read back as unknown
        QCOMPARE(back->warnings, QStringList{"Integrated GPU"});
        QVERIFY(!sessionFromJson(QJsonObject{}));

        const QString line = sessionsCsvLine(s);
        QVERIFY(line.startsWith("2026-10-21T01:02:47,2026-10-21T01:07:49,\"TEKKEN7FRR2\""));
        QVERIFY(line.contains(QString::fromUtf8("No scaler 1920 × 1080 | Magpie FSR")));
        QCOMPARE(line.count(',') - QString::fromUtf8(s.gpuName.toUtf8()).count(','), sessionsHeader().count(','));
    }

    void folderNames()
    {
        QCOMPARE(folderName("TEKKEN7FRR2"), QString("TEKKEN7FRR2"));
        QCOMPARE(folderName("Silent Hill: f"), QString("Silent Hill_ f"));
        QCOMPARE(folderName("a/b\\c?"), QString("a_b_c_"));
        QCOMPARE(folderName("  game.  "), QString("game"));
        QCOMPARE(folderName(""), QString("game"));
    }

    void compareAcrossSessions()
    {
        Session first;
        first.start = QDateTime(QDate(2026, 10, 20), QTime(10, 0));
        Segment n1;
        n1.setup = kNative;
        n1.seconds = 100;
        n1.fpsAvg = 60;
        n1.gpuAvg = 30;
        first.segments = {n1};
        Session second;
        second.start = QDateTime(QDate(2026, 10, 21), QTime(10, 0));
        Segment m;
        m.setup = kMagpie;
        m.seconds = 50;
        m.fpsAvg = 58;
        m.gpuAvg = 28;
        m.magpieGpuAvg = 6;
        Segment n2 = n1;
        n2.seconds = 300;
        n2.fpsAvg = 40;
        second.segments = {m, n2};

        const auto setups = compareSetups({second, first});   // any order in: sorted by start
        QCOMPARE(setups.size(), std::size_t(2));
        QCOMPARE(setups[0].setup, kNative);
        QCOMPARE(setups[0].seconds, 400);
        QCOMPARE(setups[0].fpsAvg, 45.0);   // (60*100 + 40*300) / 400
        QCOMPARE(setups[0].magpieGpuAvg, -1.0);
        QCOMPARE(setups[1].magpieGpuAvg, 6.0);
    }

    void comparisonJson()
    {
        Comparison c;
        c.time = QDateTime(QDate(2026, 10, 21), QTime(2, 0));
        c.game = "TEKKEN7FRR2";
        c.setup = kMagpie;
        c.sourceW = 1600;
        c.sourceH = 900;
        c.destW = 1920;
        c.destH = 1080;
        c.sharpBilinear = 10;
        c.sharpScaled = 12.345;
        c.gainPercent = gainPercent(c.sharpScaled, c.sharpBilinear);
        c.image = "compare-20261021-020000.png";
        const auto back = comparisonFromJson(toJson(c));
        QVERIFY(back);
        QCOMPARE(back->destH, 1080);
        QCOMPARE(back->sharpScaled, 12.35);
        QCOMPARE(back->gainPercent, 23.5);
        QVERIFY(!comparisonFromJson(QJsonObject{}));
    }

    void sharpnessMeasuresEdges()
    {
        const int w = 32, h = 32;
        std::vector<unsigned char> flat(w * h, 128);
        QCOMPARE(sharpness(flat, w, h), 0.0);

        std::vector<unsigned char> hard(w * h), soft(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                hard[y * w + x] = x < 16 ? 0 : 255;                              // a hard edge
                soft[y * w + x] = static_cast<unsigned char>(std::clamp((x - 12) * 32, 0, 255));   // a ramp
            }
        }
        // the same change from black to white: hard in one step, soft over eight pixels
        QVERIFY(sharpness(hard, w, h) > 1.5 * sharpness(soft, w, h));
        QVERIFY(sharpness(soft, w, h) > 0);
        QCOMPARE(sharpness(hard, 2, 2), 0.0);   // too small
        QCOMPARE(gainPercent(12, 10), 20.0);
        QCOMPARE(gainPercent(5, 0), 0.0);
    }
};

QTEST_APPLESS_MAIN(TstReport)
#include "tst_report.moc"
