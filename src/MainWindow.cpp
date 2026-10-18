#include "MainWindow.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "ui/ControlPanels.h"
#include "ui/FitDialog.h"
#include "ui/GameBar.h"
#include "ui/ScaledView.h"
#include "ui/GameRegistry.h"
#include "ui/HelpDialog.h"
#include "ui/InfoDialogs.h"
#include "ui/LogViewerDialog.h"
#include "ui/SnapshotCapture.h"
#include "ui/Theme.h"
#include "ui/WindowTable.h"
#include "window/MonitorHelper.h"

#include <algorithm>

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <Windows.h>

namespace {

constexpr int kStopViewHotkey = 1;

QSettings uiSettings()
{
    return QSettings(QCoreApplication::applicationDirPath() + "/windowmanager.ini",
                     QSettings::IniFormat);
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new WindowManagerController(this))
    , m_snapshots(new SnapshotCapture(m_controller, this))
    , m_view(new ScaledView(this))
{
    setWindowTitle("WindowManager");
    setWindowIcon(QIcon(":/window-manager-icon.png"));
    setMinimumSize(880, 620);
    resize(980, 720);

    buildUi();
    buildMenus();

    // Game bar
    connect(m_gameBar, &GameBar::selectRequested,  this, &MainWindow::addGame);
    connect(m_gameBar, &GameBar::startRequested,   this, &MainWindow::startSelectedGame);
    connect(m_gameBar, &GameBar::restartRequested, this, &MainWindow::restartSelectedGame);
    connect(m_gameBar, &GameBar::killRequested,    this, &MainWindow::killSelectedGame);
    connect(m_gameBar, &GameBar::gameChosen,       m_controller, &WindowManagerController::setTargetExecutable);

    // Table and controller
    connect(m_table, &WindowTable::windowChosen, m_controller, &WindowManagerController::setActiveWindow);
    connect(m_controller, &WindowManagerController::statusChanged, this, &MainWindow::refresh);
    connect(m_controller, &WindowManagerController::fitProposed, this, &MainWindow::showFitDialog);
    connect(m_snapshots, &SnapshotCapture::message, this, [this](const QString &text) {
        m_detailLabel->setText(text);
    });

    connect(m_view, &ScaledView::message, this, [this](const QString &text) {
        m_detailLabel->setText(text);
    });
    connect(m_view, &ScaledView::runningChanged, this, [this](bool running) {
        if (running)   // keep this window above the full-screen overlay
            QTimer::singleShot(0, this, [this]() { raise(); });
    });
    // Global hotkey: the overlay covers the screen, so stopping must not need a click.
    RegisterHotKey(reinterpret_cast<HWND>(winId()), kStopViewHotkey,
                   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'D');

    auto *monitorToggle = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(monitorToggle, &QShortcut::activated, this, &MainWindow::toggleGameMonitor);

    // Window settings
    QSettings settings = uiSettings();
    restoreGeometry(settings.value("geometry").toByteArray());
    setAlwaysOnTop(settings.value("alwaysOnTop", true).toBool());
    m_onTopAction->setChecked(windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    m_confirmKill = settings.value("confirmKill", true).toBool();
    m_confirmKillAction->setChecked(m_confirmKill);
    m_fitCheckAction->setChecked(settings.value("fitCheck", true).toBool());

    refreshGames();
    refresh();
    if (!m_controller->targetExecutable().isEmpty())
        m_controller->scanWindows();
}

MainWindow::~MainWindow()
{
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kStopViewHotkey);
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == kStopViewHotkey) {
        m_view->stop();
        *result = 0;
        return true;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *layout  = new QVBoxLayout(central);
    layout->setContentsMargins(10, 10, 10, 6);
    layout->setSpacing(8);

    m_gameBar = new GameBar(central);
    m_table   = new WindowTable(central);

    m_tabs = new QTabWidget(central);
    auto *state    = new StatePanel(m_controller, m_tabs);
    auto *position = new PositionPanel(m_controller, m_tabs);
    auto *presets  = new PresetPanel(m_controller, m_tabs);
    auto *capture  = new CapturePanel(m_controller, m_snapshots, m_tabs);
    auto *downscale = new DownscalePanel(m_controller, m_view, m_tabs);
    m_tabs->addTab(state,    "Window");
    m_tabs->addTab(position, "Position && Size");
    m_tabs->addTab(presets,  "Presets");
    m_tabs->addTab(downscale, "Downscale");
    m_tabs->addTab(capture,  "Capture");
    m_panels = {state, position, presets, downscale, capture};

    // QTabWidget sizes itself to its tallest page, which leaves blank space under
    // shorter ones. Cap its height to tab bar + current page instead; the window
    // table (the only stretchy row) takes whatever is left.
    auto fitCurrentPage = [this](int current) {
        const QWidget *page = m_tabs->widget(current);
        if (!page)
            return;
        constexpr int kPaneFrame = 8;
        m_tabs->setMaximumHeight(m_tabs->tabBar()->sizeHint().height()
                                 + page->sizeHint().height() + kPaneFrame);
    };
    connect(m_tabs, &QTabWidget::currentChanged, this, fitCurrentPage);
    fitCurrentPage(m_tabs->currentIndex());

    layout->addWidget(m_gameBar);
    layout->addWidget(buildActionRow());
    layout->addWidget(m_table, 1);
    layout->addWidget(m_tabs);
    setCentralWidget(central);

    // Status bar: message on the left, target on the right
    m_detailLabel = new QLabel("Ready.", this);
    m_targetLabel = new QLabel(this);
    m_targetLabel->setObjectName("hint");
    statusBar()->setSizeGripEnabled(true);
    statusBar()->addWidget(m_detailLabel, 1);
    statusBar()->addPermanentWidget(m_targetLabel);
}

QWidget *MainWindow::buildActionRow()
{
    auto *row = new QWidget(this);
    auto *h   = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    auto *scan = Theme::button("Scan", row, nullptr, "Refresh the window list");
    m_monitor1Button  = Theme::button("Monitor 1", row, nullptr, "Move the window to monitor 1 (Ctrl+S toggles)");
    m_monitor2Button  = Theme::button("Monitor 2", row, nullptr, "Move the window to monitor 2 (Ctrl+S toggles)");
    m_stabilityButton = Theme::button("Stability", row, nullptr, "Keep the window in place if the game moves it");
    auto *focus   = Theme::button("Focus", row, nullptr, "Bring the game window to the front");
    auto *destroy = Theme::button("Destroy", row, "danger", "Ask the selected window to close");
    auto *reset   = Theme::button("Reset All", row, "danger", "Restore default styles and forget the saved position");
    auto *kill    = Theme::button("Shutdown", row, "danger", "Terminate this session's game and loader processes");

    for (auto *b : {scan, m_monitor1Button, m_monitor2Button, m_stabilityButton, focus})
        h->addWidget(b);
    h->addStretch(1);
    for (auto *b : {destroy, reset, kill})
        h->addWidget(b);

    auto *c = m_controller;
    connect(scan,              &QPushButton::clicked, c, &WindowManagerController::scanWindows);
    connect(m_monitor1Button,  &QPushButton::clicked, c, &WindowManagerController::moveToMonitor1);
    connect(m_monitor2Button,  &QPushButton::clicked, c, &WindowManagerController::moveToMonitor2);
    connect(m_stabilityButton, &QPushButton::clicked, c, &WindowManagerController::toggleStabilizer);
    connect(focus,             &QPushButton::clicked, c, &WindowManagerController::focusTarget);
    connect(destroy,           &QPushButton::clicked, c, &WindowManagerController::destroyWindow);
    connect(reset,             &QPushButton::clicked, c, &WindowManagerController::resetAll);
    connect(kill,              &QPushButton::clicked, this, &MainWindow::shutdownTarget);
    return row;
}

void MainWindow::buildMenus()
{
    auto *file = menuBar()->addMenu("&File");
    file->addAction("&Add game...", QKeySequence::Open, this, &MainWindow::addGame);
    file->addAction("Open &config file", this, []() {
        QProcess::startDetached("notepad.exe", {AppConfig::instance().configPath()});
    });
    file->addAction("View &logs...", QKeySequence(Qt::CTRL | Qt::Key_L), this, [this]() {
        LogViewerDialog(this).exec();
    });
    file->addSeparator();
    file->addAction("E&xit", QKeySequence(Qt::CTRL | Qt::Key_Q), this, &QWidget::close);

    auto *view = menuBar()->addMenu("&View");
    m_onTopAction = view->addAction("Always on &top");
    m_onTopAction->setCheckable(true);
    connect(m_onTopAction, &QAction::toggled, this, &MainWindow::setAlwaysOnTop);
    m_confirmKillAction = view->addAction("&Confirm before terminating a game");
    m_confirmKillAction->setCheckable(true);
    connect(m_confirmKillAction, &QAction::toggled, this, [this](bool on) { m_confirmKill = on; });
    m_fitCheckAction = view->addAction("Check window &fit after launching a game");
    m_fitCheckAction->setCheckable(true);
    connect(m_fitCheckAction, &QAction::toggled, m_controller, &WindowManagerController::setFitCheckEnabled);
    view->addAction("Fit window to &monitor...", QKeySequence(Qt::CTRL | Qt::Key_F),
                    m_controller, &WindowManagerController::requestFitCheck);
    view->addSeparator();
    view->addAction("&Stop downscaled view", QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_D),
                    m_view, [this]() { m_view->stop(); });
    view->addAction("&Move this app to the other monitor", QKeySequence(Qt::CTRL | Qt::Key_M),
                    this, &MainWindow::moveAppToOppositeMonitor);

    auto *help = menuBar()->addMenu("&Help");
    help->addAction("&Help topics", QKeySequence::HelpContents, this, [this]() { HelpDialog::open(this); });
    help->addAction("&Keyboard shortcuts", this, [this]() { HelpDialog::open(this, "shortcuts"); });
    help->addAction("&Logs and troubleshooting", this, [this]() { HelpDialog::open(this, "logs-and-troubleshooting"); });
    help->addSeparator();
    help->addAction("&About", this, [this]() { InfoDialogs::showAbout(this); });
}

// ---------------------------------------------------------------------------
// State -> UI
// ---------------------------------------------------------------------------

void MainWindow::refresh()
{
    const QString exe = m_controller->targetExecutable();
    m_targetLabel->setText(exe.isEmpty() ? QString("No target") : exe);
    m_detailLabel->setText(m_controller->statusText());

    const int monitor = m_controller->activeWindow() ? m_controller->activeWindowMonitorIndex() : 0;
    Theme::setActive(m_monitor1Button, monitor == 1);
    Theme::setActive(m_monitor2Button, monitor == 2);
    Theme::setActive(m_stabilityButton, m_controller->stabilizerEnabled());

    m_table->setEmptyMessage(
        exe.isEmpty()
            ? QString("No game selected.\nUse \"Add game...\" to register one.")
            : QString("No windows found for %1.\nStart the game, then press Scan.")
                  .arg(QFileInfo(exe).fileName()));
    m_table->setWindows(m_controller->scannedWindows(), m_controller->activeWindow());
    for (auto *panel : m_panels)
        panel->refresh();
}

void MainWindow::refreshGames()
{
    QList<QPair<QString, QString>> games;
    for (const auto &g : AppConfig::instance().games())
        games.append({g.savedName, g.applicationPath});
    std::sort(games.begin(), games.end(), [](const auto &a, const auto &b) {
        return a.first.toUpper() < b.first.toUpper();
    });
    m_gameBar->setGames(games, GameRegistry::normalizePath(m_controller->targetExecutable()));
}

// ---------------------------------------------------------------------------
// Game actions
// ---------------------------------------------------------------------------

void MainWindow::addGame()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Target Executable or Batch File", QString(),
        "Launch Targets (*.exe *.bat *.cmd)");
    if (path.isEmpty())
        return;

    const QString normalized = GameRegistry::normalizePath(path);
    const auto existing = AppConfig::instance().gameByPath(normalized);
    const QString defaultName = GameRegistry::defaultFriendlyName(normalized);
    const QString initialName = existing && !existing->savedName.isEmpty() ? existing->savedName : defaultName;

    const auto name = GameRegistry::promptForFriendlyName(this, defaultName, initialName);
    if (!name)
        return;

    AppConfig::instance().saveGame(GameRegistry::buildEntry(normalized, *name));
    m_controller->setTargetExecutable(normalized);
    refreshGames();
}

void MainWindow::startSelectedGame()
{
    const QString path = m_gameBar->selectedPath();
    if (path.isEmpty())
        return;
    m_controller->setTargetExecutable(path);
    m_controller->launchTarget();
}

void MainWindow::restartSelectedGame()
{
    const QString path = m_gameBar->selectedPath();
    if (path.isEmpty())
        return;
    if (!confirmKill(path))
        return;
    m_controller->setTargetExecutable(path);
    m_controller->killTarget();
    QTimer::singleShot(350, this, [this, path]() {
        m_controller->setTargetExecutable(path);
        m_controller->launchTarget();
    });
}

void MainWindow::killSelectedGame()
{
    const QString path = m_gameBar->selectedPath();
    if (path.isEmpty())
        return;
    if (!confirmKill(path))
        return;
    m_controller->setTargetExecutable(path);
    m_controller->killTarget();
}

void MainWindow::shutdownTarget()
{
    if (!confirmKill(m_controller->targetExecutable()))
        return;
    m_controller->killTarget();
}

bool MainWindow::confirmKill(const QString &exePath)
{
    if (!m_confirmKill)
        return true;

    QString name = QFileInfo(exePath).fileName();
    if (const auto game = AppConfig::instance().gameByPath(GameRegistry::normalizePath(exePath));
        game && !game->savedName.isEmpty())
        name = game->savedName;
    if (name.isEmpty())
        name = "the current session";

    QMessageBox box(QMessageBox::Warning, "Terminate game?",
                    QString("Terminate %1?").arg(name), QMessageBox::NoButton, this);
    box.setInformativeText("This force-closes the game and the loader processes started for it. "
                           "Unsaved progress will be lost.");
    auto *terminate = box.addButton("Terminate", QMessageBox::DestructiveRole);
    auto *cancel    = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    auto *dontAsk = new QCheckBox("Don't ask again", &box);
    box.setCheckBox(dontAsk);
    box.exec();

    if (box.clickedButton() != terminate)
        return false;
    if (dontAsk->isChecked())
        m_confirmKillAction->setChecked(false); // also clears m_confirmKill
    return true;
}

// ---------------------------------------------------------------------------
// Monitors / app window
// ---------------------------------------------------------------------------

void MainWindow::showFitDialog()
{
    if (m_fitDialogOpen)
        return;
    m_fitDialogOpen = true;
    FitDialog dialog(m_controller, this);
    dialog.exec();
    m_fitDialogOpen = false;
    // the dialog's "don't open automatically" box may have turned this off
    m_fitCheckAction->setChecked(m_controller->fitCheckEnabled());
}

void MainWindow::toggleGameMonitor()
{
    if (m_controller->activeWindowMonitorIndex() == 1)
        m_controller->moveToMonitor2();
    else
        m_controller->moveToMonitor1();
}

void MainWindow::moveAppToOppositeMonitor()
{
    const int gameMonitor = m_controller->activeWindowMonitorIndex(); // 0 = unknown

    const auto monitors = MonitorHelper::enumerateMonitors();
    if (monitors.empty())
        return;

    // First monitor that is not the game's; single monitor falls back to the first.
    const MonitorGeometry *target = &monitors.front();
    for (const auto &m : monitors) {
        if (m.index != gameMonitor) {
            target = &m;
            break;
        }
    }
    move(target->x + 40, target->y + 40);
}

void MainWindow::setAlwaysOnTop(bool onTop)
{
    if (windowFlags().testFlag(Qt::WindowStaysOnTopHint) == onTop)
        return;
    const bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, onTop); // hides the window
    if (wasVisible)
        show();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_view->stop(); // give the game its window back
    QSettings settings = uiSettings();
    settings.setValue("geometry", saveGeometry());
    settings.setValue("alwaysOnTop", windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    settings.setValue("confirmKill", m_confirmKill);
    settings.setValue("fitCheck", m_controller->fitCheckEnabled());
    QMainWindow::closeEvent(event);
}
