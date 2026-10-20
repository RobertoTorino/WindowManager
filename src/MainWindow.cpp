#include "MainWindow.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/BuildInfo.h"
#include "core/Logger.h"
#include "display/DisplaySession.h"
#include "stats/StatsBackend.h"
#include "ui/ControlPanels.h"
#include "ui/DisplayPanel.h"
#include "ui/FitDialog.h"
#include "ui/GameBar.h"
#include "ui/GpuPanel.h"
#include "ui/ScaledView.h"
#include "ui/GameRegistry.h"
#include "ui/HelixPanel.h"
#include "ui/HelpDialog.h"
#include "ui/InfoDialogs.h"
#include "ui/LogViewerDialog.h"
#include "ui/SnapshotCapture.h"
#include "ui/MiniBar.h"
#include "window/GameWatch.h"
#include "ui/StatsBar.h"
#include "ui/StatsMonitor.h"
#include "ui/Theme.h"
#include "ui/WindowTable.h"
#include "window/MonitorHelper.h"

#include <algorithm>

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileDialog>
#include <QGuiApplication>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QScreen>
#include <QShortcut>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

namespace {

constexpr int kStopViewHotkey = 1;
constexpr int kReleaseMouseHotkey = 2;

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
    , m_display(new DisplaySession(this))
{
    setWindowTitle(BuildInfo::windowTitle());
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
    connect(m_display, &DisplaySession::message, this, [this](const QString &text) {
        m_detailLabel->setText(text);
    });

    // HelixSR tab
    connect(m_helix, &HelixPanel::message, this, [this](const QString &text) {
        m_detailLabel->setText(text);
    });
    connect(m_helix, &HelixPanel::runRequested, this, &MainWindow::runWithHelix);
    connect(m_helix, &HelixPanel::updateAvailableChanged, this, [this](bool available) {
        m_tabs->setTabText(m_tabs->indexOf(m_helix), available ? "HelixSR (update)" : "HelixSR");
    });
    connect(m_view, &ScaledView::runningChanged, this, [this](bool running) {
        if (running)   // keep this window above the full-screen overlay
            QTimer::singleShot(0, this, [this]() { raise(); });
    });
    // Global hotkey: the overlay covers the screen, so stopping must not need a click.
#ifdef Q_OS_WIN
    RegisterHotKey(reinterpret_cast<HWND>(winId()), kStopViewHotkey,
                   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'D');
    RegisterHotKey(reinterpret_cast<HWND>(winId()), kReleaseMouseHotkey,
                   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M');
#endif

    // Keep this window off the game's monitor: after every action, when the game moves its window,
    // and when the downscaled view starts or stops.
    m_keepOffTimer = new QTimer(this);
    m_keepOffTimer->setSingleShot(true);
    m_keepOffTimer->setInterval(400);
    connect(m_keepOffTimer, &QTimer::timeout, this, &MainWindow::keepOffGameMonitor);
    connect(m_controller, &WindowManagerController::statusChanged, m_keepOffTimer, qOverload<>(&QTimer::start));
    connect(m_view, &ScaledView::runningChanged, m_keepOffTimer, qOverload<>(&QTimer::start));
    if (m_watch)
        connect(m_watch, &GameWatch::windowsChanged, m_keepOffTimer, qOverload<>(&QTimer::start));

    // The window list follows the game by itself: at once when the game watch sees a window change or
    // the game start / stop, else every 3 s (also on systems without the watch). Scan does it on demand.
    if (m_watch)
        connect(m_watch, &GameWatch::windowsChanged, m_controller, &WindowManagerController::refreshWindows);
    if (m_monitor)
        connect(m_monitor, &StatsMonitor::gameChanged, m_controller, &WindowManagerController::refreshWindows);
    m_listTimer = new QTimer(this);
    m_listTimer->setInterval(3000);
    connect(m_listTimer, &QTimer::timeout, this, [this]() {
        if (isVisible() && !isMinimized() && !m_controller->targetExecutable().isEmpty())
            m_controller->refreshWindows();
    });
    m_listTimer->start();

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
    if (m_statsBar)
        m_statsBar->setVisible(settings.value("statsBar", true).toBool());
    m_miniOnMinimizeAction->setChecked(settings.value("miniBar", true).toBool());
    m_avoidGameMonitorAction->setChecked(settings.value("avoidGameMonitor", true).toBool());
    if (settings.contains("miniBarPos")) {
        m_mini->move(settings.value("miniBarPos").toPoint());
        m_miniPlaced = true;
    }
    updateStatsMonitor();

    // A game's display settings left behind by a crash
    m_display->recoverFromCrash();

    refreshGames();
    refresh();
    if (!m_controller->targetExecutable().isEmpty())
        m_controller->scanWindows();
    findSteamGames(true);
}

MainWindow::~MainWindow()
{
    delete m_mini;
#ifdef Q_OS_WIN
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kStopViewHotkey);
    UnregisterHotKey(reinterpret_cast<HWND>(winId()), kReleaseMouseHotkey);
#endif
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    const MSG *msg = static_cast<const MSG *>(message);
    if (msg->message == WM_HOTKEY && msg->wParam == kStopViewHotkey) {
        m_view->stop();
        *result = 0;
        return true;
    }
    if (msg->message == WM_HOTKEY && msg->wParam == kReleaseMouseHotkey) {
        releaseMouse();
        *result = 0;
        return true;
    }
#endif
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
    auto *display  = new DisplayPanel(m_controller, m_display, m_tabs);
    connect(display, &DisplayPanel::message, this, [this](const QString &text) {
        m_detailLabel->setText(text);
    });
    m_tabs->addTab(state,    "Window");
    m_tabs->addTab(position, "Position && Size");
    m_tabs->addTab(presets,  "Presets");
    m_tabs->addTab(downscale, "Downscale");
    m_tabs->addTab(display,  "Display");
    auto *gpu = new GpuPanel(m_controller, m_tabs);
    connect(gpu, &GpuPanel::message, this, [this](const QString &text) { m_detailLabel->setText(text); });
    connect(gpu, &GpuPanel::restartRequested, this, &MainWindow::restartSelectedGame);
    m_tabs->addTab(gpu,      "GPU && Power");
    m_tabs->addTab(capture,  "Capture");
    m_helix = new HelixPanel(m_controller, m_tabs);
    m_tabs->addTab(m_helix,  "HelixSR");
    m_panels = {state, position, presets, downscale, display, gpu, capture, m_helix};

    // QTabWidget sizes itself to its tallest page, which leaves blank space under
    // shorter ones. Cap its height to tab bar + current page instead; the window
    // table (the only stretchy row) takes whatever is left.
    auto fitCurrentPage = [this, layout](int current) {
        const QWidget *page = m_tabs->widget(current);
        if (!page)
            return;
        constexpr int kPaneFrame = 8;
        // Word-wrapped labels (HelixSR's "This PC" line) need more lines than the size hint assumes
        // when the text is large: use the height the page wants at its real width.
        const int pageHeight = page->hasHeightForWidth() ? page->heightForWidth(page->width())
                                                         : page->sizeHint().height();
        const int tabsHeight = m_tabs->tabBar()->sizeHint().height() + pageHeight + kPaneFrame;
        m_tabs->setMaximumHeight(tabsHeight);

        // A page can grow while hidden (HelixSR fills in its texts and game list later), and with
        // large fonts it may then need more room than the window has. Grow the window rather than
        // squeeze the page, which makes its widgets overlap. The window's minimum alone is not
        // enough: it ignores the extra lines of wrapped labels, so give the tabs their full height,
        // as far as the screen allows (the title bar and the status bar must stay on it).
        layout->invalidate();
        QTimer::singleShot(0, this, [this, tabsHeight]() {   // once the layout has been recalculated
            const int extra = std::max(0, tabsHeight - m_tabs->minimumSizeHint().height());
            int needed = std::max(620, minimumSizeHint().height() + extra);
            if (const QScreen *s = screen()) {
                const int frame = frameGeometry().height() - height();
                needed = std::max(minimumSizeHint().height(),
                                  std::min(needed, s->availableGeometry().height() - frame));
            }
            setMinimumHeight(needed);
            if (height() < needed)
                resize(width(), needed);
            // a grown window may now run off the bottom of the screen: move it up
            if (const QScreen *s = screen()) {
                const QRect avail = s->availableGeometry();
                const int overflow = frameGeometry().bottom() - avail.bottom();
                if (overflow > 0)
                    move(x(), std::max(avail.top(), y() - overflow));
            }
        });
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

    // Second bar above the status bar: the running game's FPS, CPU, GPU, memory and power state
    if (StatsBar::available()) {
        m_monitor = new StatsMonitor(m_controller, this);
        m_stats = new StatsBar(m_controller, this);
        m_statsBar = new QToolBar("Game stats", this);
        m_statsBar->setObjectName("statsBar");
        m_statsBar->setMovable(false);
        m_statsBar->setFloatable(false);
        m_statsBar->addWidget(m_stats);
        addToolBar(Qt::BottomToolBarArea, m_statsBar);
        connect(m_monitor, &StatsMonitor::statsUpdated, m_stats, &StatsBar::display);
        connect(m_monitor, &StatsMonitor::statsUpdated, gpu, &GpuPanel::showStats);
        connect(m_statsBar->toggleViewAction(), &QAction::toggled, this, &MainWindow::updateStatsMonitor);
        if (GameWatch::supported()) {
            m_watch = new GameWatch(this);
            connect(m_monitor, &StatsMonitor::gameChanged, m_watch, &GameWatch::watch);
        }
    }

    // The floating bar the app minimizes to
    m_mini = new MiniBar;
    connect(m_mini, &MiniBar::restoreRequested, this, &MainWindow::restoreFromMiniBar);
    connect(m_mini, &MiniBar::focusGameRequested, m_controller, &WindowManagerController::focusTarget);
    connect(m_mini, &MiniBar::exitRequested, this, &QWidget::close);
    if (m_monitor)
        connect(m_monitor, &StatsMonitor::statsUpdated, m_mini, &MiniBar::display);
    else
        m_mini->setStatsShown(false);
}

QWidget *MainWindow::buildActionRow()
{
    auto *row = new QWidget(this);
    auto *h   = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);

    auto *scan = Theme::button("Scan", row, nullptr,
                               "Read the window list again and pick the best window (the list also follows the game by itself)");
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
    file->addAction("Find &Steam games", this, [this]() { findSteamGames(false); });
    file->addAction("Open &config file", this, []() {
#ifdef Q_OS_WIN
        QProcess::startDetached("notepad.exe", {AppConfig::instance().configPath()});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(AppConfig::instance().configPath()));
#endif
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
    if (m_statsBar) {
        QAction *stats = m_statsBar->toggleViewAction();
        stats->setText("Game &stats bar");
        view->addAction(stats);
    }
    m_avoidGameMonitorAction = view->addAction("&Keep this app off the game's monitor");
    m_avoidGameMonitorAction->setCheckable(true);
    connect(m_avoidGameMonitorAction, &QAction::toggled, this, [this](bool on) {
        if (on)
            m_keepOffTimer->start();
    });
    view->addAction("Release the mouse\tCtrl+Alt+M", this, &MainWindow::releaseMouse);
    m_miniOnMinimizeAction = view->addAction("Minimize to &floating bar");
    m_miniOnMinimizeAction->setCheckable(true);
    view->addAction("Switch to floating &bar", QKeySequence(Qt::CTRL | Qt::Key_B), this, &MainWindow::showMiniBar);
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
    if (m_mini) {
        const auto game = exe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(exe);
        m_mini->setGame(exe.isEmpty() ? QString()
                        : game && !game->savedName.isEmpty() ? game->savedName
                                                             : QFileInfo(exe).completeBaseName());
    }
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
#ifdef Q_OS_WIN
        "Launch Targets (*.exe *.bat *.cmd)");
#else
        "Launch Targets (*.exe *.sh *.x86_64);;All Files (*)");
#endif
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

// Adds the games installed through Steam to the game list (and drops uninstalled ones). Runs in the
// background; `quiet` (at start-up) only reports when something changed.
void MainWindow::findSteamGames(bool quiet)
{
    if (!quiet)
        m_detailLabel->setText("Looking for Steam games...");
    SteamLibrary::findGamesAsync(this, [this, quiet](const SteamLibrary::Scan &scan) {
        const auto sync = GameRegistry::syncSteamGames(scan);
        Logger::instance().info("Steam games: " + std::to_string(scan.games.size()) + " installed, "
                                    + std::to_string(sync.added) + " added, " + std::to_string(sync.removed)
                                    + " removed",
                                "MainWindow");
        if (sync.added > 0 || sync.removed > 0) {
            refreshGames();
            m_helix->gamesChanged();
        }
        if (quiet && sync.added == 0 && sync.removed == 0)
            return;
        QString text = scan.libraries.isEmpty()
                           ? QString("No Steam library found.")
                           : QString("Steam games: %1 installed").arg(scan.games.size());
        if (sync.added > 0)
            text += QString(", %1 added").arg(sync.added);
        if (sync.removed > 0)
            text += QString(", %1 uninstalled game(s) removed").arg(sync.removed);
        m_detailLabel->setText(text + '.');
    });
}

void MainWindow::startSelectedGame()
{
    const QString path = m_gameBar->selectedPath();
    if (!path.isEmpty())
        launchGame(path);
}

void MainWindow::runWithHelix(const QString &exePath)
{
    m_controller->setTargetExecutable(exePath);
    refreshGames();   // show it in the game bar
    launchGame(exePath);
}

// Starts a game, never a second copy: a running game (this one or another) has to be closed first.
void MainWindow::launchGame(const QString &exePath)
{
    const auto nameOf = [](const QString &path) {
        const auto game = AppConfig::instance().gameByPath(GameRegistry::normalizePath(path));
        return game && !game->savedName.isEmpty() ? game->savedName : QFileInfo(path).fileName();
    };

    QStringList running;   // paths
    if (WindowManagerController::isRunning(exePath))
        running << exePath;
    for (const auto &game : AppConfig::instance().games())
        if (QDir::cleanPath(game.applicationPath).compare(QDir::cleanPath(exePath), Qt::CaseInsensitive) != 0
            && WindowManagerController::isRunning(game.applicationPath))
            running << game.applicationPath;

    if (!running.isEmpty()) {
        QStringList names;
        for (const auto &path : running)
            names << nameOf(path);
        const bool same = running.contains(exePath);
        QMessageBox box(QMessageBox::Warning, "Game already running", QString(), QMessageBox::NoButton, this);
        box.setText(same ? QString("%1 is already running.").arg(names.join(", "))
                         : QString("%1 is running.").arg(names.join(", ")));
        box.setInformativeText(QString("Close it and start %1? This force-closes it. Unsaved progress will be lost.")
                                   .arg(nameOf(exePath)));
        auto *close  = box.addButton(same ? "Close and restart" : "Close and start",
                                     QMessageBox::DestructiveRole);
        auto *cancel = box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(cancel);
        box.setEscapeButton(cancel);
        box.exec();
        if (box.clickedButton() != close)
            return;
        for (const auto &path : running) {
            m_controller->setTargetExecutable(path);
            m_controller->killTarget();
        }
    }
    launchWhenFree(exePath);
}

// Launches once no old process is left (launchTarget refuses while one runs). The game's own
// display settings (Display tab) are applied first, as games read the resolution at start-up.
void MainWindow::launchWhenFree(const QString &exePath)
{
    const auto launch = [this, exePath]() {
        m_display->gameStarting(exePath);
        if (!m_controller->launchTarget())
            m_display->restore("the launch failed");
    };
    m_controller->setTargetExecutable(exePath);
    if (!m_controller->isTargetRunning()) {
        launch();
        return;
    }
    auto *timer = new QTimer(this);
    timer->setInterval(200);
    int ticks = 0;
    connect(timer, &QTimer::timeout, this, [this, timer, exePath, ticks, launch]() mutable {
        m_controller->setTargetExecutable(exePath);
        if (m_controller->isTargetRunning() && ++ticks < 25)   // give up after 5 s
            return;
        timer->stop();
        timer->deleteLater();
        launch();
    });
    timer->start();
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
    launchWhenFree(path);
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

int MainWindow::gameMonitorIndex() const
{
    // While the downscaled view runs the game window is parked off-screen; what you see is the overlay.
    if (const int view = m_view->monitorIndex())
        return view;
    return m_controller->activeWindow() ? m_controller->activeWindowMonitorIndex() : 0;
}

void MainWindow::keepOffGameMonitor()
{
    if (!m_avoidGameMonitorAction->isChecked() || !isVisible() || isMinimized()
        || MonitorHelper::getMonitorCount() < 2)
        return;
    const int game = gameMonitorIndex();
    const int app  = MonitorHelper::getMonitorIndexFromWindow(quintptr(winId()));
    if (game == 0 || app != game)
        return;
    moveAppToOppositeMonitor();
    Logger::instance().info(QString("Moved this app from monitor %1 (the game's) to monitor %2")
                                .arg(game).arg(MonitorHelper::getMonitorIndexFromWindow(quintptr(winId())))
                                .toStdString(),
                            "MainWindow");
}

void MainWindow::bringToFront()
{
    if (m_mini->isVisible()) {
        restoreFromMiniBar();
    } else {
        setWindowState(windowState() & ~Qt::WindowMinimized);
        show();
        raise();
        activateWindow();
    }
#ifdef Q_OS_WIN
    SetForegroundWindow(reinterpret_cast<HWND>(winId()));
#endif
}

void MainWindow::releaseMouse()
{
    // A game that locks the mouse (ClipCursor) lets go once it loses the foreground.
#ifdef Q_OS_WIN
    ClipCursor(nullptr);
#endif
    bringToFront();
    Logger::instance().info("Mouse released (Ctrl+Alt+M): this app is in front", "MainWindow");
    m_detailLabel->setText("Mouse released. Click the game to give it the mouse back.");
}

void MainWindow::moveAppToOppositeMonitor()
{
    const int gameMonitor = gameMonitorIndex(); // 0 = unknown

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

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && m_miniOnMinimizeAction
        && m_miniOnMinimizeAction->isChecked())
        QTimer::singleShot(0, this, &MainWindow::showMiniBar);   // not inside the state change
}

void MainWindow::showMiniBar()
{
    // First time, or the saved spot is on a monitor that is gone: top right of this window's screen.
    m_mini->adjustSize();
    if (!m_miniPlaced || !QGuiApplication::screenAt(m_mini->geometry().center())) {
        const QRect area = screen()->availableGeometry();
        m_mini->move(area.right() - m_mini->width() - 24, area.top() + 24);
        m_miniPlaced = true;
    }
    hide();
    m_mini->show();
    updateStatsMonitor();
}

void MainWindow::restoreFromMiniBar()
{
    m_mini->hide();
    setWindowState(windowState() & ~Qt::WindowMinimized);
    show();
    raise();
    activateWindow();
    updateStatsMonitor();
}

void MainWindow::updateStatsMonitor()
{
    if (m_monitor)
        m_monitor->setActive((m_statsBar && !m_statsBar->isHidden()) || m_mini->isVisible());
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_view->stop(); // give the game its window back
    m_display->restore("WindowManager closed");
    QSettings settings = uiSettings();
    settings.setValue("geometry", saveGeometry());
    settings.setValue("alwaysOnTop", windowFlags().testFlag(Qt::WindowStaysOnTopHint));
    settings.setValue("confirmKill", m_confirmKill);
    settings.setValue("fitCheck", m_controller->fitCheckEnabled());
    if (m_statsBar)
        settings.setValue("statsBar", !m_statsBar->isHidden());   // this window may be hidden (floating bar)
    settings.setValue("miniBar", m_miniOnMinimizeAction->isChecked());
    settings.setValue("avoidGameMonitor", m_avoidGameMonitorAction->isChecked());
    if (m_miniPlaced)
        settings.setValue("miniBarPos", m_mini->pos());
    m_mini->hide();
    statsBackend().shutdown();   // ends the frame trace session
    QMainWindow::closeEvent(event);
}
