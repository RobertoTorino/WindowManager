#include "MagpieConfig.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Magpie {

namespace {

// The root object, or an error text.
QJsonObject parse(const QByteArray& config, QString* error)
{
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(config, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        *error = config.trimmed().isEmpty() ? QString("Magpie has not written its settings yet")
                                            : "Magpie's config is not valid JSON: " + pe.errorString();
        return {};
    }
    return doc.object();
}

bool ours(const QJsonObject& profile)
{
    return profile["name"].toString().startsWith(kProfilePrefix);
}

bool sameRules(const QJsonObject& profile, const QString& exePath, const QString& windowClass)
{
    return profile["pathRule"].toString() == exePath && profile["classNameRule"].toString() == windowClass;
}

EditResult done(QJsonObject root)
{
    // Magpie reads keys by name, so Qt's key order does not matter; integers stay integers.
    return EditResult{true, QJsonDocument(root).toJson(QJsonDocument::Indented), {}};
}

EditResult failed(const QString& error)
{
    return EditResult{false, {}, error};
}

} // namespace

QJsonObject toJson(const GameSettings& settings)
{
    QJsonObject obj;
    obj["Mode"] = settings.mode;
    if (settings.game3D)
        obj["3DGameMode"] = true;
    return obj;
}

GameSettings gameSettingsFromJson(const QJsonObject& obj)
{
    return GameSettings{obj["Mode"].toString().trimmed(), obj["3DGameMode"].toBool(false)};
}

QStringList scalingModes(const QByteArray& config)
{
    QString error;
    QStringList out;
    for (const auto& m : parse(config, &error)["scalingModes"].toArray())
        out << m.toObject()["name"].toString();
    return out;
}

QList<GameProfile> gameProfiles(const QByteArray& config)
{
    QString error;
    const QJsonObject root = parse(config, &error);
    const QStringList modes = scalingModes(config);
    QList<GameProfile> out;
    for (const auto& v : root["profiles"].toArray()) {
        const QJsonObject p = v.toObject();
        if (!ours(p))
            continue;
        GameProfile g;
        g.name = p["name"].toString().mid(kProfilePrefix.size());
        g.exePath = p["pathRule"].toString();
        g.windowClass = p["classNameRule"].toString();
        const int mode = p["scalingMode"].toInt(-1);
        g.scalingMode = mode >= 0 && mode < modes.size() ? modes[mode] : QString();
        const int auto_ = p["autoScale"].toInt();
        g.autoScale = auto_ == 1 ? AutoScale::FullScreen : auto_ == 2 ? AutoScale::Windowed : AutoScale::Off;
        g.game3D = p["3DGameMode"].toBool();
        out << g;
    }
    return out;
}

EditResult setGameProfile(const QByteArray& config, const GameProfile& game)
{
    if (game.name.trimmed().isEmpty() || game.exePath.isEmpty() || game.windowClass.isEmpty())
        return failed("The game's name, exe path and window class are needed for a Magpie profile");

    QString error;
    QJsonObject root = parse(config, &error);
    if (!error.isEmpty())
        return failed(error);

    const QStringList modes = scalingModes(config);
    if (modes.isEmpty())
        return failed("Magpie's config has no scaling modes yet (start Magpie once)");
    int mode = -1;
    for (int i = 0; i < modes.size() && mode < 0; ++i) {
        if (modes[i].compare(game.scalingMode, Qt::CaseInsensitive) == 0)
            mode = i;
    }
    if (mode < 0)
        return failed(QString("Magpie has no scaling mode \"%1\" (it has: %2)").arg(game.scalingMode, modes.join(", ")));

    QJsonArray profiles = root["profiles"].toArray();
    if (profiles.isEmpty() || !profiles[0].isObject())
        return failed("Magpie's config has no default profile yet (start Magpie once)");

    int index = -1;
    for (int i = 1; i < profiles.size(); ++i) {
        const QJsonObject p = profiles[i].toObject();
        if (!sameRules(p, game.exePath, game.windowClass))
            continue;
        if (!ours(p))
            return failed(QString("Magpie already has a profile for this game (\"%1\"), made in Magpie; "
                                  "change or delete it there").arg(p["name"].toString()));
        index = i;
    }

    // a new profile takes Magpie's defaults (capture method, cursor, ...) from the default profile
    QJsonObject p = index > 0 ? profiles[index].toObject() : profiles[0].toObject();
    p["name"] = kProfilePrefix + game.name.trimmed();
    p["packaged"] = false;
    p["pathRule"] = game.exePath;
    p["classNameRule"] = game.windowClass;
    p["autoScale"] = static_cast<int>(game.autoScale);
    p["scalingMode"] = mode;
    p["3DGameMode"] = game.game3D;
    if (!p.contains("launcherPath"))
        p["launcherPath"] = QString();
    if (!p.contains("launchParameters"))
        p["launchParameters"] = QString();

    if (index > 0)
        profiles[index] = p;
    else
        profiles.append(p);
    root["profiles"] = profiles;
    return done(root);
}

EditResult removeGameProfiles(const QByteArray& config, const QString& exePath)
{
    QString error;
    QJsonObject root = parse(config, &error);
    if (!error.isEmpty())
        return failed(error);

    const QJsonArray profiles = root["profiles"].toArray();
    QJsonArray kept;
    for (int i = 0; i < profiles.size(); ++i) {
        const QJsonObject p = profiles[i].toObject();
        const bool drop = i > 0 && ours(p) && (exePath.isEmpty() || p["pathRule"].toString() == exePath);
        if (!drop)
            kept.append(profiles[i]);
    }
    root["profiles"] = kept;
    return done(root);
}

} // namespace Magpie
