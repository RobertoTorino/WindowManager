#include "GameConfigModel.h"

#include <QDir>
#include <QStringList>
#include <QtEndian>

#include <algorithm>

namespace GameConfig {

namespace {

constexpr const char* kSystemSettings = "SystemSettings";
constexpr const char* kUserSettings   = "/Script/Engine.GameUserSettings";
constexpr const char* kScalability    = "ScalabilityGroups";

// int32 offsets in monitor_misc.bin
constexpr int kWindowX = 60;
constexpr int kWindowY = 64;
constexpr int kWindowW = 68;
constexpr int kWindowH = 72;
constexpr int kDxW     = 76;
constexpr int kDxH     = 80;

bool hasResolution(const Settings& s) { return s.width > 0 && s.height > 0; }

QString parentOf(const QString& path)
{
    const int slash = path.lastIndexOf('/');
    return slash > 0 ? path.left(slash) : QString();
}

QString nameOf(const QString& path) { return path.mid(path.lastIndexOf('/') + 1); }

// The setting's value, or the original file's (nullopt: the original has no such key)
IniText::Entry entry(const QString& original, const char* section, const char* key, bool set, const QString& value)
{
    return {section, key, set ? std::optional<QString>(value) : IniText::value(original, section, key)};
}

qint32 intAt(const QByteArray& data, int offset)
{
    return qFromLittleEndian<qint32>(data.constData() + offset);
}

void setIntAt(QByteArray& data, int offset, qint32 value)
{
    qToLittleEndian<qint32>(value, data.data() + offset);
}

} // namespace

bool Settings::isEmpty() const
{
    return renderScale == 0 && !hasResolution(*this) && !(jconfigWindow && windowWidth > 0 && windowHeight > 0);
}

QJsonObject toJson(const Settings& s)
{
    QJsonObject o;
    o["Enabled"]       = s.enabled;
    o["RenderScale"]   = s.renderScale;
    o["Width"]         = s.width;
    o["Height"]        = s.height;
    o["JConfigWindow"] = s.jconfigWindow;
    o["WindowX"]       = s.windowX;
    o["WindowY"]       = s.windowY;
    o["WindowWidth"]   = s.windowWidth;
    o["WindowHeight"]  = s.windowHeight;
    return o;
}

Settings settingsFromJson(const QJsonObject& o)
{
    Settings s;
    s.enabled     = o["Enabled"].toBool(false);
    s.renderScale = o["RenderScale"].toInt(0);
    if (s.renderScale != 0)
        s.renderScale = std::clamp(s.renderScale, kMinScale, kMaxScale);
    s.width  = std::max(0, o["Width"].toInt(0));
    s.height = std::max(0, o["Height"].toInt(0));
    if (s.width == 0 || s.height == 0)
        s.width = s.height = 0;
    s.jconfigWindow = o["JConfigWindow"].toBool(false);
    s.windowX       = o["WindowX"].toInt(0);
    s.windowY       = o["WindowY"].toInt(0);
    s.windowWidth   = std::max(0, o["WindowWidth"].toInt(0));
    s.windowHeight  = std::max(0, o["WindowHeight"].toInt(0));
    if (s.windowWidth == 0 || s.windowHeight == 0) {
        s.windowWidth = s.windowHeight = 0;
        s.jconfigWindow = false;
    }
    return s;
}

QString describe(const Settings& s)
{
    QStringList parts;
    if (s.renderScale != 0)
        parts << QString("render scale %1 %").arg(s.renderScale);
    if (hasResolution(s))
        parts << QString("%1 x %2").arg(s.width).arg(s.height);
    if (s.jconfigWindow && s.windowWidth > 0)
        parts << QString("JConfig window %1 x %2 at %3,%4").arg(s.windowWidth).arg(s.windowHeight)
                                                             .arg(s.windowX).arg(s.windowY);
    return parts.isEmpty() ? QString("no changes") : parts.join(", ");
}

Layout layoutFor(const QString& exePath)
{
    const QString exe = QDir::cleanPath(QString(exePath).replace('\\', '/'));   // Windows paths on any system
    const QString platformDir = parentOf(exe);
    const QString binaries = parentOf(platformDir);
    const QString project = parentOf(binaries);
    const QString root = parentOf(project);
    const QString platform = nameOf(platformDir);
    if (root.isEmpty() || nameOf(binaries).compare("Binaries", Qt::CaseInsensitive) != 0
        || (platform.compare("Win64", Qt::CaseInsensitive) != 0 && platform.compare("Win32", Qt::CaseInsensitive) != 0))
        return {};

    Layout l;
    l.project         = project;
    l.engineIni       = project + "/Config/DefaultEngine.ini";
    l.userSettingsIni = project + "/Config/DefaultGameUserSettings.ini";
    l.platformIni     = root + "/Engine/Config/Windows/WindowsEngine.ini";
    l.jconfigFile     = platformDir + "/monitor_misc.bin";
    return l;
}

QString exeForConfig(const QString& applicationPath, const QString& childExePath)
{
    if (layoutFor(applicationPath).valid() || childExePath.isEmpty())
        return applicationPath;
    return childExePath;
}

QList<IniText::Entry> entriesFor(IniFile file, const Settings& s, const QString& original)
{
    const bool scale = s.renderScale != 0;
    const bool size = hasResolution(s);
    const QString w = QString::number(s.width);
    const QString h = QString::number(s.height);
    switch (file) {
    case IniFile::Engine:
        // [SystemSettings] outranks the scalability groups, and only there may the scale exceed 100
        return {
            entry(original, kSystemSettings, "r.ScreenPercentage", scale, QString::number(s.renderScale)),
            entry(original, kSystemSettings, "r.Upscale.Quality", scale, QString::number(kUpscaleQuality)),
            entry(original, kSystemSettings, "ResX", size, w),
            entry(original, kSystemSettings, "ResY", size, h),
        };
    case IniFile::UserSettings:
        // the engine caps sg.ResolutionQuality at 100: above that r.ScreenPercentage does the work
        return {
            entry(original, kUserSettings, "ResolutionSizeX", size, w),
            entry(original, kUserSettings, "ResolutionSizeY", size, h),
            entry(original, kScalability, "sg.ResolutionQuality", scale,
                  QString::number(std::min(s.renderScale, 100))),
        };
    case IniFile::Platform:
        return {entry(original, kSystemSettings, "r.setres", size, QString("%1x%2").arg(w, h))};
    }
    return {};
}

bool touches(IniFile file, const Settings& s)
{
    switch (file) {
    case IniFile::Engine:
    case IniFile::UserSettings:
        return s.renderScale != 0 || hasResolution(s);
    case IniFile::Platform:
        return hasResolution(s);
    }
    return false;
}

std::optional<JConfigWindow> readJConfig(const QByteArray& data)
{
    if (data.size() != kJConfigFileSize)
        return std::nullopt;
    return JConfigWindow{intAt(data, kWindowX), intAt(data, kWindowY), intAt(data, kWindowW),
                         intAt(data, kWindowH), intAt(data, kDxW), intAt(data, kDxH)};
}

QByteArray writeJConfig(QByteArray data, const JConfigWindow& w)
{
    if (data.size() != kJConfigFileSize)
        return data;
    setIntAt(data, kWindowX, w.x);
    setIntAt(data, kWindowY, w.y);
    setIntAt(data, kWindowW, w.width);
    setIntAt(data, kWindowH, w.height);
    setIntAt(data, kDxW, w.dxWidth);
    setIntAt(data, kDxH, w.dxHeight);
    return data;
}

JConfigWindow jconfigFor(const Settings& s, const JConfigWindow& original)
{
    JConfigWindow w = original;
    if (hasResolution(s)) {
        w.dxWidth = s.width;
        w.dxHeight = s.height;
    }
    if (s.jconfigWindow && s.windowWidth > 0 && s.windowHeight > 0) {
        w.x = s.windowX;
        w.y = s.windowY;
        w.width = s.windowWidth;
        w.height = s.windowHeight;
    }
    return w;
}

bool touchesJConfig(const Settings& s)
{
    return hasResolution(s) || (s.jconfigWindow && s.windowWidth > 0 && s.windowHeight > 0);
}

QString describe(const JConfigWindow& w)
{
    return QString("window %1 x %2 at %3,%4, DX resolution %5 x %6")
        .arg(w.width).arg(w.height).arg(w.x).arg(w.y).arg(w.dxWidth).arg(w.dxHeight);
}

} // namespace GameConfig
