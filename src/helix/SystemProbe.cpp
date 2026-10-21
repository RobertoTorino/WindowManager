#include "SystemProbe.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStorageInfo>

#ifdef Q_OS_WIN
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#endif

namespace Helix {

namespace {

#ifdef Q_OS_WIN
GpuInfo probeAdapter(IDXGIAdapter1* adapter, const DXGI_ADAPTER_DESC1& desc)
{
    GpuInfo gpu;
    gpu.name = QString::fromWCharArray(desc.Description).trimmed();
    gpu.vendorId = desc.VendorId;
    gpu.videoMemoryMB = desc.DedicatedVideoMemory / (1024 * 1024);
    gpu.software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;

    ComPtr<ID3D12Device> device;
    const HRESULT hr = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        gpu.error = QString("D3D12CreateDevice failed (0x%1)").arg(static_cast<quint32>(hr), 8, 16, QChar('0'));
        return gpu;
    }
    gpu.d3d12 = true;

    // The highest shader model the driver accepts: ask from the top; unknown values are rejected.
    for (int model = 0x6A; model >= 0x60; --model) {
        D3D12_FEATURE_DATA_SHADER_MODEL sm{static_cast<D3D_SHADER_MODEL>(model)};
        if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &sm, sizeof(sm)))) {
            gpu.shaderModel = (sm.HighestShaderModel >> 4) * 10 + (sm.HighestShaderModel & 0xF);
            break;
        }
    }

    D3D12_FEATURE_DATA_D3D12_OPTIONS4 options4{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS4, &options4, sizeof(options4))))
        gpu.native16Bit = options4.Native16BitShaderOpsSupported;

    D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1)))) {
        gpu.waveMin = static_cast<int>(options1.WaveLaneCountMin);
        gpu.waveMax = static_cast<int>(options1.WaveLaneCountMax);
    }
    return gpu;
}
#endif

qint64 freeSpaceMB(const QString& dir)
{
    // the folder may not exist yet: ask about the nearest one that does
    QString probe = QFileInfo(dir).absoluteFilePath();
    while (!QFileInfo::exists(probe)) {
        const QString parent = QFileInfo(probe).absolutePath();
        if (parent == probe)
            return -1;
        probe = parent;
    }
    const QStorageInfo storage(probe);
    return storage.isValid() ? storage.bytesAvailable() / (1024 * 1024) : -1;
}

} // namespace

SystemInfo probeSystem(const QString& storageDir)
{
    SystemInfo info;
    info.freeDiskMB = freeSpaceMB(storageDir);

#ifdef Q_OS_WIN
    ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc)))
                info.gpus.append(probeAdapter(adapter.Get(), desc));
            adapter.Reset();
        }
    }

    const QString system32 = qEnvironmentVariable("SystemRoot") + "/System32";
    info.hasTar = QFile::exists(system32 + "/tar.exe");
#endif
    // Elsewhere there is no Direct3D 12 to ask: no adapters, so HelixSR is reported as unsupported.
    return info;
}

} // namespace Helix
