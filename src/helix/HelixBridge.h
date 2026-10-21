#pragma once

#include <optional>

#include <QMap>
#include <QString>
#include <QStringList>

#include "HelixRelease.h"

/**
 * Pure logic for HelixSR 2.x: the app builds and installs it through HelixSR's own Python entry points
 * instead of helixsr-setup.ps1, whose last step installs into every game it finds.
 *
 *   setup/helixsr_setup.py   builds the network (needs Python with numpy, and DXC)
 *   setup/helixsr_bridge.py  installs into / removes from one game and answers in JSON
 */
namespace Helix {

// The bridge protocol this app speaks (its replies carry "protocol").
constexpr int kBridgeProtocol = 1;

inline const QString kSetupScript  = QStringLiteral("setup/helixsr_setup.py");
inline const QString kBridgeScript = QStringLiteral("setup/helixsr_bridge.py");
inline const QString kSetupPs1     = QStringLiteral("helixsr-setup.ps1");

struct ToolDownload
{
    QString url;
    QString sha256;   // lower case
};

// The `$Downloads` table of a release's helixsr-setup.ps1: name (python, numpy, dxc) -> URL and SHA-256.
// They change per release, so they are read from the release rather than kept in the app.
QMap<QString, ToolDownload> setupDownloads(const QString& ps1Text);

// The tools the setup needs that setupDownloads() must provide.
inline const QStringList kSetupTools = {QStringLiteral("python"), QStringLiteral("numpy"), QStringLiteral("dxc")};

// Arguments for Python to build the network of the package in `packageDir`. --yes agrees to the
// download of NVIDIA's DLSS DLL (the user agreed in the app); --no-fg skips frame generation, which
// only HelixSR Scaler uses.
QStringList setupBuildArgs(const QString& packageDir, const QString& dxcExe);
QStringList setupStatusArgs(const QString& packageDir);

// "network=1 fg=0" from `helixsr_setup.py --status`; nullopt when the output has no such line.
struct BuildStatus
{
    bool network{false};
    bool frameGeneration{false};
};
std::optional<BuildStatus> parseBuildStatus(const QString& output);

// Arguments for Python to install HelixSR's FSR drop-in into (or remove it from) the game in `gameFolder`.
// Always the FSR drop-in (--way fsr): the bridge's own choice (auto) is HelixSR Scaler for most games.
QStringList bridgeArgs(const QString& packageDir, bool install, const QString& gameFolder);

struct BridgeReply
{
    bool    ok{false};
    bool    compatible{false};   // a JSON reply with the protocol this app speaks
    QString error;
    QStringList messages;
    QString installedWay;        // "fsr", "scaler", ...; empty when nothing is installed
    std::optional<Version> installedVersion;
};

// The bridge prints one JSON object as its last line; anything before it (Python warnings) is skipped.
BridgeReply parseBridgeReply(const QString& output);

} // namespace Helix
