#include "../../display/DisplayBackend.h"

#include <Windows.h>
#include <Wbemidl.h>
#include <highlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <optional>
#include <string>

// Display settings through the documented DisplayConfig / ChangeDisplaySettingsEx / SetDisplayConfig
// APIs, WMI (built-in panel brightness), DDC/CI (external monitor brightness) and mscms (colour
// profiles). The scale uses the undocumented DisplayConfig calls that Settings itself uses.

namespace {

using Microsoft::WRL::ComPtr;

// Not declared by the SDK for this build's NTDDI_VERSION; the values are fixed in wingdi.h.
constexpr auto kGetAdvancedColorInfo   = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(9);
constexpr auto kSetAdvancedColorState  = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(10);
constexpr auto kGetAdvancedColorInfo2  = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(15);   // Windows 11 24H2
constexpr auto kSetHdrState            = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(16);   // Windows 11 24H2
// Undocumented: scale steps relative to the recommended one (see DisplayModel::scaleFromRelative).
constexpr auto kGetDpiScale            = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(-3);
constexpr auto kSetDpiScale            = static_cast<DISPLAYCONFIG_DEVICE_INFO_TYPE>(-4);

struct ColorInfo   // DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value;
    UINT32 colorEncoding;
    UINT32 bitsPerColorChannel;
};

struct ColorInfo2  // DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value;
    UINT32 colorEncoding;
    UINT32 bitsPerColorChannel;
    UINT32 activeColorMode;
};

struct ColorState  // DISPLAYCONFIG_SET_HDR_STATE / DISPLAYCONFIG_SET_ADVANCED_COLOR_STATE
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    UINT32 value;
};

struct DpiScaleGet
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    INT32 minRelative;
    INT32 curRelative;
    INT32 maxRelative;
};

struct DpiScaleSet
{
    DISPLAYCONFIG_DEVICE_INFO_HEADER header;
    INT32 relative;
};

// mscms.dll colour-profile calls (Windows 10 1809+), looked up at run time.
constexpr int kScopeCurrentUser = 1;   // WCS_PROFILE_MANAGEMENT_SCOPE_CURRENT_USER
constexpr int kProfileIcc = 0;         // CPT_ICC
constexpr int kStandardColorMode = 7;  // CPST_STANDARD_DISPLAY_COLOR_MODE
constexpr int kExtendedColorMode = 8;  // CPST_EXTENDED_DISPLAY_COLOR_MODE

struct Mscms
{
    using GetScope   = HRESULT(WINAPI*)(LUID, UINT32, int*);
    using GetList    = HRESULT(WINAPI*)(int, LUID, UINT32, LPWSTR**, PDWORD);
    using GetDefault = HRESULT(WINAPI*)(int, LUID, UINT32, int, int, LPWSTR*);
    using SetDefault = HRESULT(WINAPI*)(int, PCWSTR, int, int, LUID, UINT32);

    GetScope   getScope{nullptr};
    GetList    getList{nullptr};
    GetDefault getDefault{nullptr};
    SetDefault setDefault{nullptr};

    static const Mscms& get()
    {
        static const Mscms m;
        return m;
    }
    bool available() const { return getScope && getList && getDefault && setDefault; }

private:
    Mscms()
    {
        HMODULE dll = LoadLibraryExW(L"mscms.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!dll)
            return;
        getScope   = reinterpret_cast<GetScope>(GetProcAddress(dll, "ColorProfileGetDisplayUserScope"));
        getList    = reinterpret_cast<GetList>(GetProcAddress(dll, "ColorProfileGetDisplayList"));
        getDefault = reinterpret_cast<GetDefault>(GetProcAddress(dll, "ColorProfileGetDisplayDefault"));
        setDefault = reinterpret_cast<SetDefault>(GetProcAddress(dll, "ColorProfileSetDisplayDefaultAssociation"));
    }
};

// One active source -> monitor connection.
struct Path
{
    LUID    sourceAdapter{};
    UINT32  sourceId{0};
    LUID    targetAdapter{};
    UINT32  targetId{0};
    QString deviceName;   // \\.\DISPLAY1
    QString id;           // monitor device path
    QString name;
    bool    internal{false};
};

bool fail(QString* error, const QString& text)
{
    if (error)
        *error = text;
    return false;
}

QString systemMessage(DWORD code)
{
    wchar_t* text = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<LPWSTR>(&text), 0, nullptr);
    QString s = text ? QString::fromWCharArray(text).trimmed() : QString("error %1").arg(code);
    LocalFree(text);
    return s;
}

QString changeDisplayMessage(LONG code)
{
    switch (code) {
    case DISP_CHANGE_BADMODE:     return "the display does not support this mode";
    case DISP_CHANGE_FAILED:      return "the display driver could not set this mode";
    case DISP_CHANGE_RESTART:     return "Windows must be restarted for this mode";
    case DISP_CHANGE_NOTUPDATED:  return "Windows could not store the setting";
    case DISP_CHANGE_BADPARAM:
    case DISP_CHANGE_BADFLAGS:    return "Windows refused the request";
    case DISP_CHANGE_BADDUALVIEW: return "not possible while the display is in DualView";
    default:                      return QString("error %1").arg(code);
    }
}

// QueryDisplayConfig, retried while the configuration changes between the two calls.
bool queryConfig(UINT32 flags, std::vector<DISPLAYCONFIG_PATH_INFO>& paths,
                 DISPLAYCONFIG_TOPOLOGY_ID* topology = nullptr)
{
    std::vector<DISPLAYCONFIG_MODE_INFO> modes;
    LONG r = ERROR_INSUFFICIENT_BUFFER;
    for (int attempt = 0; attempt < 5 && r == ERROR_INSUFFICIENT_BUFFER; ++attempt) {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        if (GetDisplayConfigBufferSizes(flags, &pathCount, &modeCount) != ERROR_SUCCESS)
            return false;
        paths.resize(pathCount);
        modes.resize(modeCount);
        r = QueryDisplayConfig(flags, &pathCount, paths.data(), &modeCount, modes.data(), topology);
        paths.resize(pathCount);
    }
    return r == ERROR_SUCCESS;
}

std::vector<Path> activePaths()
{
    std::vector<DISPLAYCONFIG_PATH_INFO> paths;
    if (!queryConfig(QDC_ONLY_ACTIVE_PATHS, paths))
        return {};

    std::vector<Path> out;
    for (const auto& p : paths) {
        Path path;
        path.sourceAdapter = p.sourceInfo.adapterId;
        path.sourceId      = p.sourceInfo.id;
        path.targetAdapter = p.targetInfo.adapterId;
        path.targetId      = p.targetInfo.id;
        const auto tech = p.targetInfo.outputTechnology;
        path.internal = tech == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_INTERNAL
                     || tech == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_DISPLAYPORT_EMBEDDED
                     || tech == DISPLAYCONFIG_OUTPUT_TECHNOLOGY_UDI_EMBEDDED;

        DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
        source.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        source.header.size      = sizeof(source);
        source.header.adapterId = path.sourceAdapter;
        source.header.id        = path.sourceId;
        if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
            continue;
        path.deviceName = QString::fromWCharArray(source.viewGdiDeviceName);

        DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
        target.header.type      = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
        target.header.size      = sizeof(target);
        target.header.adapterId = path.targetAdapter;
        target.header.id        = path.targetId;
        if (DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS) {
            path.name = QString::fromWCharArray(target.monitorFriendlyDeviceName);
            path.id   = QString::fromWCharArray(target.monitorDevicePath);
        }
        if (path.id.isEmpty())
            path.id = path.deviceName;
        if (path.name.isEmpty())
            path.name = path.internal ? "Built-in display" : "Display";
        out.push_back(path);
    }
    return out;
}

std::optional<Path> findPath(const QString& id)
{
    for (const auto& p : activePaths())
        if (p.id.compare(id, Qt::CaseInsensitive) == 0)
            return p;
    return std::nullopt;
}

// ----- Modes -------------------------------------------------------------------

void readModes(const Path& p, DisplayModel::Monitor& m)
{
    const std::wstring device = p.deviceName.toStdWString();
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsExW(device.c_str(), ENUM_CURRENT_SETTINGS, &dm, 0)) {
        m.mode = {static_cast<int>(dm.dmPelsWidth), static_cast<int>(dm.dmPelsHeight),
                  dm.dmDisplayFrequency > 1 ? static_cast<int>(dm.dmDisplayFrequency) : 0};
        m.orientation = static_cast<DisplayModel::Orientation>(dm.dmDisplayOrientation & 3);
        m.primary = dm.dmPosition.x == 0 && dm.dmPosition.y == 0;
    }

    for (DWORD i = 0;; ++i) {
        DEVMODEW d{};
        d.dmSize = sizeof(d);
        if (!EnumDisplaySettingsExW(device.c_str(), i, &d, 0))
            break;
        if ((d.dmBitsPerPel != 0 && d.dmBitsPerPel < 32) || (d.dmDisplayFlags & DM_INTERLACED))
            continue;
        DisplayModel::Size size{static_cast<int>(d.dmPelsWidth), static_cast<int>(d.dmPelsHeight)};
        if ((d.dmFields & DM_DISPLAYORIENTATION)
            && DisplayModel::isPortrait(static_cast<DisplayModel::Orientation>(d.dmDisplayOrientation & 3)))
            size = {size.height, size.width};   // keep the list in landscape
        const DisplayModel::Mode mode{size.width, size.height,
                                 d.dmDisplayFrequency > 1 ? static_cast<int>(d.dmDisplayFrequency) : 0};
        if (std::none_of(m.modes.begin(), m.modes.end(), [&](const DisplayModel::Mode& x) { return x == mode; }))
            m.modes.push_back(mode);
    }
}

// ----- HDR ---------------------------------------------------------------------

void readHdr(const Path& p, bool& supported, bool& on)
{
    supported = on = false;
    ColorInfo2 info2{};
    info2.header.type      = kGetAdvancedColorInfo2;
    info2.header.size      = sizeof(info2);
    info2.header.adapterId = p.targetAdapter;
    info2.header.id        = p.targetId;
    if (DisplayConfigGetDeviceInfo(&info2.header) == ERROR_SUCCESS) {
        supported = info2.value & (1u << 4);   // highDynamicRangeSupported
        on        = info2.value & (1u << 5);   // highDynamicRangeUserEnabled
        return;
    }
    ColorInfo info{};   // before 24H2, "advanced color" is HDR
    info.header.type      = kGetAdvancedColorInfo;
    info.header.size      = sizeof(info);
    info.header.adapterId = p.targetAdapter;
    info.header.id        = p.targetId;
    if (DisplayConfigGetDeviceInfo(&info.header) == ERROR_SUCCESS) {
        supported = (info.value & 1u) && !(info.value & (1u << 3));   // supported, not force-disabled
        on        = supported && (info.value & (1u << 1));
    }
}

// ----- Scale -------------------------------------------------------------------

DisplayModel::ScaleInfo readScale(const Path& p)
{
    DpiScaleGet get{};
    get.header.type      = kGetDpiScale;
    get.header.size      = sizeof(get);
    get.header.adapterId = p.sourceAdapter;
    get.header.id        = p.sourceId;
    if (DisplayConfigGetDeviceInfo(&get.header) != ERROR_SUCCESS)
        return {};
    return DisplayModel::scaleFromRelative(get.minRelative, get.curRelative, get.maxRelative);
}

// ----- Brightness: WMI (built-in panel) -----------------------------------------

class ComScope
{
public:
    ComScope() : m_hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)) {}
    ~ComScope()
    {
        if (SUCCEEDED(m_hr))
            CoUninitialize();
    }

private:
    HRESULT m_hr;   // RPC_E_CHANGED_MODE: COM is already set up differently, still usable
};

class Bstr
{
public:
    explicit Bstr(const wchar_t* text) : m_s(SysAllocString(text)) {}
    ~Bstr() { SysFreeString(m_s); }
    operator BSTR() const { return m_s; }

private:
    BSTR m_s;
};

ComPtr<IWbemServices> wmi()
{
    ComPtr<IWbemLocator> locator;
    if (FAILED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&locator))))
        return {};
    ComPtr<IWbemServices> services;
    if (FAILED(locator->ConnectServer(Bstr(L"ROOT\\WMI"), nullptr, nullptr, nullptr, 0, nullptr, nullptr,
                                      &services)))
        return {};
    CoSetProxyBlanket(services.Get(), RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL,
                      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
    return services;
}

QString wmiString(IWbemClassObject* object, const wchar_t* name)
{
    VARIANT v;
    VariantInit(&v);
    QString s;
    if (SUCCEEDED(object->Get(name, 0, &v, nullptr, nullptr)) && v.vt == VT_BSTR)
        s = QString::fromWCharArray(v.bstrVal);
    VariantClear(&v);
    return s;
}

int wmiInt(IWbemClassObject* object, const wchar_t* name)
{
    VARIANT v;
    VariantInit(&v);
    int value = -1;
    if (SUCCEEDED(object->Get(name, 0, &v, nullptr, nullptr)) && SUCCEEDED(VariantChangeType(&v, &v, 0, VT_I4)))
        value = v.lVal;
    VariantClear(&v);
    return value;
}

// Runs a WQL query and returns the object for this monitor (or the only one, for a built-in panel).
ComPtr<IWbemClassObject> wmiObjectFor(IWbemServices* services, const wchar_t* query, const Path& p)
{
    ComPtr<IEnumWbemClassObject> results;
    if (FAILED(services->ExecQuery(Bstr(L"WQL"), Bstr(query),
                                   WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &results)))
        return {};
    std::vector<ComPtr<IWbemClassObject>> all;
    for (;;) {
        ComPtr<IWbemClassObject> object;
        ULONG count = 0;
        if (results->Next(WBEM_INFINITE, 1, &object, &count) != WBEM_S_NO_ERROR || count == 0)
            break;
        if (DisplayModel::wmiInstanceMatches(p.id, wmiString(object.Get(), L"InstanceName")))
            return object;
        all.push_back(object);
    }
    if (all.size() == 1 && p.internal)
        return all.front();
    return {};
}

std::optional<int> wmiBrightness(const Path& p)
{
    ComScope com;
    const auto services = wmi();
    if (!services)
        return std::nullopt;
    const auto object = wmiObjectFor(
        services.Get(), L"SELECT InstanceName, CurrentBrightness FROM WmiMonitorBrightness WHERE Active = TRUE", p);
    if (!object)
        return std::nullopt;
    const int value = wmiInt(object.Get(), L"CurrentBrightness");
    return value >= 0 ? std::optional<int>(value) : std::nullopt;
}

bool setWmiBrightness(const Path& p, int percent)
{
    ComScope com;
    const auto services = wmi();
    if (!services)
        return false;
    const auto object = wmiObjectFor(
        services.Get(), L"SELECT * FROM WmiMonitorBrightnessMethods WHERE Active = TRUE", p);
    if (!object)
        return false;
    const std::wstring path = wmiString(object.Get(), L"__PATH").toStdWString();

    ComPtr<IWbemClassObject> methods;
    ComPtr<IWbemClassObject> definition;
    ComPtr<IWbemClassObject> in;
    if (FAILED(services->GetObject(Bstr(L"WmiMonitorBrightnessMethods"), 0, nullptr, &methods, nullptr))
        || FAILED(methods->GetMethod(L"WmiSetBrightness", 0, &definition, nullptr))
        || FAILED(definition->SpawnInstance(0, &in)))
        return false;

    VARIANT timeout;
    VariantInit(&timeout);
    timeout.vt   = VT_I4;
    timeout.lVal = 0;
    VARIANT level;
    VariantInit(&level);
    level.vt   = VT_UI1;
    level.bVal = static_cast<BYTE>(std::clamp(percent, 0, 100));
    if (FAILED(in->Put(L"Timeout", 0, &timeout, 0)) || FAILED(in->Put(L"Brightness", 0, &level, 0)))
        return false;
    return SUCCEEDED(services->ExecMethod(Bstr(path.c_str()), Bstr(L"WmiSetBrightness"), 0, nullptr,
                                          in.Get(), nullptr, nullptr));
}

// ----- Brightness: DDC/CI (external monitors) ------------------------------------

HMONITOR monitorHandle(const QString& deviceName)
{
    struct Search
    {
        std::wstring name;
        HMONITOR     found{nullptr};
    } search{deviceName.toStdWString()};

    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM data) -> BOOL {
        auto* s = reinterpret_cast<Search*>(data);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info) && s->name == info.szDevice) {
            s->found = monitor;
            return FALSE;
        }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.found;
}

template <typename F>
bool withPhysicalMonitor(const Path& p, F&& use)
{
    HMONITOR monitor = monitorHandle(p.deviceName);
    DWORD count = 0;
    if (!monitor || !GetNumberOfPhysicalMonitorsFromHMONITOR(monitor, &count) || count == 0)
        return false;
    std::vector<PHYSICAL_MONITOR> physical(count);
    if (!GetPhysicalMonitorsFromHMONITOR(monitor, count, physical.data()))
        return false;
    const bool ok = use(physical.front().hPhysicalMonitor);
    DestroyPhysicalMonitors(count, physical.data());
    return ok;
}

std::optional<int> ddcBrightness(const Path& p)
{
    int percent = -1;
    withPhysicalMonitor(p, [&](HANDLE h) {
        DWORD min = 0, cur = 0, max = 0;
        if (!GetMonitorBrightness(h, &min, &cur, &max))
            return false;
        percent = DisplayModel::brightnessPercent(static_cast<int>(cur), static_cast<int>(min), static_cast<int>(max));
        return true;
    });
    return percent >= 0 ? std::optional<int>(percent) : std::nullopt;
}

bool setDdcBrightness(const Path& p, int percent)
{
    return withPhysicalMonitor(p, [&](HANDLE h) {
        DWORD min = 0, cur = 0, max = 0;
        if (!GetMonitorBrightness(h, &min, &cur, &max))
            return false;
        return SetMonitorBrightness(h, static_cast<DWORD>(DisplayModel::brightnessValue(
                                           percent, static_cast<int>(min), static_cast<int>(max)))) != FALSE;
    });
}

// ----- Colour profiles ---------------------------------------------------------

int profileScope(const Path& p)
{
    int scope = kScopeCurrentUser;
    Mscms::get().getScope(p.targetAdapter, p.sourceId, &scope);
    return scope;
}

void readProfiles(const Path& p, DisplayModel::Monitor& m)
{
    const Mscms& mscms = Mscms::get();
    if (!mscms.available())
        return;
    const int scope = profileScope(p);

    LPWSTR* list = nullptr;
    DWORD count = 0;
    if (SUCCEEDED(mscms.getList(scope, p.targetAdapter, p.sourceId, &list, &count)) && list) {
        for (DWORD i = 0; i < count; ++i)
            if (list[i])
                m.colorProfiles << QString::fromWCharArray(list[i]);
        LocalFree(list);
    }
    LPWSTR name = nullptr;
    if (SUCCEEDED(mscms.getDefault(scope, p.targetAdapter, p.sourceId, kProfileIcc,
                                   m.hdrOn ? kExtendedColorMode : kStandardColorMode, &name))
        && name) {
        m.colorProfile = QString::fromWCharArray(name);
        LocalFree(name);
    }
}

DisplayModel::Monitor readMonitor(const Path& p)
{
    DisplayModel::Monitor m;
    m.id         = p.id;
    m.deviceName = p.deviceName;
    m.name       = p.name;
    m.internal   = p.internal;
    readModes(p, m);
    readHdr(p, m.hdrSupported, m.hdrOn);

    const auto scale = readScale(p);
    m.scaleKnown       = scale.valid;
    m.scale            = scale.current;
    m.recommendedScale = scale.recommended;
    m.scaleOptions     = scale.options;

    std::optional<int> brightness;
    if (p.internal)
        brightness = wmiBrightness(p);
    if (!brightness)
        brightness = ddcBrightness(p);
    m.brightnessSupported = brightness.has_value();
    m.brightness          = brightness.value_or(0);

    readProfiles(p, m);
    return m;
}

DISPLAYCONFIG_TOPOLOGY_ID topologyFlag(DisplayModel::Topology t)
{
    switch (t) {
    case DisplayModel::Topology::Internal: return DISPLAYCONFIG_TOPOLOGY_INTERNAL;
    case DisplayModel::Topology::Clone:    return DISPLAYCONFIG_TOPOLOGY_CLONE;
    case DisplayModel::Topology::Extend:   return DISPLAYCONFIG_TOPOLOGY_EXTEND;
    case DisplayModel::Topology::External: return DISPLAYCONFIG_TOPOLOGY_EXTERNAL;
    case DisplayModel::Topology::Unknown:  break;
    }
    return static_cast<DISPLAYCONFIG_TOPOLOGY_ID>(0);
}

class Win32Display final : public DisplayBackend
{
public:
    bool supported() const override { return true; }

    std::vector<DisplayModel::Monitor> monitors() const override
    {
        std::vector<DisplayModel::Monitor> out;
        for (const auto& p : activePaths())
            out.push_back(readMonitor(p));
        return out;
    }

    bool setMode(const QString& id, const DisplayModel::Mode& mode, DisplayModel::Orientation orientation,
                 bool temporary, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        const std::wstring device = p->deviceName.toStdWString();
        DEVMODEW dm{};
        dm.dmSize = sizeof(dm);
        if (!EnumDisplaySettingsExW(device.c_str(), ENUM_CURRENT_SETTINGS, &dm, 0))
            return fail(error, "Could not read the current mode.");
        dm.dmPelsWidth          = static_cast<DWORD>(mode.width);
        dm.dmPelsHeight         = static_cast<DWORD>(mode.height);
        dm.dmDisplayOrientation = static_cast<DWORD>(orientation);
        dm.dmFields             = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYORIENTATION;
        if (mode.refresh > 0) {
            dm.dmDisplayFrequency = static_cast<DWORD>(mode.refresh);
            dm.dmFields |= DM_DISPLAYFREQUENCY;
        }
        LONG r = ChangeDisplaySettingsExW(device.c_str(), &dm, nullptr, CDS_TEST, nullptr);
        if (r == DISP_CHANGE_SUCCESSFUL)
            r = ChangeDisplaySettingsExW(device.c_str(), &dm, nullptr,
                                         temporary ? CDS_FULLSCREEN : CDS_UPDATEREGISTRY, nullptr);
        if (r != DISP_CHANGE_SUCCESSFUL)
            return fail(error, "Could not set the mode: " + changeDisplayMessage(r) + '.');
        return true;
    }

    bool resetMode(const QString& id, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        const LONG r = ChangeDisplaySettingsExW(p->deviceName.toStdWString().c_str(), nullptr, nullptr, 0, nullptr);
        if (r != DISP_CHANGE_SUCCESSFUL)
            return fail(error, "Could not restore the mode: " + changeDisplayMessage(r) + '.');
        return true;
    }

    bool setHdr(const QString& id, bool on, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        bool supported = false;
        bool current = false;
        readHdr(*p, supported, current);
        if (!supported)   // on 24H2 the old call would switch Auto Color Management instead
            return fail(error, "This display does not support HDR.");
        ColorState state{};
        state.header.type      = kSetHdrState;
        state.header.size      = sizeof(state);
        state.header.adapterId = p->targetAdapter;
        state.header.id        = p->targetId;
        state.value            = on ? 1u : 0u;
        LONG r = DisplayConfigSetDeviceInfo(&state.header);
        if (r != ERROR_SUCCESS) {   // before 24H2
            state.header.type = kSetAdvancedColorState;
            r = DisplayConfigSetDeviceInfo(&state.header);
        }
        if (r != ERROR_SUCCESS)
            return fail(error, "Could not switch HDR: " + systemMessage(static_cast<DWORD>(r)));
        return true;
    }

    bool setScale(const QString& id, int percent, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        const auto info = readScale(*p);
        if (!info.valid)
            return fail(error, "Windows did not report the scale of this display.");
        const auto relative = DisplayModel::relativeScale(info, percent);
        if (!relative)
            return fail(error, QString("%1 % is not available at this resolution (up to %2 %).")
                                   .arg(percent).arg(info.options.empty() ? 100 : info.options.back()));
        DpiScaleSet set{};
        set.header.type      = kSetDpiScale;
        set.header.size      = sizeof(set);
        set.header.adapterId = p->sourceAdapter;
        set.header.id        = p->sourceId;
        set.relative         = *relative;
        const LONG r = DisplayConfigSetDeviceInfo(&set.header);
        if (r != ERROR_SUCCESS)
            return fail(error, "Could not set the scale: " + systemMessage(static_cast<DWORD>(r)));
        return true;
    }

    bool setBrightness(const QString& id, int percent, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        if ((p->internal && setWmiBrightness(*p, percent)) || setDdcBrightness(*p, percent))
            return true;
        return fail(error, p->internal ? "Windows did not accept the brightness."
                                       : "The monitor did not accept the brightness (DDC/CI may be off in its menu).");
    }

    bool setColorProfile(const QString& id, const QString& profile, QString* error) override
    {
        const auto p = findPath(id);
        if (!p)
            return fail(error, "The monitor is not connected.");
        const Mscms& mscms = Mscms::get();
        if (!mscms.available())
            return fail(error, "Colour profiles need Windows 10 1809 or newer.");
        bool hdrSupported = false;
        bool hdrOn = false;
        readHdr(*p, hdrSupported, hdrOn);
        const HRESULT hr = mscms.setDefault(profileScope(*p), profile.toStdWString().c_str(), kProfileIcc,
                                            hdrOn ? kExtendedColorMode : kStandardColorMode,
                                            p->targetAdapter, p->sourceId);
        if (FAILED(hr))
            return fail(error, "Could not set the colour profile: " + systemMessage(static_cast<DWORD>(hr)));
        return true;
    }

    DisplayModel::Topology topology() const override
    {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths;
        DISPLAYCONFIG_TOPOLOGY_ID id{};
        if (!queryConfig(QDC_DATABASE_CURRENT, paths, &id))
            return DisplayModel::Topology::Unknown;
        switch (id) {
        case DISPLAYCONFIG_TOPOLOGY_INTERNAL: return DisplayModel::Topology::Internal;
        case DISPLAYCONFIG_TOPOLOGY_CLONE:    return DisplayModel::Topology::Clone;
        case DISPLAYCONFIG_TOPOLOGY_EXTEND:   return DisplayModel::Topology::Extend;
        case DISPLAYCONFIG_TOPOLOGY_EXTERNAL: return DisplayModel::Topology::External;
        default:                              return DisplayModel::Topology::Unknown;
        }
    }

    int connectedDisplays() const override
    {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths;
        if (!queryConfig(QDC_ALL_PATHS, paths))
            return 0;
        std::vector<std::pair<LUID, UINT32>> seen;
        for (const auto& p : paths) {
            if (!p.targetInfo.targetAvailable)
                continue;
            const auto key = std::make_pair(p.targetInfo.adapterId, p.targetInfo.id);
            const bool known = std::any_of(seen.begin(), seen.end(), [&](const auto& k) {
                return k.first.LowPart == key.first.LowPart && k.first.HighPart == key.first.HighPart
                    && k.second == key.second;
            });
            if (!known)
                seen.push_back(key);
        }
        return static_cast<int>(seen.size());
    }

    bool setTopology(DisplayModel::Topology topology, QString* error) override
    {
        const auto flag = topologyFlag(topology);
        if (flag == 0)
            return fail(error, "Unknown display arrangement.");
        const LONG r = SetDisplayConfig(0, nullptr, 0, nullptr, SDC_APPLY | static_cast<UINT32>(flag));
        if (r != ERROR_SUCCESS)
            return fail(error, "Could not switch the displays: " + systemMessage(static_cast<DWORD>(r)));
        return true;
    }

    QString nightLightUrl() const override { return "ms-settings:nightlight"; }
};

} // namespace

DisplayBackend& displayBackend()
{
    static Win32Display backend;
    return backend;
}
