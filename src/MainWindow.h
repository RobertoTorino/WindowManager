#pragma once

#include <QElapsedTimer>
#include <QList>
#include <QMainWindow>

#include "window/FocusSwitch.h"

class ControlPanel;
class DisplaySession;
class FocusMonitor;
class GameOverlay;
class GameWatch;
class GameBar;
class HelixPanel;
class MagpiePanel;
class MagpieSession;
class PictureCompare;
class QAction;
class ReportsPanel;
class SessionRecorder;
class QActionGroup;
class QCloseEvent;
class ScaledView;
class QLabel;
class QPushButton;
class QTabWidget;
class SnapshotCapture;
class MiniBar;
class StatsMonitor;
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
    void applyOpacity();               // View > Window opacity (and only while a game runs)
    void showMiniBar();
    void restoreFromMiniBar();
    void comparePicture();             // Reports tab > Compare picture
    void updatePicture();              // the status bar's scaler line and the game info overlay, every second
    void readGameFiles();              // the game's render scale / resolution as its files hold them now
    void releaseMouse();
    void switchFocus();                // the focus-switch hotkey: game -> this app, else -> the game
    void takeForeground();             // to the front, and locked there against the game taking it back
    void releaseForeground();
    void setMouseOnly(bool on);        // while a game keeps taking the foreground back
    QString foregroundText() const;    // for the log
    void killFromHotkey();             // the terminate hotkey, pressed twice
    void updateFocusSwitch();          // label + the game hotkeys, registered only while a game is found
    void syncHotkey(int id, const QString &text, bool wanted, bool &registered);
    void chooseGameHotkey(bool kill);
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
    MagpiePanel *m_magpie{nullptr};
    MagpieSession *m_magpieSession{nullptr};   // Magpie for the game being started
    ReportsPanel *m_reports{nullptr};
    SessionRecorder *m_recorder{nullptr};    // records each game session for the Reports tab
    PictureCompare *m_pictureCompare{nullptr};
    bool         m_hiddenForCompare{false};
    StatsMonitor *m_monitor{nullptr};   // null when the platform has no stats backend
    GameOverlay *m_overlay{nullptr};    // a window of its own, deleted with this one; null without stats
    QAction     *m_overlayAction{nullptr};
    QAction     *m_overlayCaptureAction{nullptr};
    QElapsedTimer m_magpieSince;          // since the app's Magpie was first seen running
    QActionGroup *m_overlayCornerGroup{nullptr};
    QLabel      *m_scalerLabel{nullptr};  // status bar: which scaler shows the game's picture
    QString      m_lastPictureLog;        // the picture is logged when this changes
    int          m_fileScale{0};          // readGameFiles()
    int          m_fileWidth{0};
    int          m_fileHeight{0};
    bool         m_filesChanged{false};
    int          m_pictureTicks{0};
    MiniBar     *m_mini{nullptr};       // a window of its own, deleted with this one
    GameWatch   *m_watch{nullptr};      // diagnostic log of the game's windows, focus and mouse
    FocusMonitor *m_focus{nullptr};     // who has the keyboard: the game, this app or another program
    QLabel      *m_focusLabel{nullptr};
    QAction     *m_switchFocusAction{nullptr};
    QString      m_focusHotkey{"F1"};   // empty: off
    bool         m_focusHotkeyRegistered{false};
    QString      m_killHotkey{"F2"};    // terminates the game when pressed twice; empty: off
    bool         m_killHotkeyRegistered{false};
    QAction     *m_killHotkeyAction{nullptr};
    FocusSwitch::DoublePress m_killPress;
    bool         m_foregroundLocked{false};   // LockSetForegroundWindow is on (this app took the foreground)
    bool         m_mouseOnly{false};          // this window does not take the foreground (the game keeps it)
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
    QActionGroup *m_opacityGroup{nullptr};       // 100 % ... 40 %
    QAction *m_opacityGameOnlyAction{nullptr};
    QAction *m_confirmKillAction{nullptr};
    QAction *m_fitCheckAction{nullptr};
    bool     m_fitDialogOpen{false};
    bool     m_confirmKill{true};
};
