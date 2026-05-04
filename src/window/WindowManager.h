#pragma once

#include <QString>
#include <QtGlobal>
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
};
