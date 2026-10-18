#include "HelixSystem.h"

#include <algorithm>

#include <QRegularExpression>

namespace Helix {

namespace {

constexpr quint32 kAmd = 0x1002;
constexpr quint32 kNvidia = 0x10DE;
constexpr quint32 kIntel = 0x8086;

bool matches(const QString& name, const char* pattern)
{
    return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption).match(name).hasMatch();
}

// Supported > Experimental > Unsupported
int rank(Level level)
{
    switch (level) {
    case Level::Supported: return 2;
    case Level::Experimental: return 1;
    case Level::Unsupported: break;
    }
    return 0;
}

Level worse(Level a, Level b)
{
    return rank(a) <= rank(b) ? a : b;
}

struct GpuJudgement
{
    Level   level;
    QString detail;
};

GpuJudgement judge(const GpuInfo& gpu)
{
    const Family family = familyOf(gpu);
    const QString card = QString("%1 (%2)").arg(gpu.name, familyName(family));

    if (!gpu.d3d12)
        return {Level::Unsupported, QString("%1: no Direct3D 12 device (%2)")
                                        .arg(gpu.name, gpu.error.isEmpty() ? QString("driver too old?") : gpu.error)};
    if (gpu.shaderModel < 66)
        return {Level::Unsupported, QString("%1: shader model %2.%3 only, HelixSR needs 6.6 (update the graphics driver)")
                                        .arg(card).arg(gpu.shaderModel / 10).arg(gpu.shaderModel % 10)};

    switch (family) {
    case Family::Rdna1:
    case Family::Rdna2:
    case Family::Rdna3:
    case Family::Rdna4:
    case Family::RdnaOther:
        if (!gpu.native16Bit)
            return {Level::Experimental, card + ": no native 16-bit math, slower fallback"};
        return {Level::Supported, card};
    case Family::Vega:
        return {Level::Experimental, card + ": Vega is experimental in HelixSR"};
    case Family::Gcn:
        return {Level::Experimental, card + ": older than Vega, untested"};
    case Family::Nvidia:
        if (matches(gpu.name, R"(\bgtx\b)"))
            return {Level::Experimental, card + ": no DLSS on GTX cards, so HelixSR is useful here; HelixSR lists "
                                                "GTX 10 as 'should work', other GTX cards are untested"};
        return {Level::Experimental, card + ": untested; on RTX cards the game's own DLSS is better"};
    case Family::Intel:
    case Family::Unknown:
        break;
    }
    return {Level::Experimental, card + ": not a card HelixSR was tested on"};
}

} // namespace

Family familyOf(const GpuInfo& gpu)
{
    const QString& n = gpu.name;
    if (gpu.vendorId == kNvidia)
        return Family::Nvidia;
    if (gpu.vendorId == kIntel)
        return Family::Intel;
    if (gpu.vendorId != kAmd)
        return Family::Unknown;

    if (matches(n, R"(vega|radeon\s*vii\b)"))
        return Family::Vega;
    if (matches(n, R"(\brx\s*9\d{3}\b|\bw9\d{3}\b)"))
        return Family::Rdna4;
    if (matches(n, R"(\brx\s*7\d{3}\b|\bw7\d{3}\b|\b[78][1-9]0m\b)"))
        return Family::Rdna3;
    if (matches(n, R"(\brx\s*6\d{3}\b|\bw6\d{3}\b|\b6[1-9]0m\b)"))
        return Family::Rdna2;
    if (matches(n, R"(\brx\s*5\d{3}\b|\bw5\d{3}\b|\bpro\s*5\d{3}\b)"))
        return Family::Rdna1;
    if (matches(n, R"(\brx\s*[1-5]\d{2}\b|\br[579]\s|\bhd\s*\d{4}\b)"))
        return Family::Gcn;
    // A name like "AMD Radeon(TM) Graphics" says nothing; the SIMD width does: RDNA runs 32 lanes.
    if (gpu.waveMin == 32)
        return Family::RdnaOther;
    if (gpu.waveMin == 64)
        return Family::Gcn;
    return Family::Unknown;
}

QString familyName(Family family)
{
    switch (family) {
    case Family::Rdna1: return "RDNA 1";
    case Family::Rdna2: return "RDNA 2";
    case Family::Rdna3: return "RDNA 3";
    case Family::Rdna4: return "RDNA 4";
    case Family::RdnaOther: return "RDNA";
    case Family::Vega: return "Vega";
    case Family::Gcn: return "GCN";
    case Family::Nvidia: return "NVIDIA";
    case Family::Intel: return "Intel";
    case Family::Unknown: break;
    }
    return "unknown";
}

QString levelName(Level level)
{
    switch (level) {
    case Level::Supported: return "supported";
    case Level::Experimental: return "experimental";
    case Level::Unsupported: break;
    }
    return "not supported";
}

QString Verdict::details() const
{
    QStringList lines;
    for (const Check& c : checks)
        lines << QString("%1 %2: %3").arg(c.level == Level::Supported ? "OK  " : c.level == Level::Experimental ? "WARN" : "FAIL",
                                          c.what, c.detail);
    return lines.join('\n');
}

Verdict evaluate(const SystemInfo& system)
{
    Verdict v;

    // --- graphics card: the best hardware adapter decides ---
    const GpuInfo* best = nullptr;
    GpuJudgement bestJudgement{Level::Unsupported, {}};
    for (const GpuInfo& gpu : system.gpus) {
        if (gpu.software)
            continue;
        const GpuJudgement j = judge(gpu);
        if (!best || rank(j.level) > rank(bestJudgement.level)
            || (rank(j.level) == rank(bestJudgement.level) && gpu.videoMemoryMB > best->videoMemoryMB)) {
            best = &gpu;
            bestJudgement = j;
        }
    }
    if (!best) {
        v.checks << Check{Level::Unsupported, "Graphics card", "no hardware graphics adapter was found"};
    } else {
        v.gpuName = best->name;
        v.checks << Check{bestJudgement.level, "Graphics card", bestJudgement.detail};
        if (best->d3d12) {
            v.checks << Check{best->shaderModel >= 66 ? Level::Supported : Level::Unsupported, "Shader model 6.6",
                              QString("driver reports %1.%2").arg(best->shaderModel / 10).arg(best->shaderModel % 10)};
            v.checks << Check{best->native16Bit ? Level::Supported : Level::Experimental, "16-bit shader math",
                              best->native16Bit ? "native" : "not native (HelixSR falls back to 32-bit, slower)"};
            v.checks << Check{Level::Supported, "SIMD width",
                              best->waveMin == best->waveMax ? QString::number(best->waveMin)
                                                            : QString("%1 to %2").arg(best->waveMin).arg(best->waveMax)};
        }
    }

    // --- the machine ---
    if (system.freeDiskMB >= 0) {
        const bool enough = system.freeDiskMB >= kRequiredDiskMB;
        v.checks << Check{enough ? Level::Supported : Level::Unsupported, "Free disk space",
                          QString("%1 MB free, about %2 MB needed while setting up")
                              .arg(system.freeDiskMB).arg(kRequiredDiskMB)};
    }
    v.checks << Check{system.hasTar ? Level::Supported : Level::Unsupported, "tar.exe",
                      system.hasTar ? "found (unpacks the download)" : "missing (Windows 10 1803 or newer has it)"};
    v.checks << Check{system.hasPowerShell ? Level::Supported : Level::Unsupported, "Windows PowerShell",
                      system.hasPowerShell ? "found (runs HelixSR's setup)" : "missing"};

    v.level = Level::Supported;
    for (const Check& c : v.checks)
        v.level = worse(v.level, c.level);

    switch (v.level) {
    case Level::Supported:
        v.summary = QString("This PC can run HelixSR: %1.").arg(bestJudgement.detail);
        break;
    case Level::Experimental:
        v.summary = QString("HelixSR may run here, but it is experimental: %1.").arg(bestJudgement.detail);
        break;
    case Level::Unsupported: {
        QString reason = "see details";
        for (const Check& c : v.checks) {
            if (c.level == Level::Unsupported) {
                reason = c.what + ": " + c.detail;
                break;
            }
        }
        v.summary = "HelixSR cannot run on this PC - " + reason + ".";
        break;
    }
    }
    return v;
}

} // namespace Helix
