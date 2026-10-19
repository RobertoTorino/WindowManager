#pragma once

#include <QObject>
#include <QString>

class QTimer;
class WindowManagerController;

/** Screenshots of the active window, optionally as a rapid burst. */
class SnapshotCapture : public QObject
{
    Q_OBJECT

public:
    explicit SnapshotCapture(WindowManagerController* controller, QObject* parent = nullptr);

    bool isRunning() const { return m_remaining > 0; }

    void start(int count);
    void openFolder() const;

signals:
    void runningChanged(bool running);
    void message(const QString& text);

private:
    void captureOne();
    void onTick();
    void finish();

    WindowManagerController* m_controller;
    QTimer*  m_timer;
    int      m_remaining{0};
    int      m_saved{0};
    quintptr m_hwnd{0};
    QString  m_saveDir;
    QString  m_baseName;
};
