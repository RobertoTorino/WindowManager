#include "HelixBridge.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace Helix {

QMap<QString, ToolDownload> setupDownloads(const QString& ps1Text)
{
    // python = @{ url = '...'
    //             sha = '...' }
    static const QRegularExpression entry(
        R"((\w+)\s*=\s*@\{\s*url\s*=\s*'([^']+)'\s*;?\s*sha\s*=\s*'([0-9a-fA-F]{64})'\s*\})");
    QMap<QString, ToolDownload> out;
    auto it = entry.globalMatch(ps1Text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString url = m.captured(2);
        if (url.startsWith("https://"))
            out.insert(m.captured(1).toLower(), ToolDownload{url, m.captured(3).toLower()});
    }
    return out;
}

QStringList setupBuildArgs(const QString& packageDir, const QString& dxcExe)
{
    return {"-I", packageDir + "/" + kSetupScript, packageDir, "--dxc", dxcExe, "--yes", "--no-fg"};
}

QStringList setupStatusArgs(const QString& packageDir)
{
    return {"-I", packageDir + "/" + kSetupScript, packageDir, "--status"};
}

std::optional<BuildStatus> parseBuildStatus(const QString& output)
{
    static const QRegularExpression line(R"(network=([01])\s+fg=([01]))");
    const auto m = line.match(output);
    if (!m.hasMatch())
        return std::nullopt;
    return BuildStatus{m.captured(1) == "1", m.captured(2) == "1"};
}

QStringList bridgeArgs(const QString& packageDir, bool install, const QString& gameFolder)
{
    QStringList args{"-I", packageDir + "/" + kBridgeScript, packageDir, install ? "install" : "remove",
                     "--game", gameFolder};
    if (install)
        args << "--way" << "fsr";
    return args;
}

BridgeReply parseBridgeReply(const QString& output)
{
    BridgeReply r;
    const QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString line = it->trimmed();
        if (!line.startsWith('{'))
            continue;
        const auto doc = QJsonDocument::fromJson(line.toUtf8());
        if (!doc.isObject())
            continue;
        const QJsonObject o = doc.object();
        r.compatible = o["protocol"].toInt() == kBridgeProtocol;
        r.ok = o["ok"].toBool();
        r.error = o["error"].toString();
        for (const auto& m : o["messages"].toArray())
            r.messages << m.toString();
        const QJsonObject game = o["game"].toObject();
        r.installedWay = game["installed"].toString();
        r.installedVersion = parseVersion(game["version"].toString());
        break;
    }
    return r;
}

} // namespace Helix
