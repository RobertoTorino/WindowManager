#pragma once

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QTimer>

#include "window/FitCalculator.h"
#include "window/WindowManager.h"
#include "window/WindowStabilizer.h"

class WindowManagerController : public QObject
{
    Q_OBJECT

public:
    explicit WindowManagerController(QObject *parent = nullptr);

    QString statusText() const;
    QString targetExecutable() const;
    bool stabilizerEnabled() const;
    quintptr activeWindow() const;
    qint64 activeWindowPid() const;   // the game's process; 0: no window chosen
    const std::vector<WindowManager::WindowInfo>& scannedWindows() const;
        int activeWindowMonitorIndex() const;

    // Current extended-style toggles for the active window
    struct StyleState {
        bool topmost{false};
        bool toolWindow{false};
        bool layered{false};
        bool noActivate{false};
    };
    StyleState queryActiveWindowStyleState() const;

    // Current window visibility/state toggles for the active window
    struct VisibilityState {
        bool visible{false};      // IsWindowVisible
        bool minimized{false};    // IsIconic
        bool maximized{false};    // IsZoomed
        bool windowed{false};     // Has WS_CAPTION (not borderless)
    };
    VisibilityState queryActiveWindowVisibilityState() const;

    // Fit to monitor (see FitDialog). When enabled, launching a game arms a
    // check that fires fitProposed() once the game's window has settled.
    bool fitCheckEnabled() const { return m_fitCheckEnabled; }
    void setFitCheckEnabled(bool enabled);
    bool measureFit(const WindowManager::FitRequest& request, WindowManager::FitMeasurement& out);
    void applyFit(const WindowManager::FitRequest& request);

public slots:
    void requestFitCheck();   // propose a fit now for the current window
    void setTargetExecutable(const QString& exePath);
    void scanWindows();
    void refreshWindows();   // quiet: no status text, no log; statusChanged() only when something changed
    void applySavedSettings();
    void saveCurrentPosition();
    void moveToMonitor1();
    void moveToMonitor2();
    void toggleStabilizer();
    void nudgeLeft(int step);
    void nudgeRight(int step);
    void nudgeUp(int step);
    void nudgeDown(int step);
    void growWidth(int step);
    void shrinkWidth(int step);
    void growHeight(int step);
    void shrinkHeight(int step);
    void setCustomSize(int width, int height);
    void applyHorizontalOverscan(int pixels);
    void applyVerticalOverscan(int pixels);
    void setActiveWindow(quintptr hwnd);

    // Window state
    void destroyWindow();
    void hideWindow();
    void showWindow();
    void minimizeWindow();
    void maximizeWindow();
    void restoreWindow();
    void setWindowed();
    void setBorderlessFullscreen();
    void fitToScreen();
    void toggleTopmost();
    void toggleToolWindow();
    void toggleLayered();
    void toggleNoActivate();
    void resetAll();

    // Process control
    bool launchTarget();
    void killTarget();
    bool isTargetRunning() const { return m_windowManager.isTargetRunning(); }
    static bool isRunning(const QString& exePath) { return WindowManager::isExeRunning(exePath); }
        void focusTarget();

    // Resolution + display mode combos
    void borderlessTM1080p();
    void borderlessL1080p();
    void fakeFS1080p();
    void fakeFSA1080p();
    void borderlessTM1440p();
    void borderlessL1440p();
    void fakeFS1440p();
    void fakeFSA1440p();

signals:
    void statusChanged();
    void fitProposed(bool automatic);   // a game window is ready (automatic: the check after Start); show the fit dialog

private slots:
    void onBatchPollTick();
    void onFitTick();

private:
    void publishStatus(const QString& prefix = {});
    void syncStabilizerTarget();
    void setStatus(const QString& text);
    void armFitCheck();
    void disarmFitCheck();
    // Re-syncs the stabilizer on success, then publishes "okMsg"/"failMsg".
    void finish(bool ok, const char* okMsg, const char* failMsg);

    WindowManager m_windowManager;
    WindowStabilizer m_stabilizer;
    QString m_statusText;

    Fit::Trigger  m_fitTrigger;
    QTimer*       m_fitTimer{nullptr};
    QElapsedTimer m_clock;
    bool          m_fitCheckEnabled{true};

    QTimer* m_batchPollTimer{nullptr};
    int     m_batchPollCount{0};
    static constexpr int k_batchPollMaxAttempts = 15;
};