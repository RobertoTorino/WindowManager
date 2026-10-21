#pragma once

#include "stats/SessionReport.h"

#include <QColor>
#include <QPixmap>
#include <QStringList>
#include <QWidget>

#include <vector>

/**
 * The Reports tab's charts, drawn with QPainter (no Qt Charts). Both colour a setup by its place in the
 * game's setup order (Report::compareSetups: first seen first), so a setup keeps its colour in every chart;
 * "starting", "paused" and "not scaling" periods are neutral grey. One value axis per plot (FPS and GPU % are two plots).
 */
namespace ReportColors {
QColor forSetup(const QString &setup, const QStringList &order);
bool isState(const QString &setup);   // starting / paused / not scaling: a moment, not a setup to compare
QColor text();
QColor muted();
QColor grid();
}

// A session over time: FPS (and the 1 % low) above, GPU % (the game's, and Magpie's dashed) below, the line
// coloured by the setup at that moment. Hover: a crosshair and the values at that second.
class SessionChart : public QWidget
{
    Q_OBJECT

public:
    explicit SessionChart(QWidget *parent = nullptr);

    void setSession(const std::vector<Report::Sample> &samples, const QStringList &setupOrder,
                    const QString &title);

    QSize minimumSizeHint() const override { return {360, 220}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect plotArea(int index) const;   // 0: FPS, 1: GPU
    int   sampleAt(int x) const;

    std::vector<Report::Sample> m_samples;
    QStringList m_order;
    QString m_title;
    double  m_fpsMax{60};
    int     m_hoverX{-1};
};

// A game's setups side by side: one row per setup with its average FPS, 1 % low, GPU (the game's and
// Magpie's, stacked) and the time recorded. Each column has its own scale; the values are written out.
class SetupChart : public QWidget
{
    Q_OBJECT

public:
    explicit SetupChart(QWidget *parent = nullptr);

    void setSetups(const std::vector<Report::Segment> &setups, const QStringList &setupOrder);

    QSize minimumSizeHint() const override { return {360, 120}; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    int rowAt(int y) const;

    std::vector<Report::Segment> m_setups;
    QStringList m_order;
};

// A picture that fills the widget (kept in proportion) at any size, or a text when there is none.
class ImageView : public QWidget
{
    Q_OBJECT

public:
    explicit ImageView(QWidget *parent = nullptr);

    void setImage(const QPixmap &image, const QString &emptyText = {});

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_image;
    QString m_empty;
};

