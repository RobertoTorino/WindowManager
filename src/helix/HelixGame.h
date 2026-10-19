#pragma once

#include <functional>

#include <QObject>
#include <QString>
#include <QStringList>

#include "HelixRelease.h"

namespace Helix {

/** What HelixSR could do for one game, judged from the files in its folder. */
struct GameScan
{
    enum class Status {
        Unsupported,   // no FSR 3.1 DLL found: the FSR drop-in cannot be used
        Ready,         // has an FSR 3.1 DLL, HelixSR is not in it
        Installed,     // the game's FSR DLL is HelixSR's
        Replaced,      // HelixSR was installed, but the game put its own DLL back (update / verify)
    };

    Status      status{Status::Unsupported};
    QString     root;                       // the folder that was searched
    QStringList targets;                    // the game's FSR 3.1 DLLs (absolute paths)
    std::optional<Version> installedVersion;
};

// The DLL names of AMD FidelityFX 3.1. A folder with the upscaler DLL uses it; one with
// only the generic DLL (games without the separate upscaler file) uses that.
inline const QString kFsrUpscalerDll = QStringLiteral("amd_fidelityfx_upscaler_dx12.dll");
inline const QString kFsrGenericDll  = QStringLiteral("amd_fidelityfx_dx12.dll");

// "<dll>.original.dll": where the drop-in keeps the game's own file.
QString originalNameFor(const QString& dllPath);

// FSR 3.1 DLLs in `folder`, searched `maxDepth` folder levels down.
QStringList findFsrTargets(const QString& folder, int maxDepth = 16);

// The folder to search for the executable at `exePath`: its own folder, or the game's folder
// when the exe sits in bin / x64 / Win64 / Binaries (Unreal Engine: <game>/<project>/Binaries/Win64).
QString gameRootFor(const QString& exePath);

// The game for the executable at `exePath`, searched from gameRootFor().
GameScan scanGame(const QString& exePath);

// True when the file at `path` is a HelixSR DLL.
bool isHelixDll(const QString& path);

// scanGame() on a worker thread, so a huge game folder never blocks the UI. `done` runs on the thread
// of `context` (queued); it is skipped when `context` has been destroyed by then.
void scanGameAsync(QObject* context, const QString& exePath, std::function<void(const GameScan&)> done);

} // namespace Helix
