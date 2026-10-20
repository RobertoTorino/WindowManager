#pragma once

#include <QList>
#include <QMainWindow>

class ControlPanel;
class DisplaySession;
class GameWatch;
class GameBar;
class HelixPanel;
class QAction;
class QCloseEvent;
class ScaledView;
class QLabel;
class QPushButton;
class QTabWidget;
class SnapshotCapture;
class MiniBar;
class StatsBar;
class StatsMonitor;
class QToolBar;
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

    // Show this window (also out of the floating bar) and give it the keyboard: another start of the
    // app, the release-mouse hotkey.
    void bringToFront();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void buildUi();
    void buildMenus();
    QWidget *buildActionRow();

    void refresh();
    void refreshGames();

    void addGame();
    void findSteamGames(bool quiet);
    void startSelectedGame();
    void runWithHelix(const QString &exePath);
    void launchGame(const QString &exePath);
    void launchWhenFree(const QString &exePath);
    void restartSelectedGame();
    void killSelectedGame();
    void shutdownTarget();
    bool confirmKill(const QString &exePath);

    void showFitDialog();
    void toggleGameMonitor();
    void moveAppToOppositeMonitor();
    void setAlwaysOnTop(bool onTop);
    void showMiniBar();
    void restoreFromMiniBar();
    void updateStatsMonitor();
    void releaseMouse();
    int  gameMonitorIndex() const;   // the downscaled view's monitor, else the game window's; 0: unknown
    void keepOffGameMonitor();

    WindowManagerController *m_controller;
    SnapshotCapture *m_snapshots;
    ScaledView *m_view;
    DisplaySession *m_display;

    GameBar     *m_gameBar{nullptr};
    WindowTable *m_table{nullptr};
    QTabWidget  *m_tabs{nullptr};
    QList<ControlPanel *> m_panels;
    HelixPanel  *m_helix{nullptr};
    StatsBar    *m_stats{nullptr};
    QToolBar    *m_statsBar{nullptr};   // null when the platform has no stats backend
    StatsMonitor *m_monitor{nullptr};   // null when the platform has no stats backend
    MiniBar     *m_mini{nullptr};       // a window of its own, deleted with this one
    GameWatch   *m_watch{nullptr};      // diagnostic log of the game's windows, focus and mouse
    QAction     *m_miniOnMinimizeAction{nullptr};
    bool         m_miniPlaced{false};   // has a position (saved, or shown before)
    QAction     *m_avoidGameMonitorAction{nullptr};
    QTimer      *m_keepOffTimer{nullptr};   // waits for the game window to settle
    QTimer      *m_listTimer{nullptr};      // re-reads the window list every few seconds

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
