#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>

#include "magpie/MagpieConfig.h"

class QTimer;
class WindowManagerController;

/**
 * Magpie for a game the app starts (the game's setting on the Magpie tab): once the game's window
 * has settled, writes the game's profile into the app's Magpie (the window's own class and exe path,
 * auto-scale full screen, 3D game mode only when chosen) and starts Magpie in the tray; Magpie then
 * scales the window whenever it is in front. A game that shows a splash window first gets its profile
 * written again (and Magpie restarted) for its real window. When the game's process ends, Magpie is
 * closed again.
 *
 * While Magpie scales a game its window must keep its size and stay windowed: the session turns the
 * stabilizer off, and MainWindow skips the fit check (see handles()). See docs/MAGPIE.md.
 */
class MagpieSession : public QObject
{
    Q_OBJECT

public:
    explicit MagpieSession(WindowManagerController* controller, QObject* parent = nullptr);

    // A game is being started: arms the session when the game uses Magpie, else ends a running one.
    void gameStarting(const QString& exePath);

    // True while Magpie is being arranged or runs for this game.
    bool handles(const QString& exePath) const;

signals:
    void message(const QString& text);

private:
    enum class State { Idle, WaitWindow, StopOld, Running };

    void tick();
    void arrange();      // write the profile and start Magpie
    void followWindow(); // the game swapped its window (a splash screen first): profile it again
    void checkSize(quintptr hwnd);
    void stopMagpie(const QString& why);
    void end(const QString& text, bool warn);

    WindowManagerController* m_controller;
    QTimer* m_timer;
    State   m_state{State::Idle};
    QString m_exe;                // the game as the app starts it
    QString m_name;
    Magpie::GameSettings m_settings;
    QElapsedTimer m_clock;        // since the current state began
    quintptr m_lastHwnd{0};
    int      m_stableTicks{0};
    quintptr m_hwnd{0};
    qint64   m_pid{0};
    QString  m_profileClass;      // the window class in the profile Magpie runs with
    bool     m_sizeWarned{false};
};
