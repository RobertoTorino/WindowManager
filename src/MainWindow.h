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

    WindowManagerController *m_controller;
    QLabel *m_targetExeLabel;
    QLabel *m_statusLabel;
    QTableWidget *m_windowTable;
    QPushButton *m_selectExeButton;
    QPushButton *m_scanButton;
    QPushButton *m_applySavedButton;
    QPushButton *m_savePositionButton;
    QPushButton *m_monitor1Button;
    QPushButton *m_monitor2Button;
    QPushButton *m_stabilizerButton;

    QSpinBox *m_overscanSpin;
    QSpinBox *m_stepSpin;
    QSpinBox *m_widthSpin;
    QSpinBox *m_heightSpin;

    QPushButton *m_applyHOverscanButton;
    QPushButton *m_applyVOverscanButton;
    QPushButton *m_nudgeUpButton;
    QPushButton *m_nudgeDownButton;
    QPushButton *m_nudgeLeftButton;
    QPushButton *m_nudgeRightButton;
    QPushButton *m_growWButton;
    QPushButton *m_shrinkWButton;
    QPushButton *m_growHButton;
    QPushButton *m_shrinkHButton;
    QPushButton *m_setSizeButton;
    // resolution preset buttons
    QPushButton *m_preset1080Button;
    QPushButton *m_preset1440Button;
    QPushButton *m_preset4kButton;

    // process control
    QPushButton *m_launchButton;
    QPushButton *m_killButton;

    // window state
    QPushButton *m_showButton;
    QPushButton *m_hideButton;
    QPushButton *m_minimizeButton;
    QPushButton *m_maximizeButton;
    QPushButton *m_restoreButton;
    QPushButton *m_destroyButton;
    QPushButton *m_windowedButton;
    QPushButton *m_borderlessButton;
    QPushButton *m_fitScreenButton;
    QPushButton *m_topmostButton;
    QPushButton *m_toolWindowButton;
    QPushButton *m_layeredButton;
    QPushButton *m_noActivateButton;
    QPushButton *m_resetAllButton;
};