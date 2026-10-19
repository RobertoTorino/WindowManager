#include "WindowManagerController.h"

#include "config/AppConfig.h"
#include "core/Logger.h"

#include <QFileInfo>

#include <Windows.h>

namespace {

// Routes the logger to the game's own file (named after its saved name).
void selectLogGame(const QString& exePath)
{
    if (exePath.isEmpty()) {
        Logger::instance().setGame({});
        return;
    }
    QString name = QFileInfo(exePath).completeBaseName();
    if (const auto game = AppConfig::instance().gameByPath(exePath);
        game && !game->savedName.isEmpty())
        name = game->savedName;
    Logger::instance().setGame(name.toStdString());
}

} // namespace

WindowManagerController::WindowManagerController(QObject *parent)
    : QObject(parent)
    , m_statusText("Select an executable to begin window management.")
{
    m_clock.start();
    AppConfig::instance().load();
    const QString lastExe = AppConfig::instance().currentExePath();
    if (!lastExe.isEmpty()) {
        selectLogGame(lastExe);
        Logger::instance().info(
            "Restoring last executable: " + lastExe.toStdString(),
            "WindowManagerController");
        m_windowManager.setTargetExecutable(lastExe);
        m_statusText = "Ready. Loaded last selected executable.";
    } else {
        Logger::instance().info("No previous executable in config", "WindowManagerController");
    }
}

QString WindowManagerController::statusText() const
{
    return m_statusText;
}

QString WindowManagerController::targetExecutable() const
{
    return m_windowManager.targetExecutable();
}

bool WindowManagerController::stabilizerEnabled() const
{
    return m_stabilizer.isRunning();
}

quintptr WindowManagerController::activeWindow() const
{
    return m_windowManager.activeWindow();
}

WindowManagerController::StyleState WindowManagerController::queryActiveWindowStyleState() const
{
    StyleState s;
    const quintptr hwnd = m_windowManager.activeWindow();
    if (!hwnd) return s;
    const HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h)) return s;

    const LONG_PTR ex = GetWindowLongPtr(h, GWL_EXSTYLE);
    s.topmost    = (ex & WS_EX_TOPMOST) != 0;
    s.toolWindow = (ex & WS_EX_TOOLWINDOW) != 0;
    s.layered    = (ex & WS_EX_LAYERED) != 0;
    s.noActivate = (ex & WS_EX_NOACTIVATE) != 0;
    return s;
}

WindowManagerController::VisibilityState WindowManagerController::queryActiveWindowVisibilityState() const
{
    VisibilityState v;
    const quintptr hwnd = m_windowManager.activeWindow();
    if (!hwnd) return v;
    const HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h)) return v;

    v.visible    = IsWindowVisible(h) != 0;
    v.minimized  = IsIconic(h) != 0;
    v.maximized  = IsZoomed(h) != 0;
    const LONG_PTR style = GetWindowLongPtr(h, GWL_STYLE);
    v.windowed   = (style & WS_CAPTION) != 0;
    return v;
}

const std::vector<WindowManager::WindowInfo>& WindowManagerController::scannedWindows() const
{
    return m_windowManager.scannedWindows();
}

void WindowManagerController::setTargetExecutable(const QString& exePath)
{
    selectLogGame(exePath);
    Logger::instance().info(
        "setTargetExecutable: " + exePath.toStdString(), "WindowManagerController");
    if (exePath != m_windowManager.targetExecutable())
        disarmFitCheck();
    m_windowManager.setTargetExecutable(exePath);
    // Auto-scan so the table populates immediately if the game is already running
    const bool found = m_windowManager.scanWindowsForTarget();
    if (found) syncStabilizerTarget();
    publishStatus("Target executable selected");
}

void WindowManagerController::scanWindows()
{
    finish(m_windowManager.scanWindowsForTarget(), "Scan complete", "Scan failed");
}

void WindowManagerController::applySavedSettings()
{
    finish(m_windowManager.applySavedProfile(), "Applied saved settings", "Apply failed");
}

void WindowManagerController::saveCurrentPosition()
{
    finish(m_windowManager.saveCurrentPosition(), "Saved current position", "Save failed");
}

void WindowManagerController::moveToMonitor1()
{
    finish(m_windowManager.moveToMonitor(1), "Moved to monitor 1", "Move failed");
}

void WindowManagerController::moveToMonitor2()
{
    finish(m_windowManager.moveToMonitor(2), "Moved to monitor 2", "Move failed");
}

void WindowManagerController::toggleStabilizer()
{
    if (m_stabilizer.isRunning()) {
        m_stabilizer.stop();
        m_stabilizer.clearTrackedWindow();
        publishStatus("Stabilizer disabled");
        return;
    }

    m_stabilizer.start();
    syncStabilizerTarget();
    publishStatus("Stabilizer enabled");
}

void WindowManagerController::nudgeLeft(int step)
{
    finish(m_windowManager.nudge(-step, 0), "Nudged left", "Nudge failed");
}

void WindowManagerController::nudgeRight(int step)
{
    finish(m_windowManager.nudge(step, 0), "Nudged right", "Nudge failed");
}

void WindowManagerController::nudgeUp(int step)
{
    finish(m_windowManager.nudge(0, -step), "Nudged up", "Nudge failed");
}

void WindowManagerController::nudgeDown(int step)
{
    finish(m_windowManager.nudge(0, step), "Nudged down", "Nudge failed");
}

void WindowManagerController::growWidth(int step)
{
    finish(m_windowManager.nudge(0, 0, step, 0, true), "Width increased", "Resize failed");
}

void WindowManagerController::shrinkWidth(int step)
{
    finish(m_windowManager.nudge(0, 0, -step, 0, true), "Width decreased", "Resize failed");
}

void WindowManagerController::growHeight(int step)
{
    finish(m_windowManager.nudge(0, 0, 0, step, true), "Height increased", "Resize failed");
}

void WindowManagerController::shrinkHeight(int step)
{
    finish(m_windowManager.nudge(0, 0, 0, -step, true), "Height decreased", "Resize failed");
}

void WindowManagerController::setCustomSize(int width, int height)
{
    finish(m_windowManager.setCustomSize(width, height), "Custom size applied", "Set size failed");
}

void WindowManagerController::applyHorizontalOverscan(int pixels)
{
    finish(m_windowManager.applyHorizontalOverscan(pixels), "Horizontal overscan applied", "Overscan failed");
}

void WindowManagerController::applyVerticalOverscan(int pixels)
{
    finish(m_windowManager.applyVerticalOverscan(pixels), "Vertical overscan applied", "Overscan failed");
}

void WindowManagerController::setActiveWindow(quintptr hwnd)
{
    m_windowManager.setActiveWindow(hwnd);
    syncStabilizerTarget();
    publishStatus("Active window changed");
}

void WindowManagerController::syncStabilizerTarget()
{
    if (!m_stabilizer.isRunning()) return;
    const quintptr hwnd = m_windowManager.activeWindow();
    int x = 0, y = 0, w = 0, h = 0;
    if (hwnd && m_windowManager.activeWindowRect(x, y, w, h))
        m_stabilizer.trackWindow(hwnd, x, y, w, h);
    else
        m_stabilizer.clearTrackedWindow();
}

void WindowManagerController::finish(bool ok, const char* okMsg, const char* failMsg)
{
    if (ok) syncStabilizerTarget();
    publishStatus(ok ? okMsg : failMsg);
}

void WindowManagerController::publishStatus(const QString& prefix)
{
    const QString detail = m_windowManager.lastMessage();
    m_statusText = prefix.isEmpty() ? detail : prefix + ": " + detail;

    // Every user action ends up here, so this is what gives each game's log its history.
    const bool failed = m_statusText.contains("fail", Qt::CaseInsensitive);
    auto& log = Logger::instance();
    if (failed)
        log.warn(m_statusText.toStdString(), "Action");
    else
        log.info(m_statusText.toStdString(), "Action");
    emit statusChanged();
}

void WindowManagerController::destroyWindow()
{ publishStatus(m_windowManager.destroyWindow() ? "Window closed" : "Close failed"); }

void WindowManagerController::hideWindow()
{ publishStatus(m_windowManager.hideWindow() ? "Window hidden" : "Hide failed"); }

void WindowManagerController::showWindow()
{ publishStatus(m_windowManager.showWindow() ? "Window shown" : "Show failed"); }

void WindowManagerController::minimizeWindow()
{ publishStatus(m_windowManager.minimizeWindow() ? "Window minimized" : "Minimize failed"); }

void WindowManagerController::maximizeWindow()
{ publishStatus(m_windowManager.maximizeWindow() ? "Window maximized" : "Maximize failed"); }

void WindowManagerController::restoreWindow()
{ publishStatus(m_windowManager.restoreWindow() ? "Window restored" : "Restore failed"); }

void WindowManagerController::setWindowed()
{
    finish(m_windowManager.setWindowed(), "Windowed mode set", "Windowed failed");
}

void WindowManagerController::setBorderlessFullscreen()
{
    finish(m_windowManager.setBorderlessFullscreen(), "True borderless fullscreen set", "Borderless failed");
}

void WindowManagerController::fitToScreen()
{
    finish(m_windowManager.fitToScreen(), "Fit to screen", "Fit failed");
}

void WindowManagerController::toggleTopmost()
{ publishStatus(m_windowManager.toggleTopmost() ? "Topmost toggled" : "Toggle failed"); }

void WindowManagerController::toggleToolWindow()
{ publishStatus(m_windowManager.toggleToolWindow() ? "Tool window toggled" : "Toggle failed"); }

void WindowManagerController::toggleLayered()
{ publishStatus(m_windowManager.toggleLayered() ? "Layered toggled" : "Toggle failed"); }

void WindowManagerController::toggleNoActivate()
{ publishStatus(m_windowManager.toggleNoActivate() ? "No-activate toggled" : "Toggle failed"); }

void WindowManagerController::resetAll()
{
    finish(m_windowManager.resetAll(), "Window reset to defaults", "Reset failed");
}

void WindowManagerController::launchTarget()
{
    const bool isBatch =
        m_windowManager.targetExecutable().endsWith(".bat", Qt::CaseInsensitive) ||
        m_windowManager.targetExecutable().endsWith(".cmd", Qt::CaseInsensitive);

    if (isBatch)
        m_windowManager.snapshotProcesses();

    const bool ok = m_windowManager.launchTarget();
    publishStatus(ok ? "Launched" : "Launch failed: " + m_windowManager.lastMessage());

    if (ok)
        armFitCheck();

    if (ok && isBatch) {
        m_batchPollCount = 0;
        if (!m_batchPollTimer) {
            m_batchPollTimer = new QTimer(this);
            m_batchPollTimer->setInterval(1000);
            connect(m_batchPollTimer, &QTimer::timeout,
                    this, &WindowManagerController::onBatchPollTick);
        }
        m_batchPollTimer->start();
        publishStatus("Batch launched - scanning for game process...");
    }
}

void WindowManagerController::onBatchPollTick()
{
    ++m_batchPollCount;
    if (m_batchPollCount > k_batchPollMaxAttempts) {
        m_batchPollTimer->stop();
        Logger::instance().warn(
            "Batch scan timed out after " + std::to_string(k_batchPollMaxAttempts) + " seconds",
            "WindowManagerController");
        publishStatus("Batch scan timed out: game process not detected");
        return;
    }

    const bool found = m_windowManager.pollForBatchChild();
    if (found) {
        m_batchPollTimer->stop();
        syncStabilizerTarget();
        publishStatus("Game process detected");
    }
}

void WindowManagerController::focusTarget()
{ publishStatus(m_windowManager.focusTarget() ? "Focus restored" : "Focus failed"); }

int WindowManagerController::activeWindowMonitorIndex() const
{ return m_windowManager.activeWindowMonitorIndex(); }

void WindowManagerController::killTarget()
{
    Logger::instance().info("killTarget requested", "WindowManagerController");
    disarmFitCheck();
    m_stabilizer.stop();
    m_stabilizer.clearTrackedWindow();
    publishStatus(m_windowManager.killTarget() ? "Processes killed" : "Kill failed (nothing found)");
}

void WindowManagerController::borderlessTM1080p()
{ publishStatus(m_windowManager.setBorderlessSize(1, 1920, 1080) ? "Borderless 1080P (Mon 1)" : m_windowManager.lastMessage()); }

void WindowManagerController::borderlessL1080p()
{ publishStatus(m_windowManager.setBorderlessSize(2, 1920, 1080) ? "Borderless 1080P (Mon 2)" : m_windowManager.lastMessage()); }

void WindowManagerController::fakeFS1080p()
{ publishStatus(m_windowManager.setFakeFullscreen(1920, 1080) ? "Fake FS 1080P" : m_windowManager.lastMessage()); }

void WindowManagerController::fakeFSA1080p()
{ publishStatus(m_windowManager.setFakeFullscreenAspect(1920, 1080) ? "Fake FSA 1080P" : m_windowManager.lastMessage()); }

void WindowManagerController::borderlessTM1440p()
{ publishStatus(m_windowManager.setBorderlessSize(1, 2560, 1440) ? "Borderless 1440P (Mon 1)" : m_windowManager.lastMessage()); }

void WindowManagerController::borderlessL1440p()
{ publishStatus(m_windowManager.setBorderlessSize(2, 2560, 1440) ? "Borderless 1440P (Mon 2)" : m_windowManager.lastMessage()); }

void WindowManagerController::fakeFS1440p()
{ publishStatus(m_windowManager.setFakeFullscreen(2560, 1440) ? "Fake FS 1440P" : m_windowManager.lastMessage()); }

void WindowManagerController::fakeFSA1440p()
{ publishStatus(m_windowManager.setFakeFullscreenAspect(2560, 1440) ? "Fake FSA 1440P" : m_windowManager.lastMessage()); }

// ---------------------------------------------------------------------------
// Fit to monitor
// ---------------------------------------------------------------------------

void WindowManagerController::setStatus(const QString& text)
{
    m_statusText = text;
    emit statusChanged();
}

void WindowManagerController::setFitCheckEnabled(bool enabled)
{
    m_fitCheckEnabled = enabled;
    if (!enabled)
        disarmFitCheck();
}

void WindowManagerController::armFitCheck()
{
    if (!m_fitCheckEnabled)
        return;

    if (!m_fitTimer) {
        m_fitTimer = new QTimer(this);
        m_fitTimer->setInterval(500);
        connect(m_fitTimer, &QTimer::timeout, this, &WindowManagerController::onFitTick);
    }
    m_fitTrigger.arm(m_clock.elapsed());
    m_fitTimer->start();
    setStatus("Waiting for the game window to settle, then the fit check will open...");
}

void WindowManagerController::disarmFitCheck()
{
    m_fitTrigger.disarm();
    if (m_fitTimer)
        m_fitTimer->stop();
}

void WindowManagerController::onFitTick()
{
    const auto w = m_windowManager.findPrimaryWindow();
    switch (m_fitTrigger.update(m_clock.elapsed(), w.hwnd, w.ready, w.rect)) {
    case Fit::Trigger::Result::Fire:
        m_fitTimer->stop();
        m_windowManager.scanWindowsForTarget();   // refresh the table
        m_windowManager.setActiveWindow(w.hwnd);  // and act on the window we measured
        syncStabilizerTarget();
        // Nothing to correct with the dialog's default options: don't interrupt the user.
        if (WindowManager::FitMeasurement m;
            m_windowManager.measureFit(WindowManager::FitRequest{}, m) && !m.plan.needsChange()) {
            Logger::instance().info("Fit check: window already fits the monitor, dialog skipped",
                                    "WindowManagerController");
            setStatus("Game window already fits the monitor exactly.");
            break;
        }
        Logger::instance().info("Fit check: game window settled, proposing fit", "WindowManagerController");
        setStatus("Game window ready: review the proposed fit.");
        emit fitProposed();
        break;
    case Fit::Trigger::Result::TimedOut:
        m_fitTimer->stop();
        Logger::instance().warn("Fit check timed out: no game window appeared", "WindowManagerController");
        setStatus("Fit check cancelled: no game window appeared in time (use Fit to monitor... later).");
        break;
    case Fit::Trigger::Result::Idle:
        m_fitTimer->stop();
        break;
    case Fit::Trigger::Result::Waiting:
        break;
    }
}

void WindowManagerController::requestFitCheck()
{
    if (!m_windowManager.activeWindow() && !m_windowManager.scanWindowsForTarget()) {
        setStatus("Fit check: no game window found. Start the game and press Scan.");
        return;
    }
    emit fitProposed();
}

bool WindowManagerController::measureFit(const WindowManager::FitRequest& request,
                                         WindowManager::FitMeasurement& out)
{
    return m_windowManager.measureFit(request, out);
}

void WindowManagerController::applyFit(const WindowManager::FitRequest& request)
{
    const bool ok = m_windowManager.applyFit(request);
    if (m_windowManager.activeWindow())
        syncStabilizerTarget();
    // lastMessage already says whether the fit is exact, or what is still off
    setStatus(m_windowManager.lastMessage());
    Logger::instance().info(std::string("applyFit ") + (ok ? "exact" : "not exact"), "WindowManagerController");
}
