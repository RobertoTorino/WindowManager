#include "../../stats/StatsBackend.h"
#include "../../stats/DriverModel.h"

#include <Windows.h>
#include <combaseapi.h>
#include <dxgi1_6.h>
#include <evntcons.h>
#include <evntrace.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <powrprof.h>
#include <psapi.h>
#include <wrl/client.h>

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <optional>
#include <thread>

// CPU and memory from the process handle, GPU use and video memory from the "GPU Engine" and
// "GPU Process Memory" performance counters (what Task Manager shows), frames from the DXGI and
// Direct3D 9 present events of a real-time event trace (like PresentMon), and the power state.
// The per-program graphics setting is the registry value Windows Settings writes.

namespace {

using Microsoft::WRL::ComPtr;

// Microsoft-Windows-DXGI: Present_Start, PresentMultiplaneOverlay_Start
constexpr GUID kDxgiProvider = {0xCA11C036, 0x0102, 0x4A2D, {0xA6, 0xAD, 0xF0, 0x3C, 0xFE, 0xD5, 0xD3, 0xC9}};
constexpr USHORT kDxgiPresent = 42, kDxgiPresentMpo = 55;
// Microsoft-Windows-D3D9: Present_Start
constexpr GUID kD3d9Provider = {0x783ACA0A, 0x790E, 0x4D7F, {0x84, 0x51, 0xAA, 0x85, 0x05, 0x11, 0xC6, 0xB9}};
constexpr USHORT kD3d9Present = 1;

// Windows 10 / 11 power modes (the slider of the Balanced plan)
constexpr GUID kBalancedPlan     = {0x381B4222, 0xF694, 0x41F0, {0x96, 0x85, 0xFF, 0x5B, 0xB2, 0x60, 0xDF, 0x2E}};
constexpr GUID kModeEfficiency   = {0x961CC777, 0x2547, 0x4F9D, {0x81, 0x74, 0x7D, 0x86, 0x18, 0x1B, 0x8A, 0x7A}};
constexpr GUID kModeBalanced     = {0x00000000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
constexpr GUID kModePerformance  = {0x3AF9B8D9, 0x7C97, 0x431D, {0xAD, 0x78, 0x34, 0xA8, 0xBF, 0xEA, 0x43, 0x9F}};
constexpr GUID kModeBest         = {0xDED574B5, 0x45A0, 0x4F42, {0x87, 0x37, 0x46, 0x34, 0x5C, 0x09, 0xC2, 0x38}};

constexpr wchar_t kGpuPreferencesKey[] = L"Software\\Microsoft\\DirectX\\UserGpuPreferences";
constexpr wchar_t kSessionName[] = L"WindowManager-Frames";
constexpr size_t  kMaxPending    = 20000;   // presents kept between two samples

qint64 qpcFrequency()
{
    static const qint64 frequency = []() {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f.QuadPart;
    }();
    return frequency;
}

qint64 qpcToNs(qint64 ticks)
{
    const qint64 f = qpcFrequency();
    return (ticks / f) * 1'000'000'000 + (ticks % f) * 1'000'000'000 / f;
}

qint64 nowNs()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return qpcToNs(t.QuadPart);
}

qint64 fileTime100ns(const FILETIME& t)
{
    return qint64((quint64(t.dwHighDateTime) << 32) | t.dwLowDateTime);
}

// EVENT_TRACE_PROPERTIES followed by room for the session name.
struct TraceProperties
{
    EVENT_TRACE_PROPERTIES props;
    wchar_t name[64];

    TraceProperties()
    {
        ZeroMemory(this, sizeof(*this));
        props.Wnode.BufferSize    = sizeof(*this);
        props.Wnode.Flags         = WNODE_FLAG_TRACED_GUID;
        props.Wnode.ClientContext = 1;   // timestamps from QueryPerformanceCounter
        props.LogFileMode         = EVENT_TRACE_REAL_TIME_MODE;
        props.LoggerNameOffset    = offsetof(TraceProperties, name);
        props.FlushTimer          = 1;   // seconds
        props.BufferSize          = 64;  // KB
        props.MinimumBuffers      = 4;
    }
};

// A real-time trace session that counts the presents of one process.
class PresentTracer
{
public:
    ~PresentTracer() { stop(); }

    // Starts once; later calls only report the state.
    Stats::FpsSource ensureStarted(QString* error)
    {
        std::lock_guard lock(m_control);
        if (!m_tried) {
            m_tried = true;
            m_state = start(&m_error);
        }
        if (error)
            *error = m_error;
        return m_state;
    }

    void setPid(qint64 pid)
    {
        if (m_pid.exchange(pid) != pid) {
            std::lock_guard lock(m_mutex);
            m_pending.clear();
        }
    }

    std::vector<qint64> take()
    {
        std::lock_guard lock(m_mutex);
        std::vector<qint64> out;
        out.swap(m_pending);
        return out;
    }

    void stop()
    {
        std::lock_guard lock(m_control);
        if (m_session) {
            TraceProperties props;
            ControlTraceW(m_session, nullptr, &props.props, EVENT_TRACE_CONTROL_STOP);
            m_session = 0;
        }
        if (m_thread.joinable())
            m_thread.join();   // ProcessTrace returns once the session has stopped
        if (m_consumer != INVALID_PROCESSTRACE_HANDLE) {
            CloseTrace(m_consumer);
            m_consumer = INVALID_PROCESSTRACE_HANDLE;
        }
        if (m_state == Stats::FpsSource::Running)
            m_state = Stats::FpsSource::Failed;
    }

private:
    Stats::FpsSource start(QString* error)
    {
        TraceProperties props;
        ULONG rc = StartTraceW(&m_session, kSessionName, &props.props);
        if (rc == ERROR_ALREADY_EXISTS) {   // left behind by a crash
            TraceProperties old;
            ControlTraceW(0, kSessionName, &old.props, EVENT_TRACE_CONTROL_STOP);
            props = TraceProperties();
            rc = StartTraceW(&m_session, kSessionName, &props.props);
        }
        if (rc == ERROR_ACCESS_DENIED) {
            m_session = 0;
            *error = "Counting frames needs administrator rights, or membership of the Performance Log Users group.";
            return Stats::FpsSource::NoPermission;
        }
        if (rc != ERROR_SUCCESS) {
            m_session = 0;
            *error = QString("The frame trace could not be started (error %1).").arg(rc);
            return Stats::FpsSource::Failed;
        }

        const bool dxgi = enable(kDxgiProvider, {kDxgiPresent, kDxgiPresentMpo});
        const bool d3d9 = enable(kD3d9Provider, {kD3d9Present});
        if (!dxgi && !d3d9) {
            stopSession();
            *error = "The present events could not be enabled.";
            return Stats::FpsSource::Failed;
        }

        EVENT_TRACE_LOGFILEW log{};
        log.LoggerName          = const_cast<wchar_t*>(kSessionName);
        log.ProcessTraceMode    = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD
                                  | PROCESS_TRACE_MODE_RAW_TIMESTAMP;
        log.EventRecordCallback = &PresentTracer::onEvent;
        log.Context             = this;
        m_consumer = OpenTraceW(&log);
        if (m_consumer == INVALID_PROCESSTRACE_HANDLE) {
            stopSession();
            *error = QString("The frame trace could not be read (error %1).").arg(GetLastError());
            return Stats::FpsSource::Failed;
        }
        m_thread = std::thread([handle = m_consumer]() mutable { ProcessTrace(&handle, 1, nullptr, nullptr); });
        return Stats::FpsSource::Running;
    }

    // Only the present events: the providers send many more.
    bool enable(const GUID& provider, std::initializer_list<USHORT> ids)
    {
        std::vector<BYTE> filter(offsetof(EVENT_FILTER_EVENT_ID, Events) + ids.size() * sizeof(USHORT));
        auto* f = reinterpret_cast<EVENT_FILTER_EVENT_ID*>(filter.data());
        f->FilterIn = TRUE;
        f->Count    = USHORT(ids.size());
        USHORT* out = f->Events;
        for (USHORT id : ids)
            *out++ = id;

        EVENT_FILTER_DESCRIPTOR desc{};
        desc.Ptr  = reinterpret_cast<ULONGLONG>(filter.data());
        desc.Size = ULONG(filter.size());
        desc.Type = EVENT_FILTER_TYPE_EVENT_ID;

        ENABLE_TRACE_PARAMETERS params{};
        params.Version          = ENABLE_TRACE_PARAMETERS_VERSION_2;
        params.EnableFilterDesc = &desc;
        params.FilterDescCount  = 1;

        if (EnableTraceEx2(m_session, &provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE,
                           ~0ull, 0, 0, &params) == ERROR_SUCCESS)
            return true;
        // without the filter (older systems); onEvent checks the id anyway
        return EnableTraceEx2(m_session, &provider, EVENT_CONTROL_CODE_ENABLE_PROVIDER, TRACE_LEVEL_VERBOSE,
                              ~0ull, 0, 0, nullptr) == ERROR_SUCCESS;
    }

    void stopSession()
    {
        TraceProperties props;
        ControlTraceW(m_session, nullptr, &props.props, EVENT_TRACE_CONTROL_STOP);
        m_session = 0;
    }

    static void WINAPI onEvent(PEVENT_RECORD record)
    {
        auto* self = static_cast<PresentTracer*>(record->UserContext);
        const auto& header = record->EventHeader;
        if (qint64(header.ProcessId) != self->m_pid.load(std::memory_order_relaxed))
            return;
        const USHORT id = header.EventDescriptor.Id;
        const bool present = (IsEqualGUID(header.ProviderId, kDxgiProvider) && (id == kDxgiPresent || id == kDxgiPresentMpo))
                             || (IsEqualGUID(header.ProviderId, kD3d9Provider) && id == kD3d9Present);
        if (!present)
            return;
        std::lock_guard lock(self->m_mutex);
        if (self->m_pending.size() < kMaxPending)
            self->m_pending.push_back(qpcToNs(header.TimeStamp.QuadPart));
    }

    std::mutex       m_control;   // start / stop
    bool             m_tried{false};
    Stats::FpsSource m_state{Stats::FpsSource::Unsupported};
    QString          m_error;
    TRACEHANDLE      m_session{0};
    TRACEHANDLE      m_consumer{INVALID_PROCESSTRACE_HANDLE};
    std::thread      m_thread;

    std::atomic<qint64>  m_pid{0};
    std::mutex           m_mutex;     // m_pending
    std::vector<qint64>  m_pending;
};

// The two wildcard GPU counters.
class GpuCounters
{
public:
    ~GpuCounters()
    {
        if (m_query)
            PdhCloseQuery(m_query);
    }

    // Empty vectors when the counters are not there (older Windows, no WDDM 2 driver).
    void read(std::vector<Stats::CounterValue>& engines, std::vector<Stats::CounterValue>& memory)
    {
        if (!m_opened) {
            m_opened = true;
            if (PdhOpenQueryW(nullptr, 0, &m_query) != ERROR_SUCCESS) {
                m_query = nullptr;
                return;
            }
            PdhAddEnglishCounterW(m_query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_engines);
            PdhAddEnglishCounterW(m_query, L"\\GPU Process Memory(*)\\Dedicated Usage", 0, &m_memory);
        }
        if (!m_query || PdhCollectQueryData(m_query) != ERROR_SUCCESS)
            return;
        engines = values(m_engines);
        memory  = values(m_memory);
    }

private:
    static std::vector<Stats::CounterValue> values(PDH_HCOUNTER counter)
    {
        std::vector<Stats::CounterValue> out;
        if (!counter)
            return out;
        DWORD size = 0, count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr) != PDH_MORE_DATA)
            return out;   // the first collection of a rate counter has no value yet
        std::vector<BYTE> buffer(size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, items) != ERROR_SUCCESS)
            return out;
        out.reserve(count);
        for (DWORD i = 0; i < count; ++i) {
            const auto status = items[i].FmtValue.CStatus;
            if (status == PDH_CSTATUS_VALID_DATA || status == PDH_CSTATUS_NEW_DATA)
                out.push_back({QString::fromWCharArray(items[i].szName), items[i].FmtValue.doubleValue});
        }
        return out;
    }

    bool         m_opened{false};
    PDH_HQUERY   m_query{nullptr};
    PDH_HCOUNTER m_engines{nullptr};
    PDH_HCOUNTER m_memory{nullptr};
};

QString powerModeName(const GUID& mode)
{
    if (IsEqualGUID(mode, kModeEfficiency))  return "Best power efficiency";
    if (IsEqualGUID(mode, kModeBalanced))    return "Balanced";
    if (IsEqualGUID(mode, kModePerformance)) return "Better performance";
    if (IsEqualGUID(mode, kModeBest))        return "Best performance";
    return {};
}

std::optional<Stats::PowerMode> powerModeOf(const GUID& mode)
{
    if (IsEqualGUID(mode, kModeEfficiency)) return Stats::PowerMode::Efficiency;
    if (IsEqualGUID(mode, kModeBalanced))   return Stats::PowerMode::Balanced;
    if (IsEqualGUID(mode, kModeBest))       return Stats::PowerMode::Performance;
    return std::nullopt;   // Windows 10's "Better performance" step
}

QString guidText(const GUID& guid)
{
    wchar_t text[64] = {};
    StringFromGUID2(guid, text, 64);
    return QString::fromWCharArray(text);
}

QString planName(const GUID& scheme)
{
    wchar_t name[128] = {};
    DWORD size = sizeof(name);
    if (PowerReadFriendlyName(nullptr, &scheme, nullptr, nullptr, reinterpret_cast<UCHAR*>(name), &size)
        != ERROR_SUCCESS)
        return {};
    return QString::fromWCharArray(name);
}

// powrprof exports these since Windows 10 1709, but not every SDK declares them.
template <typename Fn>
Fn powrprof(const char* name)
{
    static HMODULE module = LoadLibraryW(L"powrprof.dll");
    return module ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(module, name))) : nullptr;
}

// The installed driver of each display adapter: the display class key, one numbered subkey per driver.
void readDrivers(std::vector<Stats::Adapter>& adapters)
{
    constexpr wchar_t kDisplayClass[] =
        L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4d36e968-e325-11ce-bfc1-08002be10318}";
    HKEY classKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, kDisplayClass, 0, KEY_READ, &classKey) != ERROR_SUCCESS)
        return;
    const auto text = [](HKEY key, const wchar_t* value) {
        wchar_t data[512] = {};
        DWORD bytes = sizeof(data) - sizeof(wchar_t);
        if (RegGetValueW(key, nullptr, value, RRF_RT_REG_SZ, nullptr, data, &bytes) != ERROR_SUCCESS)
            return QString();
        return QString::fromWCharArray(data);
    };
    wchar_t subkey[16];
    for (DWORD i = 0;; ++i) {
        DWORD length = 16;
        if (RegEnumKeyExW(classKey, i, subkey, &length, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
            break;
        HKEY driver = nullptr;
        if (RegOpenKeyExW(classKey, subkey, 0, KEY_READ, &driver) != ERROR_SUCCESS)
            continue;   // "Properties" is not readable without admin rights
        const QString matching = text(driver, L"MatchingDeviceId");
        for (auto& a : adapters) {
            if (Driver::matchesDevice(matching, a.vendorId, a.deviceId)) {
                a.driverVersion = text(driver, L"DriverVersion");
                a.driverDate    = Driver::parseDriverDate(text(driver, L"DriverDate"));
            }
        }
        RegCloseKey(driver);
    }
    RegCloseKey(classKey);
}

Stats::Power readPower()
{
    Stats::Power power;
    SYSTEM_POWER_STATUS status;
    if (GetSystemPowerStatus(&status)) {
        power.known        = status.ACLineStatus != 255;
        power.onBattery    = status.ACLineStatus == 0;
        power.batterySaver = status.SystemStatusFlag == 1;
        if (status.BatteryFlag != 128 && status.BatteryFlag != 255 && status.BatteryLifePercent <= 100)
            power.batteryPercent = status.BatteryLifePercent;
    }

    GUID* scheme = nullptr;
    if (PowerGetActiveScheme(nullptr, &scheme) == ERROR_SUCCESS && scheme) {
        power.plan = planName(*scheme);
        // The power mode slider only exists for the Balanced plan.
        if (IsEqualGUID(*scheme, kBalancedPlan)) {
            static const auto overlay = powrprof<DWORD(WINAPI*)(GUID*)>("PowerGetEffectiveOverlayScheme");
            GUID mode{};
            if (overlay && overlay(&mode) == ERROR_SUCCESS) {
                power.mode           = powerModeName(mode);
                power.efficiencyMode = IsEqualGUID(mode, kModeEfficiency);
                power.powerMode      = powerModeOf(mode);
            }
        }
        LocalFree(scheme);
    }
    return power;
}

class Win32Stats final : public StatsBackend
{
public:
    ~Win32Stats() override
    {
        shutdown();
        closeProcess();
    }

    bool supported() const override { return true; }

    std::vector<Stats::Adapter> adapters() override
    {
        std::lock_guard lock(m_mutex);
        if (!m_adaptersRead) {
            m_adaptersRead = true;
            ComPtr<IDXGIFactory1> factory;
            if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
                ComPtr<IDXGIAdapter1> adapter;
                for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                    DXGI_ADAPTER_DESC1 desc;
                    if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                        Stats::Adapter a;
                        a.luid = (quint64(quint32(desc.AdapterLuid.HighPart)) << 32) | desc.AdapterLuid.LowPart;
                        a.name = QString::fromWCharArray(desc.Description).trimmed();
                        a.dedicatedBytes = desc.DedicatedVideoMemory;
                        a.vendorId = desc.VendorId;
                        a.deviceId = desc.DeviceId;
                        m_adapters.push_back(a);
                    }
                    adapter.Reset();
                }
            }
        }
        readDrivers(m_adapters);   // every time: a driver can be updated while the app runs
        return m_adapters;
    }

    Stats::RawSample sample(qint64 pid) override
    {
        std::lock_guard lock(m_mutex);
        Stats::RawSample raw;
        raw.power = readPower();
        raw.fpsSource = m_tracer.ensureStarted(&raw.fpsError);
        m_tracer.setPid(pid);
        if (pid == 0)
            return raw;

        if (pid != m_pid) {
            closeProcess();
            m_pid = pid;
            m_process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, DWORD(pid));
            if (!m_process)
                m_process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
        }
        DWORD exitCode = 0;
        if (!m_process || !GetExitCodeProcess(m_process, &exitCode) || exitCode != STILL_ACTIVE)
            return raw;
        raw.processAlive = true;
        raw.timeNs = nowNs();

        FILETIME created, exited, kernel, user;
        if (GetProcessTimes(m_process, &created, &exited, &kernel, &user))
            raw.processCpu100ns = fileTime100ns(kernel) + fileTime100ns(user);
        FILETIME idle, sysKernel, sysUser;
        if (GetSystemTimes(&idle, &sysKernel, &sysUser)) {   // kernel time includes idle time
            raw.systemTotal100ns = fileTime100ns(sysKernel) + fileTime100ns(sysUser);
            raw.systemBusy100ns  = raw.systemTotal100ns - fileTime100ns(idle);
        }
        raw.cpus = std::max<int>(1, int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)));

        PROCESS_MEMORY_COUNTERS memory{};
        if (GetProcessMemoryInfo(m_process, &memory, sizeof(memory)))
            raw.workingSetBytes = memory.WorkingSetSize;

        m_gpu.read(raw.gpuEngines, raw.gpuMemory);
        raw.presentsNs = m_tracer.take();
        return raw;
    }

    void shutdown() override { m_tracer.stop(); }

    bool gpuPreferenceSupported() const override { return true; }

    Stats::GpuPreference gpuPreference(const QString& exePath) override
    {
        return Stats::gpuPreferenceFrom(readPreferenceValue(exePath));
    }

    bool setGpuPreference(const QString& exePath, Stats::GpuPreference preference, QString* error) override
    {
        const QString value = Stats::withGpuPreference(readPreferenceValue(exePath), preference);
        const std::wstring name = exePath.toStdWString();
        const std::wstring data = value.toStdWString();
        const LSTATUS rc = RegSetKeyValueW(HKEY_CURRENT_USER, kGpuPreferencesKey, name.c_str(), REG_SZ, data.c_str(),
                                           DWORD((data.size() + 1) * sizeof(wchar_t)));
        if (rc != ERROR_SUCCESS && error)
            *error = QString("Windows did not store the graphics setting (error %1).").arg(rc);
        return rc == ERROR_SUCCESS;
    }

    Stats::Power power() override { return readPower(); }

    std::vector<Stats::PowerPlan> powerPlans() override
    {
        std::vector<Stats::PowerPlan> plans;
        GUID* active = nullptr;
        PowerGetActiveScheme(nullptr, &active);
        for (ULONG i = 0;; ++i) {
            GUID scheme;
            DWORD size = sizeof(scheme);
            if (PowerEnumerate(nullptr, nullptr, nullptr, ACCESS_SCHEME, i, reinterpret_cast<UCHAR*>(&scheme), &size)
                != ERROR_SUCCESS)
                break;
            plans.push_back({guidText(scheme), planName(scheme), active && IsEqualGUID(scheme, *active)});
        }
        if (active)
            LocalFree(active);
        return plans;
    }

    bool setPowerPlan(const QString& id, QString* error) override
    {
        GUID scheme;
        if (FAILED(CLSIDFromString(reinterpret_cast<LPCOLESTR>(id.utf16()), &scheme))) {
            if (error)
                *error = "Unknown power plan.";
            return false;
        }
        const DWORD rc = PowerSetActiveScheme(nullptr, &scheme);
        if (rc != ERROR_SUCCESS && error)
            *error = QString("Windows did not switch the power plan (error %1).").arg(rc);
        return rc == ERROR_SUCCESS;
    }

    bool setPowerMode(Stats::PowerMode mode, QString* error) override
    {
        static const auto set = powrprof<DWORD(WINAPI*)(GUID)>("PowerSetActiveOverlayScheme");
        if (!set) {
            if (error)
                *error = "This Windows version has no power mode.";
            return false;
        }
        const GUID guid = mode == Stats::PowerMode::Efficiency    ? kModeEfficiency
                          : mode == Stats::PowerMode::Performance ? kModeBest
                                                                  : kModeBalanced;
        const DWORD rc = set(guid);
        if (rc != ERROR_SUCCESS && error)
            *error = QString("Windows did not switch the power mode (error %1).").arg(rc);
        return rc == ERROR_SUCCESS;
    }

    QString adapterFor(Stats::GpuPreference preference) override
    {
        ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            return {};   // before Windows 10 1803
        const auto dxgi = preference == Stats::GpuPreference::PowerSaving      ? DXGI_GPU_PREFERENCE_MINIMUM_POWER
                          : preference == Stats::GpuPreference::HighPerformance ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                                                                                : DXGI_GPU_PREFERENCE_UNSPECIFIED;
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1 desc;
        if (FAILED(factory->EnumAdapterByGpuPreference(0, dxgi, IID_PPV_ARGS(&adapter)))
            || FAILED(adapter->GetDesc1(&desc)))
            return {};
        return QString::fromWCharArray(desc.Description).trimmed();
    }

private:
    static QString readPreferenceValue(const QString& exePath)
    {
        const std::wstring name = exePath.toStdWString();
        DWORD bytes = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, kGpuPreferencesKey, name.c_str(), RRF_RT_REG_SZ, nullptr, nullptr, &bytes)
            != ERROR_SUCCESS)
            return {};
        std::wstring data(bytes / sizeof(wchar_t) + 1, L'\0');
        if (RegGetValueW(HKEY_CURRENT_USER, kGpuPreferencesKey, name.c_str(), RRF_RT_REG_SZ, nullptr, data.data(), &bytes)
            != ERROR_SUCCESS)
            return {};
        return QString::fromWCharArray(data.c_str());
    }

    void closeProcess()
    {
        if (m_process)
            CloseHandle(m_process);
        m_process = nullptr;
        m_pid = 0;
    }

    std::mutex    m_mutex;
    PresentTracer m_tracer;
    GpuCounters   m_gpu;
    qint64        m_pid{0};
    HANDLE        m_process{nullptr};
    bool          m_adaptersRead{false};
    std::vector<Stats::Adapter> m_adapters;
};

} // namespace

StatsBackend& statsBackend()
{
    static Win32Stats backend;
    return backend;
}
