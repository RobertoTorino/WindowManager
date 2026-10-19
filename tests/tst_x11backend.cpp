#include <QtTest>
#include <QGuiApplication>
#include <QProcess>
#include <QStandardPaths>

#include "platform/PlatformBackend.h"

// Runs the real X11 backend against a throw-away xterm. Skipped without a display or xterm,
// so it is safe on CI machines. What the window manager does with a request is its own business:
// only the outcome is checked, with a short wait because moves are asynchronous.
class TstX11Backend : public QObject
{
    Q_OBJECT

    QProcess m_xterm;
    quintptr m_window{0};
    bool     m_usable{false};

    template <class Pred> bool waitFor(Pred pred, int ms = 3000)
    {
        for (int waited = 0; waited < ms; waited += 50) {
            if (pred())
                return true;
            QTest::qWait(50);
        }
        return pred();
    }

    quintptr findWindowOf(qint64 pid)
    {
        for (const auto& w : platform().topLevelWindows())
            if (w.pid == pid)
                return w.handle;
        return 0;
    }

private slots:
    void initTestCase()
    {
        if (qEnvironmentVariableIsEmpty("DISPLAY"))
            QSKIP("no X display");
        const QString xterm = QStandardPaths::findExecutable("xterm");
        if (xterm.isEmpty())
            QSKIP("xterm is not installed");

        m_xterm.start(xterm, {"-T", "wmtest", "-geometry", "40x10+200+200", "-e", "sleep", "120"});
        QVERIFY(m_xterm.waitForStarted());
        QVERIFY2(waitFor([&] { m_window = findWindowOf(m_xterm.processId()); return m_window != 0; }, 8000),
                 "the xterm window never showed up in the window list");
        m_usable = true;
    }

    void cleanupTestCase()
    {
        m_xterm.kill();
        m_xterm.waitForFinished(2000);
    }

    void describesTheWindow()
    {
        if (!m_usable) QSKIP("no window");
        PlatformBackend& os = platform();
        QVERIFY(os.isValid(m_window));
        QCOMPARE(os.windowTitle(m_window), QString("wmtest"));
        QCOMPARE(os.windowPid(m_window), m_xterm.processId());
        QVERIFY(os.windowState(m_window).visible);

        Fit::Rect r;
        QVERIFY(os.windowRect(m_window, r));
        QVERIFY(r.width() > 100 && r.height() > 50);
    }

    void movesAndResizes()
    {
        if (!m_usable) QSKIP("no window");
        PlatformBackend& os = platform();
        const Fit::Rect wanted = Fit::Rect::fromXYWH(120, 140, 700, 420);
        QVERIFY(os.setBounds(m_window, wanted));

        Fit::Rect now;
        const bool arrived = waitFor([&] {
            return os.windowRect(m_window, now) && now.width() == wanted.width()
                   && now.height() == wanted.height();
        });
        QVERIFY2(arrived, qPrintable(QString("window is at %1,%2 %3x%4").arg(now.left).arg(now.top)
                                         .arg(now.width()).arg(now.height())));
        QCOMPARE(now.left, wanted.left);
        QCOMPARE(now.top, wanted.top);
    }

    void dropsAndRestoresTheFrame()
    {
        if (!m_usable) QSKIP("no window");
        PlatformBackend& os = platform();
        os.stripFrame(m_window);
        QVERIFY(waitFor([&] { return !os.windowState(m_window).hasCaption; }));
        os.restoreFrame(m_window);
        QVERIFY(waitFor([&] { return os.windowState(m_window).hasCaption; }));
    }

    void processInformation()
    {
        if (!m_usable) QSKIP("no window");
        PlatformBackend& os = platform();
        const uint32_t pid = static_cast<uint32_t>(m_xterm.processId());
        QVERIFY(os.processImagePath(pid).endsWith("xterm"));
        QVERIFY(os.processCreationTime(pid) > 0);
        QVERIFY(os.processCreationTime(pid) <= os.now());

        bool listed = false;
        for (const auto& p : os.processes())
            if (p.pid == pid) {
                listed = true;
                QCOMPARE(p.exeName, QString("xterm"));
                QCOMPARE(p.ppid, os.currentPid());
            }
        QVERIFY(listed);
    }

    void monitorsAreReported()
    {
        const auto mons = platform().monitors();
        QVERIFY(!mons.empty());
        QVERIFY(mons.front().full.width() > 0);
        QVERIFY(mons.front().work.width() <= mons.front().full.width());
    }

    void terminatingEndsTheProcess()
    {
        if (!m_usable) QSKIP("no window");
        QVERIFY(platform().terminateProcess(static_cast<uint32_t>(m_xterm.processId())));
        QVERIFY(m_xterm.waitForFinished(3000));
        QVERIFY(waitFor([&] { return !platform().isValid(m_window); }));
    }
};

int main(int argc, char** argv)
{
    qputenv("QT_QPA_PLATFORM", "xcb");
    QGuiApplication app(argc, argv);
    TstX11Backend test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_x11backend.moc"
