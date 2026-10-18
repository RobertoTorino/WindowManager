#pragma once

#include <QList>
#include <QMainWindow>

class ControlPanel;
class GameBar;
class HelixPanel;
class QAction;
class QCloseEvent;
class ScaledView;
class QLabel;
class QPushButton;
class QTabWidget;
class SnapshotCapture;
class WindowManagerController;
class WindowTable;

/**
 * Composes the UI: game bar, primary action row, window table, tabbed control
 * panels and a status bar. All window logic lives in WindowManagerController.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void buildUi();
    void buildMenus();
    QWidget *buildActionRow();

    void refresh();
    void refreshGames();

    void addGame();
    void startSelectedGame();
    void runWithHelix(const QString &exePath);
    void restartSelectedGame();
    void killSelectedGame();
    void shutdownTarget();
    bool confirmKill(const QString &exePath);

    void showFitDialog();
    void toggleGameMonitor();
    void moveAppToOppositeMonitor();
    void setAlwaysOnTop(bool onTop);

    WindowManagerController *m_controller;
    SnapshotCapture *m_snapshots;
    ScaledView *m_view;

    GameBar     *m_gameBar{nullptr};
    WindowTable *m_table{nullptr};
    QTabWidget  *m_tabs{nullptr};
    QList<ControlPanel *> m_panels;
    HelixPanel  *m_helix{nullptr};

    QPushButton *m_monitor1Button{nullptr};
    QPushButton *m_monitor2Button{nullptr};
    QPushButton *m_stabilityButton{nullptr};

    QLabel  *m_detailLabel{nullptr};
    QLabel  *m_targetLabel{nullptr};
    QAction *m_onTopAction{nullptr};
    QAction *m_confirmKillAction{nullptr};
    QAction *m_fitCheckAction{nullptr};
    bool     m_fitDialogOpen{false};
    bool     m_confirmKill{true};
};
