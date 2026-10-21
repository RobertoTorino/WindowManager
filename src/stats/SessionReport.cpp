#include "SessionReport.h"

#include <QJsonArray>

#include <algorithm>
#include <cmath>

namespace Report {

namespace {

const QString kTimes = QString::fromUtf8(" × ");
const QString kArrow = QString::fromUtf8(" → ");

QString number(double v)
{
    return v < 0 ? QString() : QString::number(v, 'f', 1);
}

double parseNumber(const QString& text)
{
    bool ok = false;
    const double v = text.toDouble(&ok);
    return ok ? v : -1;
}

QString quoted(const QString& text)
{
    QString t = text;
    return '"' + t.replace('"', "\"\"") + '"';
}

// One CSV line into its fields ("..." may hold commas and "" for a quote).
QStringList splitCsv(const QString& line)
{
    QStringList out;
    QString field;
    bool inQuotes = false;
    for (int i = 0; i < line.size(); ++i) {
        const QChar c = line[i];
        if (inQuotes) {
            if (c == '"' && i + 1 < line.size() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else if (c == '"') {
                inQuotes = false;
            } else {
                field += c;
            }
        } else if (c == '"') {
            inQuotes = true;
        } else if (c == ',') {
            out << field;
            field.clear();
        } else {
            field += c;
        }
    }
    out << field;
    return out;
}

// Running figures for one setup.
struct Totals
{
    Segment seg;
    double fps = 0, low1 = 0, gpu = 0, magpie = 0, cpu = 0;
    int nFps = 0, nLow1 = 0, nGpu = 0, nMagpie = 0, nCpu = 0;

    void add(const Sample& s)
    {
        ++seg.seconds;
        seg.percent = s.percent;
        const auto sum = [](double v, double& total, int& n) {
            if (v >= 0) {
                total += v;
                ++n;
            }
        };
        sum(s.fps, fps, nFps);
        sum(s.low1, low1, nLow1);
        sum(s.gpu, gpu, nGpu);
        sum(s.magpieGpu, magpie, nMagpie);
        sum(s.cpu, cpu, nCpu);
        seg.gpuPeak = std::max(seg.gpuPeak, s.gpu);
        seg.cpuPeak = std::max(seg.cpuPeak, s.cpu);
        seg.ramPeakMb = std::max(seg.ramPeakMb, s.ramMb);
        seg.vramPeakMb = std::max(seg.vramPeakMb, s.vramMb);
    }

    Segment result() const
    {
        Segment s = seg;
        const auto avg = [](double total, int n) { return n ? total / n : -1.0; };
        s.fpsAvg = avg(fps, nFps);
        s.low1Avg = avg(low1, nLow1);
        s.gpuAvg = avg(gpu, nGpu);
        s.magpieGpuAvg = avg(magpie, nMagpie);
        s.cpuAvg = avg(cpu, nCpu);
        return s;
    }
};

void put(QJsonObject& o, const char* key, double v)
{
    if (v >= 0)
        o[key] = std::round(v * 10) / 10;
}

double take(const QJsonObject& o, const char* key)
{
    return o.contains(key) ? o[key].toDouble(-1) : -1;
}

QJsonObject toJson(const Segment& s)
{
    QJsonObject o;
    o["setup"] = s.setup;
    o["seconds"] = s.seconds;
    o["scalePercent"] = s.percent;
    put(o, "fpsAvg", s.fpsAvg);
    put(o, "fps1LowAvg", s.low1Avg);
    put(o, "gpuAvg", s.gpuAvg);
    put(o, "gpuPeak", s.gpuPeak);
    put(o, "magpieGpuAvg", s.magpieGpuAvg);
    put(o, "cpuAvg", s.cpuAvg);
    put(o, "cpuPeak", s.cpuPeak);
    put(o, "ramPeakMb", s.ramPeakMb);
    put(o, "vramPeakMb", s.vramPeakMb);
    return o;
}

Segment segmentFromJson(const QJsonObject& o)
{
    Segment s;
    s.setup = o["setup"].toString();
    s.seconds = o["seconds"].toInt();
    s.percent = o["scalePercent"].toInt(100);
    s.fpsAvg = take(o, "fpsAvg");
    s.low1Avg = take(o, "fps1LowAvg");
    s.gpuAvg = take(o, "gpuAvg");
    s.gpuPeak = take(o, "gpuPeak");
    s.magpieGpuAvg = take(o, "magpieGpuAvg");
    s.cpuAvg = take(o, "cpuAvg");
    s.cpuPeak = take(o, "cpuPeak");
    s.ramPeakMb = take(o, "ramPeakMb");
    s.vramPeakMb = take(o, "vramPeakMb");
    return s;
}

} // namespace

QString samplesHeader()
{
    return "t,fps,fps_1low,cpu,gpu,magpie_gpu,ram_mb,vram_mb,setup,scale_percent,on_battery";
}

QString toCsv(const Sample& s)
{
    return QStringList{QString::number(s.t), number(s.fps), number(s.low1), number(s.cpu), number(s.gpu),
                       number(s.magpieGpu), number(s.ramMb), number(s.vramMb), quoted(s.setup),
                       QString::number(s.percent), s.onBattery ? "1" : "0"}
        .join(',');
}

std::optional<Sample> sampleFromCsv(const QString& line)
{
    const QStringList f = splitCsv(line.trimmed());
    if (f.size() < 11)
        return std::nullopt;
    bool ok = false;
    Sample s;
    s.t = f[0].toInt(&ok);
    if (!ok)
        return std::nullopt;   // the header, or a broken line
    s.fps = parseNumber(f[1]);
    s.low1 = parseNumber(f[2]);
    s.cpu = parseNumber(f[3]);
    s.gpu = parseNumber(f[4]);
    s.magpieGpu = parseNumber(f[5]);
    s.ramMb = parseNumber(f[6]);
    s.vramMb = parseNumber(f[7]);
    s.setup = f[8];
    s.percent = f[9].toInt();
    s.onBattery = f[10] == "1";
    return s;
}

std::vector<Sample> readSamples(const QString& csvText)
{
    std::vector<Sample> out;
    for (const QString& line : csvText.split('\n', Qt::SkipEmptyParts)) {
        if (auto s = sampleFromCsv(line))
            out.push_back(*s);
    }
    return out;
}

QString setupName(const QString& scaler, SetupState state, int fromW, int fromH, int toW, int toH, int windowW,
                  int windowH)
{
    if (state == SetupState::Paused)
        return scaler + " paused";
    if (state == SetupState::Starting)
        return scaler + " starting";
    if (state == SetupState::Scaling)
        return scaler + " " + QString::number(fromW) + kTimes + QString::number(fromH) + kArrow
               + QString::number(toW) + kTimes + QString::number(toH);
    const QString size = windowW > 0 && windowH > 0 ? " " + QString::number(windowW) + kTimes + QString::number(windowH)
                                                    : QString();
    if (scaler.isEmpty() || scaler == "None")
        return "No scaler" + size;
    return scaler + " not scaling" + size;
}

std::vector<Segment> segmentsOf(const std::vector<Sample>& samples)
{
    std::vector<Totals> totals;
    for (const Sample& s : samples) {
        auto it = std::find_if(totals.begin(), totals.end(), [&](const Totals& t) { return t.seg.setup == s.setup; });
        if (it == totals.end()) {
            totals.push_back({});
            totals.back().seg.setup = s.setup;
            it = totals.end() - 1;
        }
        it->add(s);
    }
    std::vector<Segment> out;
    for (const Totals& t : totals)
        out.push_back(t.result());
    return out;
}

Segment overallOf(const std::vector<Sample>& samples)
{
    Totals t;
    t.seg.setup = "Whole session";
    for (const Sample& s : samples)
        t.add(s);
    Segment s = t.result();
    s.percent = 100;
    return s;
}

Session summarize(Session meta, const std::vector<Sample>& samples)
{
    meta.segments = segmentsOf(samples);
    meta.overall = overallOf(samples);
    return meta;
}

QJsonObject toJson(const Session& s)
{
    QJsonObject o;
    o["schemaVersion"] = kSchemaVersion;
    o["game"] = s.game;
    o["exeName"] = s.exeName;
    if (s.steamAppId)
        o["steamAppId"] = qint64(s.steamAppId);
    o["start"] = s.start.toString(Qt::ISODate);
    o["end"] = s.end.toString(Qt::ISODate);
    o["secondsPlayed"] = qint64(s.start.secsTo(s.end));
    o["appVersion"] = s.appVersion;
    o["gpu"] = s.gpuName;
    o["monitor"] = s.monitor;
    o["samplesFile"] = s.samplesFile;
    o["warnings"] = QJsonArray::fromStringList(s.warnings);
    QJsonArray segments;
    for (const Segment& seg : s.segments)
        segments.append(toJson(seg));
    o["setups"] = segments;
    o["overall"] = toJson(s.overall);
    return o;
}

std::optional<Session> sessionFromJson(const QJsonObject& o)
{
    if (o["schemaVersion"].toInt() < 1 || o["game"].toString().isEmpty())
        return std::nullopt;
    Session s;
    s.game = o["game"].toString();
    s.exeName = o["exeName"].toString();
    s.steamAppId = quint32(o["steamAppId"].toInteger());
    s.start = QDateTime::fromString(o["start"].toString(), Qt::ISODate);
    s.end = QDateTime::fromString(o["end"].toString(), Qt::ISODate);
    s.appVersion = o["appVersion"].toString();
    s.gpuName = o["gpu"].toString();
    s.monitor = o["monitor"].toString();
    s.samplesFile = o["samplesFile"].toString();
    for (const auto& w : o["warnings"].toArray())
        s.warnings << w.toString();
    for (const auto& seg : o["setups"].toArray())
        s.segments.push_back(segmentFromJson(seg.toObject()));
    s.overall = segmentFromJson(o["overall"].toObject());
    return s;
}

QString sessionsHeader()
{
    return "start,end,game,exe,seconds,setups,fps_avg,fps_1low_avg,gpu_avg,magpie_gpu_avg,cpu_avg,ram_peak_mb,"
           "vram_peak_mb,gpu";
}

QString sessionsCsvLine(const Session& s)
{
    QStringList setups;
    for (const Segment& seg : s.segments)
        setups << seg.setup;
    const Segment& o = s.overall;
    return QStringList{s.start.toString(Qt::ISODate), s.end.toString(Qt::ISODate), quoted(s.game),
                       quoted(s.exeName), QString::number(s.start.secsTo(s.end)), quoted(setups.join(" | ")),
                       number(o.fpsAvg), number(o.low1Avg), number(o.gpuAvg), number(o.magpieGpuAvg),
                       number(o.cpuAvg), number(o.ramPeakMb), number(o.vramPeakMb), quoted(s.gpuName)}
        .join(',');
}

QString folderName(const QString& game)
{
    QString out;
    for (const QChar c : game.trimmed()) {
        const bool ok = (c.isLetterOrNumber() && c.unicode() < 128) || c == ' ' || c == '-' || c == '_' || c == '.';
        out += ok ? c : QChar('_');
    }
    while (out.endsWith('.') || out.endsWith(' '))
        out.chop(1);
    out = out.left(60);
    return out.isEmpty() ? QString("game") : out;
}

std::vector<Segment> compareSetups(std::vector<Session> sessions)
{
    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) { return a.start < b.start; });
    struct Merge
    {
        Segment seg;
        double w[5] = {0, 0, 0, 0, 0};   // fps, low1, gpu, magpie, cpu
        double sum[5] = {0, 0, 0, 0, 0};
    };
    std::vector<Merge> merged;
    for (const Session& session : sessions) {
        for (const Segment& s : session.segments) {
            auto it = std::find_if(merged.begin(), merged.end(), [&](const Merge& m) { return m.seg.setup == s.setup; });
            if (it == merged.end()) {
                merged.push_back({});
                merged.back().seg.setup = s.setup;
                it = merged.end() - 1;
            }
            Merge& m = *it;
            m.seg.seconds += s.seconds;
            m.seg.percent = s.percent;
            const double values[5] = {s.fpsAvg, s.low1Avg, s.gpuAvg, s.magpieGpuAvg, s.cpuAvg};
            for (int i = 0; i < 5; ++i) {
                if (values[i] >= 0) {
                    m.sum[i] += values[i] * s.seconds;
                    m.w[i] += s.seconds;
                }
            }
            m.seg.gpuPeak = std::max(m.seg.gpuPeak, s.gpuPeak);
            m.seg.cpuPeak = std::max(m.seg.cpuPeak, s.cpuPeak);
            m.seg.ramPeakMb = std::max(m.seg.ramPeakMb, s.ramPeakMb);
            m.seg.vramPeakMb = std::max(m.seg.vramPeakMb, s.vramPeakMb);
        }
    }
    std::vector<Segment> out;
    for (const Merge& m : merged) {
        Segment s = m.seg;
        double* targets[5] = {&s.fpsAvg, &s.low1Avg, &s.gpuAvg, &s.magpieGpuAvg, &s.cpuAvg};
        for (int i = 0; i < 5; ++i)
            *targets[i] = m.w[i] > 0 ? m.sum[i] / m.w[i] : -1;
        out.push_back(s);
    }
    return out;
}

QJsonObject toJson(const Comparison& c)
{
    QJsonObject o;
    o["schemaVersion"] = kSchemaVersion;
    o["time"] = c.time.toString(Qt::ISODate);
    o["game"] = c.game;
    o["setup"] = c.setup;
    o["source"] = QJsonArray{c.sourceW, c.sourceH};
    o["dest"] = QJsonArray{c.destW, c.destH};
    o["sharpnessSource"] = std::round(c.sharpSource * 100) / 100;
    o["sharpnessBilinear"] = std::round(c.sharpBilinear * 100) / 100;
    o["sharpnessScaled"] = std::round(c.sharpScaled * 100) / 100;
    o["gainPercent"] = std::round(c.gainPercent * 10) / 10;
    o["image"] = c.image;
    return o;
}

std::optional<Comparison> comparisonFromJson(const QJsonObject& o)
{
    if (o["schemaVersion"].toInt() < 1 || !o.contains("sharpnessScaled"))
        return std::nullopt;
    Comparison c;
    c.time = QDateTime::fromString(o["time"].toString(), Qt::ISODate);
    c.game = o["game"].toString();
    c.setup = o["setup"].toString();
    c.sourceW = o["source"].toArray().at(0).toInt();
    c.sourceH = o["source"].toArray().at(1).toInt();
    c.destW = o["dest"].toArray().at(0).toInt();
    c.destH = o["dest"].toArray().at(1).toInt();
    c.sharpSource = o["sharpnessSource"].toDouble();
    c.sharpBilinear = o["sharpnessBilinear"].toDouble();
    c.sharpScaled = o["sharpnessScaled"].toDouble();
    c.gainPercent = o["gainPercent"].toDouble();
    c.image = o["image"].toString();
    return c;
}

double sharpness(const std::vector<unsigned char>& gray, int width, int height)
{
    if (width < 3 || height < 3 || gray.size() < size_t(width) * size_t(height))
        return 0;
    double total = 0;
    const auto at = [&](int x, int y) { return double(gray[size_t(y) * width + x]); };
    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            const double gx = at(x + 1, y - 1) + 2 * at(x + 1, y) + at(x + 1, y + 1)
                              - at(x - 1, y - 1) - 2 * at(x - 1, y) - at(x - 1, y + 1);
            const double gy = at(x - 1, y + 1) + 2 * at(x, y + 1) + at(x + 1, y + 1)
                              - at(x - 1, y - 1) - 2 * at(x, y - 1) - at(x + 1, y - 1);
            total += (gx * gx + gy * gy) / 16;   // /16: back to the 0..255 range of one step, squared
        }
    }
    // RMS, not the mean: a blurred edge spreads the same change over more pixels, which keeps the mean
    // gradient but lowers the squared one
    return std::sqrt(total / (double(width - 2) * double(height - 2)));
}

double gainPercent(double scaled, double reference)
{
    return reference > 0 ? (scaled / reference - 1) * 100 : 0;
}

} // namespace Report
