#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

/**
 * Pure logic behind the game session reports (Reports tab): no OS calls, no widgets (tst_report).
 *
 * While a game runs the app keeps one sample a second in reports/<game>/<start>-samples.csv; when the
 * game ends, the session's summary goes to reports/<game>/<start>.json and a row to reports/sessions.csv.
 * The summary has one Segment per *setup* (what showed the picture and at what sizes, e.g.
 * "Magpie FSR 1600 × 900 → 1920 × 1080"), so the same game can be compared across setups. A picture
 * comparison (Magpie against a plain stretch of the same frame) is kept as compare-<time>.json + .png.
 *
 * No private data: the game's name and exe file name only, no user names, paths, PC name or serials.
 */
namespace Report {

constexpr int kSchemaVersion = 1;
constexpr int kMinSessionSeconds = 10;   // shorter sessions are not kept

// One second of a session. -1: not known.
struct Sample
{
    int     t{0};             // seconds since the session started
    double  fps{-1};
    double  low1{-1};         // 1 % low
    double  cpu{-1};
    double  gpu{-1};          // the game's busiest GPU engine
    double  magpieGpu{-1};    // Magpie's own GPU use
    double  ramMb{-1};
    double  vramMb{-1};
    QString setup;            // see setupName
    int     percent{100};     // the scale of that setup
    bool    onBattery{false};
};

QString samplesHeader();
QString toCsv(const Sample& s);
std::optional<Sample> sampleFromCsv(const QString& line);
std::vector<Sample> readSamples(const QString& csvText);   // skips the header and broken lines

// What the scaler did at that second.
enum class SetupState { Scaling, Paused, Starting, Idle };   // Idle: not scaling, or no scaler at all

// "Magpie FSR 1600 × 900 → 1920 × 1080", "Downscaled view 3840 × 2160 → 1920 × 1080", "Magpie FSR paused",
// "Magpie FSR starting", "Magpie FSR not scaling 1920 × 1080", "No scaler 1920 × 1080".
// scaler: Picture's scalerName; from/to: what it scales (Scaling); window: the game's drawn area (Idle).
QString setupName(const QString& scaler, SetupState state, int fromW, int fromH, int toW, int toH, int windowW,
                  int windowH);

// The figures for one setup (or the whole session). -1: no value seen.
struct Segment
{
    QString setup;
    int     seconds{0};
    int     percent{100};
    double  fpsAvg{-1};
    double  low1Avg{-1};
    double  gpuAvg{-1};
    double  gpuPeak{-1};
    double  magpieGpuAvg{-1};
    double  cpuAvg{-1};
    double  cpuPeak{-1};
    double  ramPeakMb{-1};
    double  vramPeakMb{-1};
};

// One Segment per setup, in the order each first appears; overall: every sample.
std::vector<Segment> segmentsOf(const std::vector<Sample>& samples);
Segment overallOf(const std::vector<Sample>& samples);

struct Session
{
    QString   game;           // the saved name
    QString   exeName;        // file name only
    quint32   steamAppId{0};
    QDateTime start;
    QDateTime end;
    QString   appVersion;
    QString   gpuName;
    QString   monitor;        // "1920 × 1080 · 120 Hz"
    QString   samplesFile;    // file name next to the JSON
    QStringList warnings;
    std::vector<Segment> segments;
    Segment   overall;
};

Session summarize(Session meta, const std::vector<Sample>& samples);   // fills segments and overall

QJsonObject toJson(const Session& s);
std::optional<Session> sessionFromJson(const QJsonObject& o);

// reports/sessions.csv: one row per session, all games.
QString sessionsHeader();
QString sessionsCsvLine(const Session& s);

// A file-system-safe folder name for a game: letters, digits, space, '-', '_', '.'; others become '_'.
QString folderName(const QString& game);

// A game's setups over all its sessions, each merged (averages weighted by seconds), in the order each
// was first seen: the comparison chart.
std::vector<Segment> compareSetups(std::vector<Session> sessions);

// A picture comparison: the same moment as the game drew it, stretched plainly (bilinear) and as the scaler
// showed it. Sharpness: see sharpness(); gain: scaled against bilinear, percent.
struct Comparison
{
    QDateTime time;
    QString   game;
    QString   setup;
    int       sourceW{0};
    int       sourceH{0};
    int       destW{0};
    int       destH{0};
    double    sharpSource{0};     // the game's own frame, 1:1
    double    sharpBilinear{0};
    double    sharpScaled{0};
    double    gainPercent{0};
    QString   image;              // the side-by-side PNG, file name next to the JSON
};
QJsonObject toJson(const Comparison& c);
std::optional<Comparison> comparisonFromJson(const QJsonObject& o);

// RMS gradient of the luminance (Sobel, "Tenengrad"), 0..~255: higher = sharper. Blur lowers it even where
// the mean gradient stays the same. `gray` is width*height
// bytes, row by row.
double sharpness(const std::vector<unsigned char>& gray, int width, int height);
double gainPercent(double scaled, double reference);   // (scaled / reference - 1) * 100; 0 without a reference

} // namespace Report
