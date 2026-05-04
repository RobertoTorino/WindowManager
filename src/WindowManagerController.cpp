#include "WindowManagerController.h"

#include "config/AppConfig.h"

WindowManagerController::WindowManagerController(QObject *parent)
    : QObject(parent)
    , m_statusText("Select an executable to begin window management.")
{
    AppConfig::instance().load();
    const QString lastExe = AppConfig::instance().currentExePath();
    if (!lastExe.isEmpty()) {
        m_windowManager.setTargetExecutable(lastExe);
        m_statusText = "Ready. Loaded last selected executable.";
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

const std::vector<WindowManager::WindowInfo>& WindowManagerController::scannedWindows() const
{
    return m_windowManager.scannedWindows();
}

void WindowManagerController::setTargetExecutable(const QString& exePath)
{
    m_windowManager.setTargetExecutable(exePath);
    publishStatus("Target executable selected");
}

void WindowManagerController::scanWindows()
{
    const bool ok = m_windowManager.scanWindowsForTarget();
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Scan complete" : "Scan failed");
}

void WindowManagerController::applySavedSettings()
{
    const bool ok = m_windowManager.applySavedProfile();
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Applied saved settings" : "Apply failed");
}

void WindowManagerController::saveCurrentPosition()
{
    const bool ok = m_windowManager.saveCurrentPosition();
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Saved current position" : "Save failed");
}

void WindowManagerController::moveToMonitor1()
{
    const bool ok = m_windowManager.moveToMonitor(1);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Moved to monitor 1" : "Move failed");
}

void WindowManagerController::moveToMonitor2()
{
    const bool ok = m_windowManager.moveToMonitor(2);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Moved to monitor 2" : "Move failed");
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
    const bool ok = m_windowManager.nudge(-step, 0);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Nudged left" : "Nudge failed");
}

void WindowManagerController::nudgeRight(int step)
{
    const bool ok = m_windowManager.nudge(step, 0);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Nudged right" : "Nudge failed");
}

void WindowManagerController::nudgeUp(int step)
{
    const bool ok = m_windowManager.nudge(0, -step);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Nudged up" : "Nudge failed");
}

void WindowManagerController::nudgeDown(int step)
{
    const bool ok = m_windowManager.nudge(0, step);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Nudged down" : "Nudge failed");
}

void WindowManagerController::growWidth(int step)
{
    const bool ok = m_windowManager.nudge(0, 0, step, 0, true);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Width increased" : "Resize failed");
}

void WindowManagerController::shrinkWidth(int step)
{
    const bool ok = m_windowManager.nudge(0, 0, -step, 0, true);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Width decreased" : "Resize failed");
}

void WindowManagerController::growHeight(int step)
{
    const bool ok = m_windowManager.nudge(0, 0, 0, step, true);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Height increased" : "Resize failed");
}

void WindowManagerController::shrinkHeight(int step)
{
    const bool ok = m_windowManager.nudge(0, 0, 0, -step, true);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Height decreased" : "Resize failed");
}

void WindowManagerController::setCustomSize(int width, int height)
{
    const bool ok = m_windowManager.setCustomSize(width, height);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Custom size applied" : "Set size failed");
}

void WindowManagerController::applyHorizontalOverscan(int pixels)
{
    const bool ok = m_windowManager.applyHorizontalOverscan(pixels);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Horizontal overscan applied" : "Overscan failed");
}

void WindowManagerController::applyVerticalOverscan(int pixels)
{
    const bool ok = m_windowManager.applyVerticalOverscan(pixels);
    if (ok) {
        syncStabilizerTarget();
    }
    publishStatus(ok ? "Vertical overscan applied" : "Overscan failed");
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

void WindowManagerController::publishStatus(const QString& prefix)
{
    const QString detail = m_windowManager.lastMessage();
    m_statusText = prefix.isEmpty() ? detail : prefix + ": " + detail;
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
    const bool ok = m_windowManager.setWindowed();
    if (ok) syncStabilizerTarget();
    publishStatus(ok ? "Windowed mode set" : "Windowed failed");
}

void WindowManagerController::setBorderlessFullscreen()
{
    const bool ok = m_windowManager.setBorderlessFullscreen();
    if (ok) syncStabilizerTarget();
    publishStatus(ok ? "True borderless fullscreen set" : "Borderless failed");
}

void WindowManagerController::fitToScreen()
{
    const bool ok = m_windowManager.fitToScreen();
    if (ok) syncStabilizerTarget();
    publishStatus(ok ? "Fit to screen" : "Fit failed");
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
    const bool ok = m_windowManager.resetAll();
    if (ok) syncStabilizerTarget();
    publishStatus(ok ? "Window reset to defaults" : "Reset failed");
}

void WindowManagerController::launchTarget()
{ publishStatus(m_windowManager.launchTarget() ? "Launched" : "Launch failed"); }

void WindowManagerController::killTarget()
{
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