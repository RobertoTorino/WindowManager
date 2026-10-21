#pragma once

#include "window/FocusSwitch.h"

#include <QObject>
#include <QString>

class QTimer;
class WindowManagerController;

/**
 * Follows who has the keyboard while a game runs: the game, this app or another program
 * (FocusSwitch::ownerOf on the foreground window, read every 250 ms). The status bar shows it,
 * and MainWindow registers the focus-switch hotkey while a game is found.
 */
class FocusMonitor : public QObject
{
    Q_OBJECT

public:
    explicit FocusMonitor(WindowManagerController *controller, QObject *parent = nullptr);

    FocusSwitch::Owner owner() const { return m_owner; }
    QString foregroundName() const { return m_lastName; }   // the foreground program's file name; empty: none

signals:
    void ownerChanged(FocusSwitch::Owner owner);

private:
    void poll();

    WindowManagerController *m_controller;
    QTimer *m_timer;
    FocusSwitch::Owner m_owner{FocusSwitch::Owner::None};
    qint64 m_lastPid{0};          // the foreground process last time and the game then ...
    QString m_lastExe;
    bool   m_lastRunsGame{false}; // ... and whether it runs the game's exe (read once per process)
    QString m_lastName;           // its file name
};
