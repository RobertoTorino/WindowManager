#pragma once

#include <QString>
#include "FitCalculator.h"
#include <QtGlobal>
#include <cstdint>
#include <unordered_set>
#include <vector>

class WindowManager
{
public:
    struct WindowInfo
    {
        quintptr hwnd{0};
        qint64 pid{0};
        QString title;
        QString className;
        bool visible{false};
        int x{0};
        int y{0};
        int w{0};
        int h{0};
    };

    WindowManager() = default;

    // ----- Fit to monitor -------------------------------------------------
    struct FitRequest
    {
        int  monitorIndex{0};    // 1-based; 0 = the monitor the window is on
        bool workArea{false};    // true: leave the taskbar visible
        bool stripFrame{true};   // remove title bar / borders first (if present)
    };

    struct FitMeasurement
    {
        bool     valid{false};
        QString  error;

        quintptr hwnd{0};
        QString  title;
        QString  className;
        bool     hasFrame{false};      // WS_CAPTION or WS_THICKFRAME present
        Fit::Rect outer;               // window rectangle
        Fit::Rect client;              // picture, in screen coordinates
        Fit::Rect visibleFrame;        // DWM bounds (excludes invisible resize border)
        int      monitorIndex{0};
        Fit::Rect monitor;             // the target monitor (full rectangle)
        Fit::Rect target;              // monitor or work area, per the request
        bool     stripFrame{false};    // effective: frame will be removed
        Fit::Plan plan;
    };

    // The window most likely to be "the game": visible, big enough, not a
    // console / IME helper. `ready` means usable now (visible, not minimized).
    struct PrimaryWindow
    {
        quintptr  hwnd{0};
        bool      ready{false};
        Fit::Rect rect;
    };
    PrimaryWindow findPrimaryWindow() const;

    bool measureFit(const FitRequest& request, FitMeasurement& out);
    bool applyFit(const FitRequest& request);


    void setTargetExecutable(const QString& exePath);
    QString targetExecutable() const;

    const std::vector<WindowInfo>& scannedWindows() const;
    quintptr activeWindow() const;

    bool scanWindowsForTarget();
    bool applySavedProfile();
    bool saveCurrentPosition();
    bool moveToMonitor(int monitorIndex);
    bool nudge(int dx, int dy, int dw = 0, int dh = 0, bool symmetric = false);
    bool setCustomSize(int width, int height);
    bool applyHorizontalOverscan(int pixels);
    bool applyVerticalOverscan(int pixels);
    bool activeWindowRect(int& x, int& y, int& w, int& h) const;
    void setActiveWindow(quintptr hwnd);

    // Window visibility / state
    bool destroyWindow();
    bool hideWindow();
    bool showWindow();
    bool minimizeWindow();
    bool maximizeWindow();
    bool restoreWindow();

    // Window layout / style
    bool setWindowed();
    bool setBorderlessFullscreen();
    bool setBorderlessSize(int monitorIndex, int w, int h);
    bool setFakeFullscreen(int w, int h);
    bool setFakeFullscreenAspect(int w, int h);
    bool fitToScreen();
    bool toggleTopmost();
    bool toggleToolWindow();
    bool toggleLayered();
    bool toggleNoActivate();
    bool resetAll();

    // Process control
    bool launchTarget();
    bool killTarget();

    // Batch-launch child detection
    void snapshotProcesses();
    bool pollForBatchChild(); // true once the real game process is found

        // Focus / monitor helpers
        bool focusTarget();
        int  activeWindowMonitorIndex() const;

    QString lastMessage() const;

    // Process filter lists (case-insensitive exe names)
    static const QStringList& ignoreList();
    static const QStringList& teknoLoaderList();

    static QString processImagePath(qint64 pid);
    static QString windowTitle(quintptr hwnd);
    static QString windowClass(quintptr hwnd);

private:
    bool ensureActiveWindow();
    bool applyRect(quintptr hwnd, int x, int y, int w, int h, bool borderless = true);

    QString m_targetExePath;
    quintptr m_activeWindow{0};
    QString m_lastMessage;
    std::vector<WindowInfo> m_scanned;

    // Batch-launch child detection state
    bool m_batchLaunchPending{false};
    std::unordered_set<uint32_t> m_preLaunchPids;

    // Session tracking so killTarget only touches processes we are responsible for
    std::unordered_set<uint32_t> m_sessionPids;   // launched / detected game pids
    uint64_t                     m_launchTime{0}; // FILETIME of last launch, 0 = none
};
