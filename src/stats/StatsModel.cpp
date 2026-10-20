#include "StatsModel.h"

#include <QRegularExpression>
#include <QStringList>

#include <algorithm>
#include <map>
#include <utility>

namespace Stats {

namespace {

constexpr qint64 kLowWindowNs = 5'000'000'000;   // 1 % low and worst frame: the last 5 s
constexpr qint64 kStaleNs     = 3'000'000'000;   // no present for this long: no frame rate
constexpr double kBusyCpu     = 95.0;
constexpr double kBusyGpu     = 10.0;   // percent: below it, which adapter renders says little
constexpr quint64 kDedicatedMin = 1024ull * 1024 * 1024;

QString percent(double value)
{
    return QString::number(qRound(value)) + " %";
}

} // namespace

std::optional<GpuInstance> parseGpuInstance(const QString& instance)
{
    static const QRegularExpression re(
        "^pid_(\\d+)_luid_0x([0-9A-Fa-f]{1,8})_0x([0-9A-Fa-f]{1,8})_phys_\\d+(?:_eng_\\d+_engtype_(.*))?$");
    const auto m = re.match(instance);
    if (!m.hasMatch())
        return std::nullopt;
    GpuInstance gpu;
    gpu.pid    = m.captured(1).toLongLong();
    gpu.luid   = (m.captured(2).toULongLong(nullptr, 16) << 32) | m.captured(3).toULongLong(nullptr, 16);
    gpu.engine = m.captured(4);
    return gpu;
}

GpuUse gpuUseFor(qint64 pid, const std::vector<CounterValue>& engines)
{
    std::map<std::pair<quint64, QString>, double> perEngine;
    for (const auto& counter : engines) {
        const auto gpu = parseGpuInstance(counter.instance);
        if (gpu && gpu->pid == pid && !gpu->engine.isEmpty())
            perEngine[{gpu->luid, gpu->engine}] += counter.value;
    }
    GpuUse use;
    bool found = false;
    for (const auto& [key, value] : perEngine) {
        // the busiest engine; on a tie the 3D engine names the adapter
        if (!found || value > use.percent || (value == use.percent && key.second == "3D")) {
            use.percent = value;
            use.luid    = key.first;
            found = true;
        }
    }
    use.percent = std::clamp(use.percent, 0.0, 100.0);
    return use;
}

quint64 videoMemoryFor(qint64 pid, const std::vector<CounterValue>& memory)
{
    quint64 total = 0;
    for (const auto& counter : memory) {
        const auto gpu = parseGpuInstance(counter.instance);
        if (gpu && gpu->pid == pid && gpu->engine.isEmpty() && counter.value > 0)
            total += static_cast<quint64>(counter.value);
    }
    return total;
}

double cpuPercent(qint64 busy100ns, qint64 wall100ns, int cpus)
{
    if (busy100ns <= 0 || wall100ns <= 0 || cpus <= 0)
        return 0;
    return std::clamp(100.0 * double(busy100ns) / (double(wall100ns) * cpus), 0.0, 100.0);
}

FrameRate frameRate(const std::vector<qint64>& presentsNs, qint64 nowNs, qint64 windowNs)
{
    FrameRate rate;
    if (presentsNs.size() < 2 || nowNs - presentsNs.back() > kStaleNs)
        return rate;

    // The window ends at the newest present, not at "now": present events arrive in batches,
    // up to a second late.
    const qint64 from = presentsNs.back() - windowNs;
    std::vector<qint64> frames;   // frame times, ns
    for (size_t i = 1; i < presentsNs.size(); ++i) {
        if (presentsNs[i - 1] >= from && presentsNs[i] > presentsNs[i - 1])
            frames.push_back(presentsNs[i] - presentsNs[i - 1]);
    }
    if (frames.empty())
        return rate;

    qint64 sum = 0;
    for (qint64 f : frames)
        sum += f;
    rate.valid = true;
    rate.fps = 1e9 * double(frames.size()) / double(sum);

    std::sort(frames.begin(), frames.end(), std::greater<>());
    const size_t slowest = std::max<size_t>(1, frames.size() / 100);
    qint64 slowSum = 0;
    for (size_t i = 0; i < slowest; ++i)
        slowSum += frames[i];
    rate.low1Fps      = 1e9 * double(slowest) / double(slowSum);
    rate.worstFrameMs = double(frames.front()) / 1e6;
    return rate;
}

const Adapter* dedicatedAdapter(const std::vector<Adapter>& adapters)
{
    const Adapter* best = nullptr;
    for (const auto& a : adapters) {
        if (!best || a.dedicatedBytes > best->dedicatedBytes)
            best = &a;
    }
    return best && best->dedicatedBytes >= kDedicatedMin ? best : nullptr;
}

const Adapter* adapterByLuid(const std::vector<Adapter>& adapters, quint64 luid)
{
    for (const auto& a : adapters) {
        if (a.luid == luid)
            return &a;
    }
    return nullptr;
}

std::vector<Warning> warningsFor(const GameStats& stats, const std::vector<Adapter>& adapters)
{
    std::vector<Warning> out;
    if (!stats.running)
        return out;

    const Power& p = stats.power;
    if (p.known && p.onBattery)
        out.push_back({"On battery",
                       "The PC runs on its battery. Windows and the graphics driver then lower the CPU and GPU "
                       "speed, often to less than half. Plug in the charger."});
    if (p.known && p.batterySaver)
        out.push_back({"Battery saver on",
                       "Battery saver limits background work and the CPU speed. Switch it off under "
                       "Settings > System > Power & battery."});
    if (p.efficiencyMode)
        out.push_back({"Power-saving mode",
                       QString("The power mode is \"%1\", which favours battery life over speed. Choose \"Best "
                               "performance\" under Settings > System > Power & battery.").arg(p.mode)});

    // Only while the game really draws: a splash screen may start on another adapter for a moment.
    if (stats.gpu && *stats.gpu >= kBusyGpu && !stats.gpuName.isEmpty()) {
        const Adapter* dedicated = dedicatedAdapter(adapters);
        if (dedicated && dedicated->name != stats.gpuName)
            out.push_back({"Integrated GPU",
                           QString("The game renders on %1, not on %2. On the GPU & Power tab, set the game to High "
                                   "performance, then restart it.")
                               .arg(stats.gpuName, dedicated->name)});
    }

    if (stats.systemCpu && *stats.systemCpu >= kBusyCpu)
        out.push_back({"CPU fully used",
                       "All CPU cores are busy. Another program may be using the CPU: check Task Manager."});
    return out;
}

QString shortAdapterName(const QString& name)
{
    QString s = name;
    s.remove("(R)", Qt::CaseInsensitive).remove("(TM)", Qt::CaseInsensitive);
    s = s.simplified();
    // the vendor only: "GeForce" / "Radeon" stay, so "AMD Radeon 780M" does not become "780M"
    for (const char* vendor : {"NVIDIA ", "AMD ", "Intel "}) {
        if (s.startsWith(vendor, Qt::CaseInsensitive))
            return s.mid(int(qstrlen(vendor)));
    }
    return s;
}

GpuPreference gpuPreferenceFrom(const QString& value)
{
    for (const QString& entry : value.split(';', Qt::SkipEmptyParts)) {
        const QString key = entry.section('=', 0, 0).trimmed();
        if (key.compare("GpuPreference", Qt::CaseInsensitive) != 0)
            continue;
        const int n = entry.section('=', 1).trimmed().toInt();
        if (n == 1)
            return GpuPreference::PowerSaving;
        if (n == 2)
            return GpuPreference::HighPerformance;
        return GpuPreference::Auto;
    }
    return GpuPreference::Auto;
}

QString withGpuPreference(const QString& value, GpuPreference preference)
{
    QStringList entries;
    for (const QString& entry : value.split(';', Qt::SkipEmptyParts)) {
        if (entry.section('=', 0, 0).trimmed().compare("GpuPreference", Qt::CaseInsensitive) != 0)
            entries << entry.trimmed();
    }
    entries << QString("GpuPreference=%1").arg(int(preference));   // Settings writes 0 for "Let Windows decide" too
    return entries.join(';') + ';';
}

QString gpuPreferenceName(GpuPreference preference)
{
    switch (preference) {
    case GpuPreference::PowerSaving:     return "Power saving";
    case GpuPreference::HighPerformance: return "High performance";
    case GpuPreference::Auto:            break;
    }
    return "Let Windows decide";
}

QString powerModeName(PowerMode mode)
{
    switch (mode) {
    case PowerMode::Efficiency:  return "Best power efficiency";
    case PowerMode::Performance: return "Best performance";
    case PowerMode::Balanced:    break;
    }
    return "Balanced";
}

QString formatBytes(quint64 bytes)
{
    constexpr double kMb = 1024.0 * 1024.0;
    if (bytes >= 1024ull * 1024 * 1024)
        return QString::number(double(bytes) / (kMb * 1024.0), 'f', 1) + " GB";
    return QString::number(qRound64(double(bytes) / kMb)) + " MB";
}

QString compactLine(const GameStats& stats)
{
    if (!stats.running)
        return "Game not running";
    QStringList parts;
    parts << (stats.fps.valid ? QString::number(qRound(stats.fps.fps)) + " FPS" : QString("- FPS"));
    if (stats.cpu)
        parts << "CPU " + percent(*stats.cpu);
    if (stats.gpu)
        parts << "GPU " + percent(*stats.gpu);
    return parts.join(" · ");
}

QString summaryLine(const GameStats& stats)
{
    if (!stats.running)
        return "game not running";

    QStringList parts;
    if (stats.fps.valid)
        parts << QString("FPS %1 (1% low %2, worst frame %3 ms)")
                     .arg(qRound(stats.fps.fps)).arg(qRound(stats.fps.low1Fps))
                     .arg(stats.fps.worstFrameMs, 0, 'f', 1);
    else if (stats.fpsSource == FpsSource::NoPermission)
        parts << "FPS n/a (needs administrator rights)";
    else if (stats.fpsSource == FpsSource::Running)
        parts << "FPS - (no frames seen)";
    else
        parts << "FPS n/a";

    if (stats.cpu)
        parts << "CPU " + percent(*stats.cpu)
                     + (stats.systemCpu ? " (system " + percent(*stats.systemCpu) + ")" : QString());
    if (stats.gpu)
        parts << "GPU " + percent(*stats.gpu) + (stats.gpuName.isEmpty() ? QString() : " on " + stats.gpuName);
    if (stats.ram)
        parts << "RAM " + formatBytes(*stats.ram);
    if (stats.vram)
        parts << "VRAM " + formatBytes(*stats.vram);

    const Power& p = stats.power;
    if (p.known) {
        QString power = p.onBattery ? "on battery" : "AC power";
        if (p.batteryPercent >= 0)
            power += QString(" (battery %1 %)").arg(p.batteryPercent);
        if (!p.plan.isEmpty())
            power += ", plan " + p.plan;
        if (!p.mode.isEmpty())
            power += ", mode " + p.mode;
        parts << power;
    }
    return parts.join(", ");
}

void Sampler::reset()
{
    m_pid = 0;
    m_previous.reset();
    m_presents.clear();
}

GameStats Sampler::update(qint64 pid, const RawSample& raw, const std::vector<Adapter>& adapters)
{
    if (pid != m_pid) {
        reset();
        m_pid = pid;
    }

    GameStats stats;
    stats.power     = raw.power;
    stats.fpsSource = raw.fpsSource;
    stats.fpsError  = raw.fpsError;
    stats.running   = pid != 0 && raw.processAlive;
    if (!stats.running) {
        m_previous.reset();
        m_presents.clear();
        return stats;
    }

    stats.ram = raw.workingSetBytes;
    if (!raw.gpuMemory.empty())
        stats.vram = videoMemoryFor(pid, raw.gpuMemory);
    if (!raw.gpuEngines.empty()) {
        const GpuUse use = gpuUseFor(pid, raw.gpuEngines);
        stats.gpu = use.percent;
        if (const Adapter* a = adapterByLuid(adapters, use.luid))
            stats.gpuName = a->name;
    }

    if (m_previous && raw.timeNs > m_previous->timeNs) {
        stats.cpu = cpuPercent(raw.processCpu100ns - m_previous->processCpu100ns,
                               (raw.timeNs - m_previous->timeNs) / 100, raw.cpus);
        stats.systemCpu = cpuPercent(raw.systemBusy100ns - m_previous->systemBusy100ns,
                                     raw.systemTotal100ns - m_previous->systemTotal100ns, 1);
    }

    m_presents.insert(m_presents.end(), raw.presentsNs.begin(), raw.presentsNs.end());
    if (!std::is_sorted(m_presents.begin(), m_presents.end()))   // one batch per CPU buffer
        std::sort(m_presents.begin(), m_presents.end());
    if (!m_presents.empty()) {
        const qint64 keepFrom = m_presents.back() - kLowWindowNs;
        m_presents.erase(m_presents.begin(),
                         std::lower_bound(m_presents.begin(), m_presents.end(), keepFrom));
    }
    stats.fps = frameRate(m_presents, raw.timeNs, kFpsWindowNs);
    if (stats.fps.valid) {
        const FrameRate longer = frameRate(m_presents, raw.timeNs, kLowWindowNs);
        stats.fps.low1Fps      = longer.low1Fps;
        stats.fps.worstFrameMs = longer.worstFrameMs;
    }

    stats.warnings = warningsFor(stats, adapters);

    RawSample previous;   // only the counters are needed next time
    previous.timeNs           = raw.timeNs;
    previous.processCpu100ns  = raw.processCpu100ns;
    previous.systemBusy100ns  = raw.systemBusy100ns;
    previous.systemTotal100ns = raw.systemTotal100ns;
    m_previous = previous;
    return stats;
}

} // namespace Stats
