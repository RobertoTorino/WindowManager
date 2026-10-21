#include "GameConfigFiles.h"

#include "core/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QStringList>

namespace GameConfig {

namespace {

enum class Encoding { Utf8, Utf8Bom, Utf16Le };

struct Text
{
    bool     ok{false};
    QString  text;
    Encoding encoding{Encoding::Utf8};
};

// Unreal writes its ini files as plain text, UTF-8 with a BOM or UTF-16; whichever it is stays.
Text readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray data = file.readAll();
    if (data.startsWith("\xEF\xBB\xBF"))
        return {true, QString::fromUtf8(data.mid(3)), Encoding::Utf8Bom};
    if (data.startsWith("\xFF\xFE"))
        return {true, QStringDecoder(QStringDecoder::Utf16LE)(data.mid(2)), Encoding::Utf16Le};
    return {true, QString::fromUtf8(data), Encoding::Utf8};
}

bool writeBytes(const QString& path, const QByteArray& data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(data);
    return file.commit();
}

bool writeText(const QString& path, const QString& text, Encoding encoding)
{
    QByteArray data;
    switch (encoding) {
    case Encoding::Utf8:    data = text.toUtf8(); break;
    case Encoding::Utf8Bom: data = "\xEF\xBB\xBF" + text.toUtf8(); break;
    case Encoding::Utf16Le: data = "\xFF\xFE" + QByteArray(QStringEncoder(QStringEncoder::Utf16LE)(text)); break;
    }
    return writeBytes(path, data);
}

QString fileName(const QString& path) { return QFileInfo(path).fileName(); }

void log(const QString& line)
{
    Logger::instance().info(line.toStdString(), "GameConfig");
}

// Keeps a copy of the original once, before the first change
bool keepOriginal(const QString& path, QString* message)
{
    const QString copy = path + kOriginalSuffix;
    if (QFile::exists(copy) || QFile::copy(path, copy))
        return true;
    if (message)
        *message = QString("Could not keep a copy of %1").arg(fileName(path));
    return false;
}

bool writeIni(const QString& path, IniFile kind, const Settings& s, QString* message)
{
    if (!QFile::exists(path))
        return true;
    const QString copy = path + kOriginalSuffix;
    const bool hasCopy = QFile::exists(copy);
    if (!hasCopy && !touches(kind, s))
        return true;   // never changed and nothing to change

    const Text current = readText(path);
    const Text original = hasCopy ? readText(copy) : current;
    if (!current.ok || !original.ok) {
        if (message)
            *message = QString("Could not read %1").arg(fileName(path));
        return false;
    }
    const QList<IniText::Entry> entries = entriesFor(kind, s, original.text);
    const QString changed = IniText::apply(current.text, entries, "=");
    if (changed == current.text)
        return true;

    if (!keepOriginal(path, message))
        return false;
    if (!writeText(path, changed, current.encoding)) {
        if (message)
            *message = QString("Could not write %1 (read-only, or the game is running?)").arg(fileName(path));
        return false;
    }
    for (const IniText::Entry& e : entries) {
        const auto before = IniText::value(current.text, e.section, e.key);
        if (before != e.value)
            log(QString("%1 [%2] %3: %4 -> %5").arg(fileName(path), e.section, e.key,
                                                    before.value_or("(not set)"), e.value.value_or("(removed)")));
    }
    return true;
}

bool writeJConfigFile(const QString& path, const Settings& s, QString* message)
{
    if (!QFile::exists(path))
        return true;
    const QString copy = path + kOriginalSuffix;
    const bool hasCopy = QFile::exists(copy);
    if (!hasCopy && !touchesJConfig(s))
        return true;

    QByteArray current;
    QByteArray originalData;
    {   // closed again before the file is replaced (Windows refuses to replace an open file)
        QFile file(path);
        QFile originalFile(hasCopy ? copy : path);
        if (!file.open(QIODevice::ReadOnly) || !originalFile.open(QIODevice::ReadOnly)) {
            if (message)
                *message = QString("Could not read %1").arg(fileName(path));
            return false;
        }
        current = file.readAll();
        originalData = originalFile.readAll();
    }
    const auto now = readJConfig(current);
    const auto original = readJConfig(originalData);
    if (!now || !original) {
        log(QString("%1 is not the expected %2 bytes: JConfig left alone").arg(fileName(path)).arg(kJConfigFileSize));
        return true;
    }
    const JConfigWindow wanted = jconfigFor(s, *original);
    if (wanted == *now)
        return true;

    if (!keepOriginal(path, message))
        return false;
    if (!writeBytes(path, writeJConfig(current, wanted))) {
        if (message)
            *message = QString("Could not write %1 (read-only, or JConfig is open?)").arg(fileName(path));
        return false;
    }
    log(QString("%1 (JConfig): %2 -> %3").arg(fileName(path), describe(*now), describe(wanted)));
    return true;
}

bool writeAll(const Layout& l, const Settings& s, QString* message)
{
    return writeIni(l.engineIni, IniFile::Engine, s, message)
        && writeIni(l.userSettingsIni, IniFile::UserSettings, s, message)
        && writeIni(l.platformIni, IniFile::Platform, s, message)
        && writeJConfigFile(l.jconfigFile, s, message);
}

QString valueText(const std::optional<QString>& v)
{
    return v ? *v : QString("not set");
}

} // namespace

Status status(const QString& exePath)
{
    Status st;
    st.layout = layoutFor(exePath);
    if (!st.layout.valid())
        return st;
    const Layout& l = st.layout;
    st.engineIni = QFile::exists(l.engineIni);
    st.userSettingsIni = QFile::exists(l.userSettingsIni);
    st.platformIni = QFile::exists(l.platformIni);
    for (const QString& path : {l.engineIni, l.userSettingsIni, l.platformIni, l.jconfigFile})
        st.changed = st.changed || QFile::exists(path + kOriginalSuffix);
    if (!st.engineIni)
        return st;

    QStringList lines;
    const Text engine = readText(l.engineIni);
    const Text user = readText(l.userSettingsIni);
    lines << QString("Render scale: r.ScreenPercentage %1, sg.ResolutionQuality %2")
                 .arg(valueText(IniText::value(engine.text, "SystemSettings", "r.ScreenPercentage")),
                      valueText(IniText::value(user.text, "ScalabilityGroups", "sg.ResolutionQuality")));
    QString resolution = QString("Resolution: user settings %1 x %2, engine %3 x %4")
                             .arg(valueText(IniText::value(user.text, "/Script/Engine.GameUserSettings", "ResolutionSizeX")),
                                  valueText(IniText::value(user.text, "/Script/Engine.GameUserSettings", "ResolutionSizeY")),
                                  valueText(IniText::value(engine.text, "SystemSettings", "ResX")),
                                  valueText(IniText::value(engine.text, "SystemSettings", "ResY")));
    if (st.platformIni)
        resolution += QString(", r.setres %1")
                          .arg(valueText(IniText::value(readText(l.platformIni).text, "SystemSettings", "r.setres")));
    lines << resolution;

    st.renderScale = IniText::value(engine.text, "SystemSettings", "r.ScreenPercentage").value_or(QString()).toInt();
    st.width = IniText::value(user.text, "/Script/Engine.GameUserSettings", "ResolutionSizeX").value_or(QString()).toInt();
    st.height = IniText::value(user.text, "/Script/Engine.GameUserSettings", "ResolutionSizeY").value_or(QString()).toInt();
    if ((st.width <= 0 || st.height <= 0) && st.platformIni) {
        const QStringList setres = IniText::value(readText(l.platformIni).text, "SystemSettings", "r.setres")
                                       .value_or(QString()).split('x');
        if (setres.size() == 2) {
            st.width = setres[0].trimmed().toInt();
            st.height = setres[1].trimmed().toInt();
        }
    }

    QFile jconfig(l.jconfigFile);
    if (jconfig.open(QIODevice::ReadOnly)) {
        if (const auto w = readJConfig(jconfig.readAll())) {
            st.jconfig = true;
            lines << "JConfig: " + describe(*w);
        }
    }
    st.summary = lines.join('\n');
    return st;
}

bool apply(const QString& exePath, const Settings& s, QString* message)
{
    const Layout l = layoutFor(exePath);
    if (!l.valid() || !QFile::exists(l.engineIni)) {
        if (message)
            *message = "This game has no Unreal Engine config files this app can change";
        return false;
    }
    log("Writing the game's config: " + describe(s));
    if (!writeAll(l, s, message))
        return false;
    if (message)
        *message = "Game config written: " + describe(s) + ". It takes effect when the game starts.";
    return true;
}

bool restore(const QString& exePath, QString* message)
{
    const Layout l = layoutFor(exePath);
    if (!l.valid())
        return true;
    log("Putting the game's original config back");
    if (!writeAll(l, Settings{}, message))
        return false;
    for (const QString& path : {l.engineIni, l.userSettingsIni, l.platformIni, l.jconfigFile})
        QFile::remove(path + kOriginalSuffix);
    if (message)
        *message = "The game's original config is back.";
    return true;
}

} // namespace GameConfig
