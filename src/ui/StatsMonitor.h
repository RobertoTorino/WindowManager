#pragma once

#include "stats/StatsModel.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class WindowManagerController;

/**
 * Samples the selected game once a second while something shows it (the stats bar or the floating
 * bar): the reading runs on the thread pool (statsBackend()), Stats::Sampler turns it into
 * GameStats, and statsUpdated() hands that to the views. Writes a summary to the game log every
 * minute, and each warning when it appears.
 */
class StatsMonitor : public QObject
{
    Q_OBJECT

public:
    explicit StatsMonitor(WindowManagerController *controller, QObject *parent = nullptr);

    void setActive(bool active);   // starts / stops the sampling
    const Stats::GameStats &last() const { return m_last; }

signals:
    void statsUpdated(const Stats::GameStats &stats);
    void gameChanged(qint64 pid);   // the running game's process; 0 when it stopped

private:
    void tick();
    qint64 gamePid();
    qint64 magpiePid();   // a running Magpie.exe, looked up every few seconds; 0: none
    void log(const Stats::GameStats &stats);

    WindowManagerController *m_controller;
    QTimer         m_timer;
    bool           m_busy{false};
    Stats::Sampler m_sampler;
    std::vector<Stats::Adapter> m_adapters;
    Stats::GameStats m_last;
    qint64           m_reportedPid{0};

    QString       m_exe;         // the target the pids below belong to
    qint64        m_foundPid{0}; // found by looking for the target's process
    qint64        m_deadPid{0};  // the last pid that turned out to have exited
    QElapsedTimer m_pidSearch;   // that search runs at most every 5 s
    qint64        m_magpiePid{0};
    QElapsedTimer m_magpieSearch;

    QElapsedTimer m_running;     // since the game was first seen running
    qint64        m_nextLogMs{0};
    std::vector<Stats::Warning> m_loggedWarnings;
    bool m_loggedFpsSource{false};
};
