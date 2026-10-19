#include <QtTest>
#include <QImage>

#include "config/AppConfig.h"
#include "platform/PlatformBackend.h"
#include "window/MonitorHelper.h"
#include "window/WindowManager.h"

namespace {

// In-memory desktop: a few windows, monitors and processes, and a log of what was asked of it.
class FakeBackend final : public PlatformBackend
{
public:
    struct Win
    {
        quintptr  handle{0};
        qint64    pid{0};
        QString   title;
        QString   cls;
        Fit::Rect rect;
        bool      visible{true};
        bool      framed{true};
        bool      topmost{false};
    };

    std::vector<Win>          wins;
    std::vector<Monitor>      mons;
    std::vector<ProcessEntry> procs;
    QHash<qint64, QString>    imagePaths;
    std::vector<uint32_t>     terminated;
    std::vector<ShowState>    shows;

    Win* find(quintptr h)
    {
        for (auto& w : wins)
            if (w.handle == h) return &w;
        return nullptr;
    }
    const Win* find(quintptr h) const { return const_cast<FakeBackend*>(this)->find(h); }

    std::vector<TopLevelWindow> topLevelWindows() const override
    {
        std::vector<TopLevelWindow> out;
        for (const auto& w : wins)
            out.push_back({w.handle, w.pid, w.rect});
        return out;
    }
    bool isValid(quintptr h) const override { return find(h) != nullptr; }
    WindowState windowState(quintptr h) const override
    {
        WindowState s;
        if (const Win* w = find(h)) {
            s.valid = true;
            s.visible = w->visible;
            s.hasCaption = s.hasFrame = w->framed;
            s.topmost = w->topmost;
        }
        return s;
    }
    QString windowTitle(quintptr h) const override { const Win* w = find(h); return w ? w->title : QString(); }
    QString windowClass(quintptr h) const override { const Win* w = find(h); return w ? w->cls : QString(); }
    qint64  windowPid(quintptr h) const override { const Win* w = find(h); return w ? w->pid : 0; }
    bool windowRect(quintptr h, Fit::Rect& out) const override
    {
        const Win* w = find(h);
        if (!w) return false;
        out = w->rect;
        return true;
    }
    bool clientRect(quintptr h, Fit::Rect& out) const override { return windowRect(h, out); }
    std::optional<Fit::Rect> visibleFrame(quintptr) const override { return std::nullopt; }
    bool setBounds(quintptr h, const Fit::Rect& r) override
    {
        Win* w = find(h);
        if (!w) return false;
        w->rect = r;
        return true;
    }
    void stripFrame(quintptr h) override { if (Win* w = find(h)) w->framed = false; }
    void restoreFrame(quintptr h) override { if (Win* w = find(h)) w->framed = true; }
    void resetWindow(quintptr h) override { restoreFrame(h); }
    void show(quintptr, ShowState s) override { shows.push_back(s); }
    void bringToTop(quintptr) override {}
    void focus(quintptr) override {}
    void requestClose(quintptr) override {}
    bool supportsFlag(WindowFlag f) const override { return f == WindowFlag::Topmost; }
    bool toggleFlag(quintptr h, WindowFlag f) override
    {
        Win* w = find(h);
        if (!w || f != WindowFlag::Topmost) return false;
        w->topmost = !w->topmost;
        return w->topmost;
    }

    std::vector<Monitor> monitors() const override { return mons; }
    std::optional<Monitor> nearestMonitor(quintptr h) const override
    {
        const Win* w = find(h);
        if (!w) return std::nullopt;
        const int cx = w->rect.left + w->rect.width() / 2;
        for (const auto& m : mons)
            if (cx >= m.full.left && cx < m.full.right) return m;
        return mons.empty() ? std::nullopt : std::optional<Monitor>(mons.front());
    }

    std::vector<ProcessEntry> processes() const override { return procs; }
    QString processImagePath(qint64 pid) const override { return imagePaths.value(pid); }
    uint64_t processCreationTime(uint32_t) const override { return 100; }
    uint64_t now() const override { return 50; }
    uint32_t currentPid() const override { return 1; }
    bool terminateProcess(uint32_t pid) override { terminated.push_back(pid); return true; }
    bool startProcess(const QString&, const QString&, qint64* pid) override
    {
        if (pid) *pid = 4242;
        return true;
    }
    QImage captureScreen(const Fit::Rect&) const override { return {}; }
};

const QString kGame = "C:/games/game.exe";

} // namespace

class TstWindowManager : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    FakeBackend   m_fake;

    // Two monitors side by side; the game window sits on the first.
    void resetDesktop()
    {
        m_fake = FakeBackend{};
        m_fake.mons = {
            {Fit::Rect::fromXYWH(0, 0, 1920, 1080),    Fit::Rect::fromXYWH(0, 0, 1920, 1040)},
            {Fit::Rect::fromXYWH(1920, 0, 1280, 1024), Fit::Rect::fromXYWH(1920, 0, 1280, 1024)},
        };
        m_fake.wins = {
            {10, 100, "Notepad", "Notepad", Fit::Rect::fromXYWH(50, 50, 400, 300), true, true, false},
            {20, 200, "The Game", "GameWnd", Fit::Rect::fromXYWH(100, 100, 800, 600), true, true, false},
            {21, 200, "", "Helper", Fit::Rect::fromXYWH(0, 0, 64, 64), false, true, false},
        };
        m_fake.procs = {{1, 0, "wm.exe"}, {100, 1, "notepad.exe"}, {200, 1, "game.exe"}, {300, 200, "child.exe"}};
        m_fake.imagePaths = {{100, "C:/Windows/notepad.exe"}, {200, kGame}, {300, "C:/games/child.exe"}};
        setPlatformBackendForTesting(&m_fake);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        AppConfig::instance().setConfigPathOverride(m_dir.filePath("cfg.json"));
    }

    void init() { resetDesktop(); }
    void cleanup() { setPlatformBackendForTesting(nullptr); }

    void scanFindsOnlyTargetWindowsLargestVisibleFirst()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());

        const auto& found = wm.scannedWindows();
        QCOMPARE(found.size(), size_t(2));
        QCOMPARE(found[0].hwnd, quintptr(20));
        QCOMPARE(found[0].title, QString("The Game"));
        QCOMPARE(found[1].title, QString("<untitled>"));   // hidden helper sorts last
        QCOMPARE(wm.activeWindow(), quintptr(20));
    }

    void scanReportsNoMatch()
    {
        WindowManager wm;
        wm.setTargetExecutable("C:/games/other.exe");
        QVERIFY(!wm.scanWindowsForTarget());
        QCOMPARE(wm.activeWindow(), quintptr(0));
    }

    void borderlessFullscreenFillsTheMonitorAndDropsTheFrame()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(wm.setBorderlessFullscreen());

        const auto* w = m_fake.find(20);
        QCOMPARE(w->rect, m_fake.mons[0].full);
        QVERIFY(!w->framed);
    }

    void moveToMonitorCentresOnTheWorkArea()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(wm.moveToMonitor(2));

        // 800x600 centred in 1920,0 1280x1024
        QCOMPARE(m_fake.find(20)->rect, Fit::Rect::fromXYWH(1920 + 240, 212, 800, 600));
        QCOMPARE(wm.activeWindowMonitorIndex(), 2);
    }

    void moveToMissingMonitorFails()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(!wm.moveToMonitor(3));
        QVERIFY(wm.lastMessage().contains("not available"));
    }

    void toggleTopmostReportsTheNewState()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(wm.toggleTopmost());
        QCOMPARE(wm.lastMessage(), QString("Topmost set."));
        QVERIFY(wm.toggleTopmost());
        QCOMPARE(wm.lastMessage(), QString("Topmost removed."));
    }

    void unsupportedFlagIsRefusedWithAMessage()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(!wm.toggleLayered());
        QVERIFY(wm.lastMessage().contains("not supported"));
    }

    void killTerminatesTheGameAndItsChildOnly()
    {
        WindowManager wm;
        wm.setTargetExecutable(kGame);
        QVERIFY(wm.scanWindowsForTarget());
        QVERIFY(wm.killTarget());

        auto& t = m_fake.terminated;
        QVERIFY(std::find(t.begin(), t.end(), 200u) != t.end());
        QVERIFY(std::find(t.begin(), t.end(), 100u) == t.end());   // notepad untouched
        QVERIFY(std::find(t.begin(), t.end(), 1u) == t.end());     // never ourselves
    }

    void monitorHelperReadsTheBackend()
    {
        QCOMPARE(MonitorHelper::getMonitorCount(), 2);
        QCOMPARE(MonitorHelper::getMonitorIndexFromPoint(2000, 10), 2);
        QCOMPARE(MonitorHelper::getMonitorIndexFromWindow(20), 1);

        const auto work = MonitorHelper::getMonitorWorkArea(1);
        QVERIFY(work.has_value());
        QCOMPARE(work->h, 1040);
        QVERIFY(!MonitorHelper::getMonitorWorkArea(0).has_value());
        QVERIFY(!MonitorHelper::getMonitorWorkArea(3).has_value());
    }
};

// Tests always install the fake; reaching the real backend would touch the user's desktop.
PlatformBackend& nativePlatformBackend()
{
    qFatal("nativePlatformBackend() called in a test");
}

QTEST_MAIN(TstWindowManager)
#include "tst_windowmanager.moc"
