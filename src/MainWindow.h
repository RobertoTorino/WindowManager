#pragma once

#include <QMainWindow>

class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class WindowManagerController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void buildUi();
    void refreshStatus();
    void refreshWindowTable();
        void moveAppToOppositeMonitor();

    WindowManagerController *m_controller;

    QTableWidget *m_windowTable;

    // Top-right exe controls (2 rows)
    QPushButton *m_selectExeButton;
    QPushButton *m_launchButton;
    QPushButton *m_stabilizerButton;
    QPushButton *m_killButton;
        QPushButton *m_focusButton;
        QPushButton *m_moveAppButton;

    // Row 1 (under table): state
    QPushButton *m_destroyButton;
    QPushButton *m_hideButton;
    QPushButton *m_showButton;
    QPushButton *m_minimizeButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_windowedButton;
    QPushButton *m_borderlessButton;
    QPushButton *m_scanButton;

    // Row 2: more state + profile
    QPushButton *m_restoreButton;
    QPushButton *m_fitScreenButton;
    QPushButton *m_topmostButton;
    QPushButton *m_toolWindowButton;
    QPushButton *m_layeredButton;
    QPushButton *m_noActivateButton;
    QPushButton *m_overscanButton;
    QPushButton *m_applySavedButton;

    // Row 3: overscan + nudge
    QPushButton *m_applyHOverscanButton;
    QSpinBox    *m_overscanSpin;
    QPushButton *m_applyVOverscanButton;
    QPushButton *m_nudgeUpButton;
    QPushButton *m_nudgeDownButton;
    QPushButton *m_nudgeLeftButton;
    QPushButton *m_nudgeRightButton;
    QSpinBox    *m_stepSpin;

    // Row 4: resize + save + set size
    QPushButton *m_growWButton;
    QPushButton *m_growHButton;
    QPushButton *m_shrinkWButton;
    QPushButton *m_shrinkHButton;
    QPushButton *m_savePositionButton;
    QSpinBox    *m_widthSpin;
    QSpinBox    *m_heightSpin;
    QPushButton *m_setSizeButton;

    // Row 5: common resolution presets
    QPushButton *m_preset1920x1080Button;
    QPushButton *m_preset1920x1200Button;
    QPushButton *m_preset1920x1440Button;
    QPushButton *m_preset2048x1152Button;
    QPushButton *m_preset2048x1536Button;
    QPushButton *m_preset2560x1440Button;
    QPushButton *m_preset2560x1600Button;
    QPushButton *m_preset2880x1800Button;
    QPushButton *m_preset3840x2160Button;

    // Row 6: ultra-high presets + 1080P mode variants
    QPushButton *m_preset4096x2160Button;
    QPushButton *m_preset5120x2880Button;
    QPushButton *m_preset6016x3384Button;
    QPushButton *m_preset7680x4320Button;
    QPushButton *m_label1080pButton;
    QPushButton *m_btm1080pButton;
    QPushButton *m_bl1080pButton;
    QPushButton *m_ffs1080pButton;
    QPushButton *m_ffsa1080pButton;

    // Row 7: 1440P mode variants
    QPushButton *m_label1440pButton;
    QPushButton *m_btm1440pButton;
    QPushButton *m_bl1440pButton;
    QPushButton *m_ffs1440pButton;
    QPushButton *m_ffsa1440pButton;

    // Bottom row
    QPushButton *m_monitor1Button;
    QPushButton *m_monitor2Button;
    QPushButton *m_resetAllButton;
};
