#pragma once

#include <QList>
#include <QString>

/**
 * Pure logic: can this PC run HelixSR? The facts come from SystemProbe (Win32); this unit only
 * judges them, so it is unit-tested (tst_helix).
 *
 * HelixSR runs NVIDIA's DLSS network as Direct3D 12 compute shaders (shader model 6.6, 16-bit
 * math, wave operations). Its README names AMD RDNA 1 and newer as supported; Vega is
 * experimental and NVIDIA Pascal "should work". Anything else is untested.
 */
namespace Helix {

struct GpuInfo
{
    QString name;
    quint32 vendorId{0};
    quint64 videoMemoryMB{0};
    bool    software{false};     // WARP / Microsoft Basic Render Driver
    bool    d3d12{false};        // a Direct3D 12 device could be created
    int     shaderModel{0};      // highest supported, 66 = 6.6
    bool    native16Bit{false};  // native 16-bit shader math
    int     waveMin{0};          // smallest / largest SIMD width (32 on RDNA, 64 on GCN and Vega)
    int     waveMax{0};
    QString error;
};

struct SystemInfo
{
    QList<GpuInfo> gpus;
    qint64 freeDiskMB{-1};       // where HelixSR is stored; -1 = unknown
    bool   hasTar{true};         // tar.exe unpacks the download
};

enum class Level { Supported, Experimental, Unsupported };

enum class Family { Unknown, Rdna1, Rdna2, Rdna3, Rdna4, RdnaOther, Vega, Gcn, Nvidia, Intel };

struct Check
{
    Level   level{Level::Supported};
    QString what;     // "Graphics card"
    QString detail;   // "AMD Radeon RX 7900 XTX (RDNA 3)"
};

struct Verdict
{
    Level        level{Level::Unsupported};
    QString      summary;   // one line for the tab
    QString      gpuName;   // the card that would be used (empty if none)
    QList<Check> checks;

    QString details() const;   // the checks, one per line
};

constexpr qint64 kRequiredDiskMB = 1500;   // download + unpacked package + build

Family  familyOf(const GpuInfo& gpu);
QString familyName(Family family);
QString levelName(Level level);   // "supported" / "experimental" / "not supported"

Verdict evaluate(const SystemInfo& system);

} // namespace Helix
