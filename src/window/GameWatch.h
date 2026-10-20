#pragma once

#include <QObject>

#include <memory>

/**
 * Diagnostic log of a running game, for games that fight over their window, the focus or the mouse
 * (Tekken 7): while watching a process it writes to the log (source "Watch")
 *  - each of the game's top-level windows when it appears, changes size / frame / visibility, or closes;
 *  - every change of the foreground window: the game, this app, or another program (exe name only);
 *  - whether the mouse is held inside a rectangle (ClipCursor).
 * The lines come from Watch::WindowTracker and friends (GameWatchModel). One implementation per platform:
 * src/platform/win32/Win32GameWatch.cpp (WinEvent hooks), and a stand-in under src/platform/unsupported/.
 */
class GameWatch : public QObject
{
    Q_OBJECT

public:
    explicit GameWatch(QObject *parent = nullptr);
    ~GameWatch() override;

    static bool supported();
    void watch(qint64 pid);   // 0 stops; the same pid again does nothing

    struct Impl;   // per platform; public so the OS callbacks (plain functions) can reach it

signals:
    void windowsChanged();   // a game window appeared, moved, changed size or closed

private:
    std::unique_ptr<Impl> d;
};
