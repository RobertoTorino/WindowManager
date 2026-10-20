#pragma once

#include "stats/StatsModel.h"

#include <QWidget>

class QLabel;

/**
 * The floating bar the app minimizes to: a small frameless window, always on top, that does not take
 * the keyboard from the game. It shows the game's name, FPS / CPU / GPU and the first warning.
 * Drag it anywhere; double-click or "Open" brings the app back; right-click for more.
 */
class MiniBar : public QWidget
{
    Q_OBJECT

public:
    MiniBar();   // a top-level window of its own: hiding the main window must not hide it

    void setGame(const QString &name);
    void display(const Stats::GameStats &stats);
    void setStatsShown(bool shown);   // false: no stats backend on this platform

signals:
    void restoreRequested();
    void focusGameRequested();
    void exitRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    QLabel *m_game;
    QLabel *m_stats;
    QLabel *m_warning;
};
