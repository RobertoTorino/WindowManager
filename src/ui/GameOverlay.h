#pragma once

#include "stats/PictureInfo.h"

#include <QRect>
#include <QWidget>

#include <vector>

class QLabel;

/**
 * The game info overlay: a see-through panel in a corner of the game's monitor with the scaler, the
 * picture sizes, the scale, the game's window and monitor, FPS / CPU / GPU / memory / power and the
 * warnings (Picture::overlayLines). It never takes the keyboard or the mouse (clicks go through to the
 * game), stays above Magpie's full-screen window, and by default is left out of screenshots, snapshots
 * and recordings.
 */
class GameOverlay : public QWidget
{
    Q_OBJECT

public:
    enum class Corner { TopLeft, TopRight, BottomLeft, BottomRight };

    GameOverlay();   // a top-level window of its own

    void setCorner(Corner corner);
    Corner corner() const { return m_corner; }

    // Left out of screenshots and recordings (the default), or not.
    void setCapturable(bool capturable);
    bool capturable() const { return m_capturable; }

    // Shows the lines in the corner of `monitor` (physical pixels, MonitorHelper's numbers) and puts the panel
    // back on top.
    void display(const QString &title, const std::vector<Picture::Line> &lines, const QRect &monitor);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void place();
    void raiseAboveAll();
    void applyAffinity();

    QLabel *m_text;
    Corner  m_corner{Corner::TopLeft};
    QRect   m_monitor;              // physical pixels; empty: not shown yet
    bool    m_capturable{false};
};
