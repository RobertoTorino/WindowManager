#include <QtTest>

#include "gameconfig/GameConfigFiles.h"
#include "gameconfig/GameConfigModel.h"

#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtEndian>

using namespace GameConfig;

namespace {

// Excerpts of Tekken 7 Fated Retribution R2's files (Windows line endings, as on disk)
const QByteArray kEngineIni =
    "[URL]\r\nGameName=TekkenGame\r\n\r\n[SystemSettings]\r\nResX=1920\r\nResY=1080\r\nFullscreen=true\r\n"
    "r.TextureStreaming=0\r\n\r\n[/Script/Engine.Engine]\r\nbSmoothFrameRate=False\r\n\r\n\r\n";
const QByteArray kUserSettingsIni =
    "\r\n[/Script/Engine.GameUserSettings]\r\nResolutionSizeX=1920\r\nResolutionSizeY=1080\r\nFullscreenMode=0\r\n"
    "bUseVSync=True\r\nVersion=5\r\n\r\n[ScalabilityGroups]\r\nsg.ResolutionQuality=100\r\nsg.AntiAliasingQuality=2\r\n";
const QByteArray kPlatformIni =
    "[Audio]\r\nAudioDeviceModuleName=XAudio2\r\n\r\n[SystemSettings]\r\nr.setres=1280x720\r\n";

// monitor_misc.bin as JConfig wrote it on 2026-10-20 (window 1280x720 at 16,16, DX 1920x1080)
QByteArray jconfigBytes()
{
    const qint32 values[] = {1, 0, 1, 1, 1, 1, 0, 250, 0, 0, 0, 1, 1, 1, 0, 16, 16, 1280, 720, 1920, 1080};
    QByteArray data(kJConfigFileSize, '\0');
    for (int i = 0; i < int(std::size(values)); ++i)
        qToLittleEndian<qint32>(values[i], data.data() + 4 * i);
    qToLittleEndian<qint32>(50, data.data() + 124);
    return data;
}

QByteArray readAll(const QString& path)
{
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

void writeAll(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(data);
}

// A Tekken-like install in `root`; returns the exe path
QString makeGame(const QString& root)
{
    const QString exe = root + "/TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe";
    writeAll(exe, "MZ");
    writeAll(root + "/TekkenGame/Config/DefaultEngine.ini", kEngineIni);
    writeAll(root + "/TekkenGame/Config/DefaultGameUserSettings.ini", kUserSettingsIni);
    writeAll(root + "/Engine/Config/Windows/WindowsEngine.ini", kPlatformIni);
    writeAll(root + "/TekkenGame/Binaries/Win64/monitor_misc.bin", jconfigBytes());
    return exe;
}

std::optional<QString> valueIn(const QList<IniText::Entry>& entries, const QString& key)
{
    for (const auto& e : entries)
        if (e.key == key)
            return e.value;
    return QString("<missing>");
}

} // namespace

class TstGameConfig : public QObject
{
    Q_OBJECT
private slots:
    void layout()
    {
        const Layout l = layoutFor("C:/Games/TK7/TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe");
        QVERIFY(l.valid());
        QCOMPARE(l.project, QString("C:/Games/TK7/TekkenGame"));
        QCOMPARE(l.engineIni, QString("C:/Games/TK7/TekkenGame/Config/DefaultEngine.ini"));
        QCOMPARE(l.userSettingsIni, QString("C:/Games/TK7/TekkenGame/Config/DefaultGameUserSettings.ini"));
        QCOMPARE(l.platformIni, QString("C:/Games/TK7/Engine/Config/Windows/WindowsEngine.ini"));
        QCOMPARE(l.jconfigFile, QString("C:/Games/TK7/TekkenGame/Binaries/Win64/monitor_misc.bin"));

        QVERIFY(layoutFor("C:\\Games\\X\\Proj\\binaries\\win32\\game.exe").valid());   // native separators, any case
        QVERIFY(!layoutFor("C:/Games/Other/game.exe").valid());
        QVERIFY(!layoutFor("C:/Games/X/Proj/Binaries/Linux/game").valid());
        QVERIFY(!layoutFor("").valid());
    }

    void configExeOfABatchGame()
    {
        const QString unreal = "C:/Games/SHf/SHf/Binaries/Win64/SHf-Win64-Shipping.exe";
        QCOMPARE(exeForConfig("C:/Games/SHf/Launcher.bat", unreal), unreal);
        QCOMPARE(exeForConfig("C:/Games/SHf/Launcher.bat", ""), QString("C:/Games/SHf/Launcher.bat"));
        const QString tekken = "C:/Games/TK7/TekkenGame/Binaries/Win64/TekkenGame-Win64-Shipping.exe";
        QCOMPARE(exeForConfig(tekken, "C:/elsewhere/other.exe"), tekken);   // its own path wins
    }

    void engineKeys()
    {
        const QString original = QString::fromUtf8(kEngineIni);
        Settings s;
        s.renderScale = 150;
        auto e = entriesFor(IniFile::Engine, s, original);
        QCOMPARE(valueIn(e, "r.ScreenPercentage"), std::optional<QString>("150"));
        QCOMPARE(valueIn(e, "r.Upscale.Quality"), std::optional<QString>("3"));
        QCOMPARE(valueIn(e, "ResX"), std::optional<QString>("1920"));   // left: the original's
        for (const auto& entry : e)
            QCOMPARE(entry.section, QString("SystemSettings"));

        // nothing set: the original's values, and keys it does not have are removed
        e = entriesFor(IniFile::Engine, Settings{}, original);
        QCOMPARE(valueIn(e, "r.ScreenPercentage"), std::optional<QString>());
        QCOMPARE(valueIn(e, "ResY"), std::optional<QString>("1080"));

        s = Settings{};
        s.width = 2560;
        s.height = 1440;
        e = entriesFor(IniFile::Engine, s, original);
        QCOMPARE(valueIn(e, "ResX"), std::optional<QString>("2560"));
        QCOMPARE(valueIn(e, "ResY"), std::optional<QString>("1440"));
    }

    void userSettingsAndPlatformKeys()
    {
        Settings s;
        s.renderScale = 200;
        auto e = entriesFor(IniFile::UserSettings, s, QString::fromUtf8(kUserSettingsIni));
        QCOMPARE(valueIn(e, "sg.ResolutionQuality"), std::optional<QString>("100"));   // the engine caps it at 100
        s.renderScale = 67;
        e = entriesFor(IniFile::UserSettings, s, QString::fromUtf8(kUserSettingsIni));
        QCOMPARE(valueIn(e, "sg.ResolutionQuality"), std::optional<QString>("67"));
        QCOMPARE(valueIn(e, "ResolutionSizeX"), std::optional<QString>("1920"));

        s = Settings{};
        s.width = 1920;
        s.height = 1080;
        e = entriesFor(IniFile::Platform, s, QString::fromUtf8(kPlatformIni));
        QCOMPARE(valueIn(e, "r.setres"), std::optional<QString>("1920x1080"));
        e = entriesFor(IniFile::Platform, Settings{}, QString::fromUtf8(kPlatformIni));
        QCOMPARE(valueIn(e, "r.setres"), std::optional<QString>("1280x720"));
    }

    void touchesOnlyWhatIsSet()
    {
        Settings s;
        QVERIFY(!touches(IniFile::Engine, s) && !touches(IniFile::Platform, s) && !touchesJConfig(s));
        s.renderScale = 125;
        QVERIFY(touches(IniFile::Engine, s) && touches(IniFile::UserSettings, s));
        QVERIFY(!touches(IniFile::Platform, s) && !touchesJConfig(s));
        s = Settings{};
        s.jconfigWindow = true;
        s.windowWidth = 1920;
        s.windowHeight = 1080;
        QVERIFY(touchesJConfig(s) && !touches(IniFile::Engine, s));
    }

    void jconfigFile()
    {
        const QByteArray data = jconfigBytes();
        const auto w = readJConfig(data);
        QVERIFY(w.has_value());
        QCOMPARE(*w, (JConfigWindow{16, 16, 1280, 720, 1920, 1080}));
        QVERIFY(!readJConfig(data.left(200)).has_value());

        const JConfigWindow wanted{0, -1080, 1920, 1080, 3840, 2160};
        const QByteArray written = writeJConfig(data, wanted);
        QCOMPARE(written.size(), data.size());
        QCOMPARE(*readJConfig(written), wanted);
        QCOMPARE(written.left(60), data.left(60));     // the other values stay
        QCOMPARE(written.mid(84), data.mid(84));
        QCOMPARE(writeJConfig(data.left(10), wanted), data.left(10));

        Settings s;
        QCOMPARE(jconfigFor(s, *w), *w);
        s.width = 2560;
        s.height = 1440;
        QCOMPARE(jconfigFor(s, *w), (JConfigWindow{16, 16, 1280, 720, 2560, 1440}));
        s.jconfigWindow = true;
        s.windowX = 0;
        s.windowY = -1080;
        s.windowWidth = 1920;
        s.windowHeight = 1080;
        QCOMPARE(jconfigFor(s, *w), (JConfigWindow{0, -1080, 1920, 1080, 2560, 1440}));
    }

    void json()
    {
        Settings s;
        s.enabled = true;
        s.renderScale = 150;
        s.width = 1920;
        s.height = 1080;
        s.jconfigWindow = true;
        s.windowY = -1080;
        s.windowWidth = 1920;
        s.windowHeight = 1080;
        QCOMPARE(settingsFromJson(toJson(s)), s);
        QVERIFY(Settings{}.isEmpty());
        QVERIFY(!s.isEmpty());

        const Settings bad = settingsFromJson(QJsonObject{{"RenderScale", 9999}, {"Width", 1920}, {"Height", 0},
                                                          {"JConfigWindow", true}});
        QCOMPARE(bad.renderScale, kMaxScale);
        QCOMPARE(bad.width, 0);
        QVERIFY(!bad.jconfigWindow);   // no rectangle
        QCOMPARE(describe(s), QString("render scale 150 %, 1920 x 1080, JConfig window 1920 x 1080 at 0,-1080"));
        QCOMPARE(describe(Settings{}), QString("no changes"));
    }

    void writeAndRestoreFiles()
    {
        QTemporaryDir dir;
        const QString exe = makeGame(dir.path());
        const Layout l = layoutFor(exe);

        Status st = status(exe);
        QVERIFY(st.supported() && st.userSettingsIni && st.platformIni && st.jconfig);
        QVERIFY(!st.changed);
        QVERIFY(st.summary.contains("r.ScreenPercentage not set"));
        QVERIFY(st.summary.contains("r.setres 1280x720"));
        QVERIFY(st.summary.contains("window 1280 x 720 at 16,16"));

        Settings s;
        s.enabled = true;
        s.renderScale = 150;
        s.width = 1920;
        s.height = 1080;
        s.jconfigWindow = true;
        s.windowY = -1080;
        s.windowWidth = 1920;
        s.windowHeight = 1080;
        QString message;
        QVERIFY2(apply(exe, s, &message), qPrintable(message));

        const QByteArray engine = readAll(l.engineIni);
        QVERIFY(engine.contains("[SystemSettings]\r\nResX=1920\r\nResY=1080\r\nFullscreen=true\r\n"
                                "r.TextureStreaming=0\r\nr.ScreenPercentage=150\r\nr.Upscale.Quality=3\r\n\r\n"
                                "[/Script/Engine.Engine]"));
        QVERIFY(!engine.contains(QByteArray("\n\n")));   // Windows endings kept
        QVERIFY(readAll(l.platformIni).contains("r.setres=1920x1080\r\n"));
        QCOMPARE(readAll(l.userSettingsIni), kUserSettingsIni);   // 150 % keeps sg.ResolutionQuality at 100
        QCOMPARE(*readJConfig(readAll(l.jconfigFile)), (JConfigWindow{0, -1080, 1920, 1080, 1920, 1080}));
        QCOMPARE(readAll(l.engineIni + kOriginalSuffix), kEngineIni);
        QCOMPARE(readAll(l.jconfigFile + kOriginalSuffix), jconfigBytes());
        QVERIFY(!QFile::exists(l.userSettingsIni + kOriginalSuffix));   // nothing changed there
        QVERIFY(status(exe).changed);

        // the same again changes nothing
        QVERIFY(apply(exe, s, &message));
        QCOMPARE(readAll(l.engineIni), engine);

        // the user edits another key; a setting taken away gets the original back
        QFile f(l.engineIni);
        QVERIFY(f.open(QIODevice::Append));
        f.write("[Mine]\r\nKeep=1\r\n");
        f.close();
        s.renderScale = 0;
        QVERIFY(apply(exe, s, &message));
        QVERIFY(!readAll(l.engineIni).contains("r.ScreenPercentage"));
        QVERIFY(readAll(l.engineIni).contains("Keep=1"));

        // restore: the originals' values back, the copies gone, the user's edit kept
        QVERIFY2(restore(exe, &message), qPrintable(message));
        QCOMPARE(readAll(l.platformIni), kPlatformIni);
        QCOMPARE(readAll(l.jconfigFile), jconfigBytes());
        QVERIFY(readAll(l.engineIni).contains("Keep=1"));
        QVERIFY(!readAll(l.engineIni).contains("r.Upscale.Quality"));
        for (const QString& path : {l.engineIni, l.userSettingsIni, l.platformIni, l.jconfigFile})
            QVERIFY(!QFile::exists(path + kOriginalSuffix));
        QVERIFY(!status(exe).changed);
    }

    void keepsTheEncoding()
    {
        QTemporaryDir dir;
        const QString exe = makeGame(dir.path());
        const Layout l = layoutFor(exe);
        writeAll(l.engineIni, "\xEF\xBB\xBF[SystemSettings]\nResX=1920\n");
        Settings s;
        s.renderScale = 75;
        QString message;
        QVERIFY(apply(exe, s, &message));
        QCOMPARE(readAll(l.engineIni), QByteArray("\xEF\xBB\xBF[SystemSettings]\nResX=1920\nr.ScreenPercentage=75\n"
                                                  "r.Upscale.Quality=3\n"));
        QVERIFY(readAll(l.userSettingsIni).contains("sg.ResolutionQuality=75\r\n"));
    }

    void notAnUnrealGame()
    {
        QTemporaryDir dir;
        const QString exe = dir.filePath("Game/game.exe");
        writeAll(exe, "MZ");
        QVERIFY(!status(exe).supported());
        QString message;
        Settings s;
        s.renderScale = 150;
        QVERIFY(!apply(exe, s, &message));
        QVERIFY(!message.isEmpty());
        QVERIFY(restore(exe, &message));   // nothing to do
    }
};

QTEST_APPLESS_MAIN(TstGameConfig)
#include "tst_gameconfig.moc"
