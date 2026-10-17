#pragma once

#include <QObject>
#include <QString>

#include "window/FitCalculator.h"

class QTimer;
class QWidget;

/**
 * Downscaled view ("supersampling"): the game window is made larger than the
 * monitor so the game renders more pixels, parked out of sight, and a borderless
 * overlay on the monitor shows a live scaled-down copy of it (a DWM thumbnail).
 *
 * Keyboard and gamepad stay with the game window. The mouse is forwarded: positions on
 * the overlay are mapped back to the game's own coordinates and posted to the game as
 * ordinary mouse messages (works for games that read the mouse from their window, not
 * for ones using raw input / DirectInput / GetCursorPos).
 */
class ScaledView : public QObject
{
    Q_OBJECT

public:
    struct Options
    {
        quintptr  gameHwnd{0};
        Fit::Rect monitor;      // where the overlay goes (the monitor's rectangle)
        int       monitorIndex{0};
        int       renderW{0};   // size the game window is given
        int       renderH{0};
        bool      forwardMouse{true};
    };

    explicit ScaledView(QObject *parent = nullptr);
    ~ScaledView() override;

    bool isRunning() const { return m_running; }
    QString description() const { return m_description; }

    bool start(const Options &options, QString *error = nullptr);
    void stop(const QString &reason = {});

signals:
    void runningChanged(bool running);
    void message(const QString &text);

private:
    class Overlay;
    friend class Overlay;

    bool handleMouse(unsigned int msg, quintptr wParam, qintptr lParam);
    void tick();
    void updateThumbnail();
    void restoreGameWindow();

    struct State;
    State *m_state;          // Win32 details kept out of the header

    bool    m_running{false};
    bool    m_forwardMouse{true};
    bool    m_mouseCaptured{false};
    Fit::Rect m_dest;        // where the picture sits on the overlay (client pixels)
    int     m_gameW{0};      // the game's client size, for mapping
    int     m_gameH{0};
    QString m_description;
    QTimer *m_timer;
    QWidget *m_overlay{nullptr};
};
