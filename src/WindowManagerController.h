#pragma once

#include <QObject>
#include <QString>

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
    const std::vector<WindowManager::WindowInfo>& scannedWindows() const;

public slots:
    void setTargetExecutable(const QString& exePath);
    void scanWindows();
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
    void launchTarget();
    void killTarget();

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

private:
    void publishStatus(const QString& prefix = {});
    void syncStabilizerTarget();

    WindowManager m_windowManager;
    WindowStabilizer m_stabilizer;
    QString m_statusText;
};