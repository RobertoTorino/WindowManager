#pragma once

#include "stats/StatsModel.h"

#include <QWidget>

class QLabel;
class WindowManagerController;

/**
 * The game stats bar at the bottom of the window: FPS, CPU, GPU, RAM and video memory of the
 * running game, the power state, and a warning when something is known to slow games down
 * (on battery, integrated GPU). Shows what StatsMonitor samples.
 */
class StatsBar : public QWidget
{
    Q_OBJECT

public:
    explicit StatsBar(WindowManagerController *controller, QWidget *parent = nullptr);

    static bool available();   // false: this platform has no backend (the bar is not added)

    void display(const Stats::GameStats &stats);

private:
    struct Item
    {
        QWidget *box{nullptr};
        QLabel  *value{nullptr};
    };
    Item addItem(const QString &caption, const QString &tooltip);

    WindowManagerController *m_controller;
    QLabel *m_idle{nullptr};
    Item m_fps, m_cpu, m_gpu, m_ram, m_vram;
    QLabel *m_warning{nullptr};
    QLabel *m_power{nullptr};
};
