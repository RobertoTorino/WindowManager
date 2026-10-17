#pragma once

#include <QWidget>

#include "window/FitCalculator.h"
#include "window/Supersample.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class ScaledView;
class QSpinBox;
class SnapshotCapture;
class WindowManagerController;

/** Base for the tab pages. refresh() re-reads live window state. */
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(WindowManagerController* controller, QWidget* parent = nullptr)
        : QWidget(parent), m_controller(controller) {}

    virtual void refresh() {}

protected:
    WindowManagerController* m_controller;
};

/** Visibility, window mode and extended-style toggles. */
class StatePanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit StatePanel(WindowManagerController* controller, QWidget* parent = nullptr);
    void refresh() override;

private:
    QPushButton *m_show, *m_hide, *m_minimize, *m_maximize;
    QPushButton *m_windowed, *m_borderless;
    QPushButton *m_topmost, *m_toolWindow, *m_layered, *m_noActivate;
};

/** Nudge, resize, exact size, overscan and saved profile. */
class PositionPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit PositionPanel(WindowManagerController* controller, QWidget* parent = nullptr);

private:
    QSpinBox* m_step;
    QSpinBox* m_width;
    QSpinBox* m_height;
    QSpinBox* m_overscan;
};

/** One-click resolution presets. */
class PresetPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit PresetPanel(WindowManagerController* controller, QWidget* parent = nullptr);
};

/** Screenshot burst. */
class CapturePanel : public ControlPanel
{
    Q_OBJECT

public:
    CapturePanel(WindowManagerController* controller, SnapshotCapture* capture,
                 QWidget* parent = nullptr);

private:
    QPushButton* m_burst;
};

/** Supersampling: game renders larger than the monitor, shown scaled down. */
class DownscalePanel : public ControlPanel
{
    Q_OBJECT

public:
    DownscalePanel(WindowManagerController* controller, ScaledView* view, QWidget* parent = nullptr);
    void refresh() override;

private:
    bool plan(Supersample::Size& render, Fit::Rect& monitor, int& monitorIndex, quintptr& hwnd);
    void updatePreview();
    void startView();

    ScaledView*  m_view;
    QComboBox*   m_monitor;
    QComboBox*   m_factor;
    QComboBox*   m_aspect;
    QCheckBox*   m_forwardMouse;
    QLabel*      m_preview;
    QPushButton* m_start;
    QPushButton* m_stop;
    QString      m_error;
};
