#include "MagpieSession.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "magpie/MagpieProcess.h"
#include "magpie/MagpieRelease.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace {

constexpr int kTickMs = 500;
constexpr int kWindowWaitMs = 180000;   // launchers (Steam, JConfig) can take a while
constexpr int kSettleTicks = 3;         // the same window for 1.5 s
constexpr int kStopWaitMs = 15000;

void logInfo(const QString& text) { Logger::instance().info(text.toStdString(), "Magpie"); }
void logWarn(const QString& text) { Logger::instance().warn(text.toStdString(), "Magpie"); }

bool oursRunning()
{
    for (const auto& m : Magpie::runningMagpies()) {
        if (Magpie::isFrom(m, Magpie::installDir()))
            return true;
    }
    return false;
}

QString otherMagpie()
{
    for (const auto& m : Magpie::runningMagpies()) {
        if (!Magpie::isFrom(m, Magpie::installDir()))
            return m.path.isEmpty() ? QString("pid %1").arg(m.pid) : QDir::toNativeSeparators(m.path);
    }
    return {};
}

} // namespace

MagpieSession::MagpieSession(WindowManagerController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &MagpieSession::tick);
}

bool MagpieSession::handles(const QString& exePath) const
{
    return m_state != State::Idle
           && QDir::cleanPath(exePath).compare(QDir::cleanPath(m_exe), Qt::CaseInsensitive) == 0;
}

void MagpieSession::gameStarting(const QString& exePath)
{
    const auto game = AppConfig::instance().gameByPath(exePath);
    if (m_state == State::Running && !handles(exePath))
        stopMagpie("another game is starting");
    if (!game || !game->magpie.enabled()) {
        m_timer->stop();
        m_state = State::Idle;
        return;
    }
    m_exe = exePath;
    m_name = game->savedName.isEmpty() ? QFileInfo(exePath).completeBaseName() : game->savedName;
    m_settings = game->magpie;
    if (!Magpie::installedVersion(Magpie::installDir())) {
        end(m_name + ": Magpie is not set up (Magpie tab), so the game is not scaled.", true);
        return;
    }
    logInfo(QString("%1 starts with Magpie (%2%3): waiting for its window")
                .arg(m_name, m_settings.mode, m_settings.game3D ? ", 3D game mode" : ""));
    m_state = State::WaitWindow;
    m_clock.start();
    m_lastHwnd = 0;
    m_stableTicks = 0;
    m_profileClass.clear();
    m_sizeWarned = false;
    m_timer->start();
}

void MagpieSession::tick()
{
    switch (m_state) {
    case State::Idle:
        m_timer->stop();
        return;

    case State::WaitWindow: {
        if (m_clock.elapsed() > kWindowWaitMs) {
            end(m_name + ": its window did not appear, so Magpie was not started.", true);
            return;
        }
        if (!handles(m_controller->targetExecutable()))
            return;   // another game is selected for the moment; its window is not ours
        const quintptr hwnd = m_controller->activeWindow();
        const Magpie::WindowFacts f = Magpie::windowFacts(hwnd);
        if (!hwnd || !f.visible || f.className.isEmpty() || f.exePath.isEmpty()) {
            m_lastHwnd = 0;
            m_stableTicks = 0;
            return;
        }
        m_stableTicks = hwnd == m_lastHwnd ? m_stableTicks + 1 : 0;
        m_lastHwnd = hwnd;
        if (m_stableTicks < kSettleTicks)
            return;
        m_hwnd = hwnd;
        m_pid = f.pid;
        if (const QString other = otherMagpie(); !other.isEmpty()) {
            end(QString("%1: another Magpie is running (%2), so this app's Magpie cannot start. Close it to "
                        "let the app scale the game.").arg(m_name, other), true);
            return;
        }
        if (oursRunning()) {   // it read its config at start-up: restart it with the game's profile
            logInfo("Magpie is running: closing it so it reads the game's profile");
            Magpie::postQuit();
            m_state = State::StopOld;
            m_clock.start();
            return;
        }
        arrange();
        return;
    }

    case State::StopOld:
        if (oursRunning()) {
            if (m_clock.elapsed() > kStopWaitMs)
                end(m_name + ": Magpie did not close, so the game's profile could not be written.", true);
            return;
        }
        arrange();
        return;

    case State::Running:
        if (!Magpie::processAlive(m_pid)) {
            stopMagpie("the game has ended");
            m_timer->stop();
            m_state = State::Idle;
            return;
        }
        followWindow();
        return;
    }
}

void MagpieSession::followWindow()
{
    if (!handles(m_controller->targetExecutable()))
        return;
    const quintptr hwnd = m_controller->activeWindow();
    const Magpie::WindowFacts f = Magpie::windowFacts(hwnd);
    if (!hwnd || hwnd == m_hwnd || !f.visible || f.pid != m_pid || f.className.isEmpty()
        || f.className == m_profileClass) {
        m_lastHwnd = 0;
        m_stableTicks = 0;
        if (hwnd == m_hwnd)
            checkSize(hwnd);   // the game may resize its window after the first frames
        return;
    }
    m_stableTicks = hwnd == m_lastHwnd ? m_stableTicks + 1 : 0;
    m_lastHwnd = hwnd;
    if (m_stableTicks < kSettleTicks)
        return;
    logInfo(QString("%1 now shows window class %2 (the profile has %3): writing the profile again")
                .arg(m_name, f.className, m_profileClass));
    m_hwnd = hwnd;
    if (oursRunning()) {
        Magpie::postQuit();
        m_state = State::StopOld;
        m_clock.start();
        return;
    }
    arrange();
}

void MagpieSession::checkSize(quintptr hwnd)
{
    if (m_sizeWarned)
        return;
    const Magpie::WindowFacts f = Magpie::windowFacts(hwnd);
    if (!f.largerThanMonitor())
        return;
    m_sizeWarned = true;
    const QString text =
        QString("%1: its window (%2 x %3) is bigger than its monitor (%4 x %5). Magpie scales up: it shrinks "
                "a bigger window to fit, which looks squeezed. Give the game a resolution below the monitor's "
                "(for example 1280 x 720 on the Game config tab; with JConfig, its DX resolution as well).")
            .arg(m_name)
            .arg(f.clientWidth).arg(f.clientHeight).arg(f.monitorWidth).arg(f.monitorHeight);
    logWarn(text);
    emit message(text);
}

void MagpieSession::arrange()
{
    const Magpie::WindowFacts f = Magpie::windowFacts(m_hwnd);
    if (f.className.isEmpty() || f.exePath.isEmpty()) {
        m_state = State::WaitWindow;   // the window went away meanwhile (a launcher's): wait for the next
        m_clock.start();
        return;
    }

    const QString dir = Magpie::installDir();
    QFile file(Magpie::configPath(dir));
    const QByteArray config = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    file.close();
    Magpie::GameProfile profile;
    profile.name = m_name;
    profile.exePath = f.exePath;
    profile.windowClass = f.className;
    profile.scalingMode = m_settings.mode;
    profile.game3D = m_settings.game3D;
    const Magpie::EditResult r = Magpie::setGameProfile(config, profile);
    if (!r.ok) {
        end(m_name + ": Magpie's profile was not written: " + r.error, true);
        return;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(r.config) != r.config.size()) {
        end(m_name + ": could not write " + file.fileName(), true);
        return;
    }
    file.close();
    m_profileClass = f.className;
    logInfo(QString("Profile for %1: %2, window class %3, %4%5")
                .arg(m_name, QDir::toNativeSeparators(f.exePath), f.className, m_settings.mode,
                     m_settings.game3D ? ", 3D game mode (the mouse stays in the game until Magpie closes)" : ""));

    // Magpie restarts the scaling every time the window is moved or resized: the stabilizer re-asserting
    // the position would keep doing that.
    if (m_controller->stabilizerEnabled()) {
        m_controller->toggleStabilizer();
        logInfo("Stability turned off: it would move the window Magpie scales");
    }

    if (!Magpie::startInTray(dir)) {
        end(m_name + ": could not start " + QDir::toNativeSeparators(Magpie::exePath(dir)), true);
        return;
    }
    m_state = State::Running;
    QString text = QString("%1: Magpie scales it with %2 while it is in front.").arg(m_name, m_settings.mode);
    if (f.maximized || f.fullScreen) {
        text += " The game is maximized or borderless full screen, which Magpie does not scale: set it to a "
                "window at the size it should render.";
        logWarn(text);
    } else {
        logInfo(text);
    }
    emit message(text);
    checkSize(m_hwnd);
}

void MagpieSession::stopMagpie(const QString& why)
{
    if (oursRunning()) {
        Magpie::postQuit();
        logInfo("Magpie closed: " + why);
    }
}

void MagpieSession::end(const QString& text, bool warn)
{
    m_timer->stop();
    m_state = State::Idle;
    if (warn)
        logWarn(text);
    else
        logInfo(text);
    emit message(text);
}
