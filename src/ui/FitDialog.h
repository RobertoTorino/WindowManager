#pragma once

#include <QDialog>

#include "window/WindowManager.h"

class QCheckBox;
class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;
class WindowManagerController;

/**
 * "Fit window to monitor": shows the measured window and picture rectangles, the
 * monitor they should fill, and the correction (move + width/height change) the
 * manual buttons would otherwise need. Applying it makes the picture cover the
 * target exactly, then re-measures to confirm.
 */
class FitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FitDialog(WindowManagerController *controller, QWidget *parent = nullptr);

    void done(int result) override;

private:
    WindowManager::FitRequest request() const;
    void recompute();
    void setCell(int row, int column, const QString &text);

    WindowManagerController *m_controller;
    WindowManager::FitMeasurement m_last;

    QLabel *m_header;
    QComboBox *m_monitor;
    QComboBox *m_area;
    QCheckBox *m_strip;
    QGridLayout *m_table;
    QLabel *m_frameInfo;
    QLabel *m_positionLine;
    QLabel *m_sizeLine;
    QLabel *m_summary;
    QCheckBox *m_dontAsk;
    QPushButton *m_apply;
};
