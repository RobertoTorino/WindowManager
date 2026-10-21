#pragma once

#include <QDate>
#include <QString>
#include <QtGlobal>

#include <optional>
#include <vector>

/**
 * Pure logic behind the game stats bar: no OS calls, no widgets (tst_stats).
 *
 * StatsBackend reads raw numbers once a second (process CPU time, the GPU performance
 * counters, present events, the power state); a Sampler turns two of those in a row into
 * percentages, FPS and the warnings that explain a slow game (on battery, wrong GPU).
 */
namespace Stats {

// A graphics adapter. luid is DXGI's LUID as (HighPart << 32) | LowPart.
struct Adapter
{
    quint64 luid{0};
    QString name;
    quint64 dedicatedBytes{0};   // video memory of its own; integrated GPUs have little or none
    quint32 vendorId{0};         // PCI ids (Driver::kNvidia...)
    quint32 deviceId{0};
    QString driverVersion;       // Windows' number, "32.0.16.1742"; empty: not found
    QDate   driverDate;
};

// One value of a wildcard performance counter ("GPU Engine(*)\Utilization Percentage").
struct CounterValue
{
    QString instance;
    double  value{0};
};

// "pid_23412_luid_0x00000000_0x0000F02F_phys_0_eng_0_engtype_3D"
// "pid_23412_luid_0x00000000_0x0000F02F_phys_0"   (GPU Process Memory: no engine)
struct GpuInstance
{
    qint64  pid{0};
    quint64 luid{0};
    QString engine;   // "3D", "Copy", "VideoDecode"...; empty for the memory counters
};
std::optional<GpuInstance> parseGpuInstance(const QString& instance);

// Like Task Manager: per adapter and engine type the sum over the process's engines, and the
// busiest of those. luid is the adapter of that engine (0: no counter for this process).
struct GpuUse
{
    double  percent{0};
    quint64 luid{0};
};
GpuUse gpuUseFor(qint64 pid, const std::vector<CounterValue>& engines);

// Dedicated video memory of the process, summed over the adapters (bytes).
quint64 videoMemoryFor(qint64 pid, const std::vector<CounterValue>& memory);

// CPU use over one interval: busy time (100 ns units) against wall time on `cpus` cores.
// 100 % = every core busy, as in Task Manager.
double cpuPercent(qint64 busy100ns, qint64 wall100ns, int cpus);

// Frame rate from present timestamps (ns, ascending) seen in the last `windowNs` before nowNs.
struct FrameRate
{
    bool   valid{false};       // at least two presents in the window
    double fps{0};
    double low1Fps{0};         // 1 % low: the frame rate of the slowest 1 % of frame times
    double worstFrameMs{0};
};
FrameRate frameRate(const std::vector<qint64>& presentsNs, qint64 nowNs, qint64 windowNs);

enum class FpsSource {
    Unsupported,   // this system cannot count frames
    NoPermission,  // needs administrator rights (or the Performance Log Users group)
    Failed,        // the trace could not be started for another reason
    Running,
};

// Windows 11's power mode (Settings > System > Power & battery); only with the Balanced plan.
enum class PowerMode { Efficiency, Balanced, Performance };
QString powerModeName(PowerMode mode);

struct PowerPlan
{
    QString id;      // the plan's GUID, "{8c5e7fda-...}"
    QString name;    // "High performance"
    bool    active{false};
};

struct Power
{
    bool    known{false};
    bool    onBattery{false};
    int     batteryPercent{-1};   // -1: no battery / unknown
    bool    batterySaver{false};
    QString plan;                 // "High performance"
    QString mode;                 // Windows 11 power mode: "Best performance"; empty: not known
    bool    efficiencyMode{false};   // the power mode favours battery life
    std::optional<PowerMode> powerMode;   // the mode above, when it is one of the three
};

// What the backend reads once per sample. Times are in one clock (ns), cpu times in 100 ns.
struct RawSample
{
    bool    processAlive{false};
    qint64  timeNs{0};
    qint64  processCpu100ns{0};       // kernel + user time of the game
    qint64  systemBusy100ns{0};       // all cores, since boot
    qint64  systemTotal100ns{0};
    int     cpus{1};
    quint64 workingSetBytes{0};
    std::vector<CounterValue> gpuEngines;   // empty: counters not available
    std::vector<CounterValue> gpuMemory;
    std::vector<qint64> presentsNs;   // the game's presents since the previous sample
    FpsSource fpsSource{FpsSource::Unsupported};
    QString   fpsError;
    Power     power;
};

struct Warning
{
    QString text;     // short, for the bar
    QString detail;   // the reason and what to do, for the tooltip and the log
    bool operator==(const Warning&) const = default;
};

// What the bar shows.
struct GameStats
{
    bool running{false};
    std::optional<double> cpu;         // percent of the whole CPU
    std::optional<double> systemCpu;
    std::optional<double> gpu;
    std::optional<double> magpieGpu;   // Magpie's own GPU use while it runs (its FSR work is not in `gpu`)
    QString gpuName;                   // the adapter the game renders on
    std::optional<quint64> ram;        // working set, bytes
    std::optional<quint64> vram;       // dedicated video memory, bytes
    FrameRate fps;
    FpsSource fpsSource{FpsSource::Unsupported};
    QString   fpsError;
    Power     power;
    std::vector<Warning> warnings;
};

// Warnings for a slow game: on battery, battery saver, power-saving mode, rendering on an
// integrated GPU while a dedicated one is there (only when the game uses the GPU, at least 10 %),
// the whole CPU busy. Only while the game runs.
std::vector<Warning> warningsFor(const GameStats& stats, const std::vector<Adapter>& adapters);

// The adapter with the most dedicated memory, if it clearly is a dedicated GPU (>= 1 GB).
const Adapter* dedicatedAdapter(const std::vector<Adapter>& adapters);
const Adapter* adapterByLuid(const std::vector<Adapter>& adapters, quint64 luid);

// For the bar: "NVIDIA GeForce GTX 1650" -> "GeForce GTX 1650", "Intel(R) UHD Graphics" -> "UHD Graphics".
QString shortAdapterName(const QString& name);

// "6.1 GB", "820 MB"
QString formatBytes(quint64 bytes);

// Windows' graphics setting per program (Settings > System > Display > Graphics). Stored per exe as
// "GpuPreference=2;" in HKCU/Software/Microsoft/DirectX/UserGpuPreferences, next to other entries
// ("AppStatus=4096;", "SwapEffectUpgradeEnable=1;") that must be kept. Same numbers as DXGI_GPU_PREFERENCE.
enum class GpuPreference { Auto = 0, PowerSaving = 1, HighPerformance = 2 };

GpuPreference gpuPreferenceFrom(const QString& value);   // Auto when absent or unknown
QString withGpuPreference(const QString& value, GpuPreference preference);
QString gpuPreferenceName(GpuPreference preference);       // as Windows Settings calls it

// For the floating bar: "58 FPS · CPU 23 % · GPU 97 %", or "Game not running".
QString compactLine(const GameStats& stats);

// One line for the game log: "FPS 58 (1% low 41), CPU 23 %, GPU 97 % on ..., RAM ..., AC power".
QString summaryLine(const GameStats& stats);

/**
 * Turns RawSamples into GameStats. Percentages need two samples in a row of the same process:
 * the first sample after a (new) game only gives memory and power. Present timestamps are kept
 * for frameRate's window.
 */
class Sampler
{
public:
    static constexpr qint64 kFpsWindowNs = 1'000'000'000;

    // magpiePid: a running Magpie.exe (0: none), whose GPU use is read from the same counters.
    GameStats update(qint64 pid, const RawSample& raw, const std::vector<Adapter>& adapters, qint64 magpiePid = 0);
    void reset();

private:
    qint64 m_pid{0};
    std::optional<RawSample> m_previous;
    std::vector<qint64> m_presents;
};

} // namespace Stats
