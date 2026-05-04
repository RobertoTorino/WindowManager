#include "MainWindow.h"

#include <cstring>
#include <QDate>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QAbstractSpinBox>
#include <QShortcut>
#include <QSpinBox>
#include <QSizePolicy>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "window/MonitorHelper.h"

// Converts __DATE__ ("May  4 2026") + __TIME__ ("13:01:39") to "build: 2026-05-04 13:01:39 - RT"
static QString buildTimestamp()
{
    static const char* months[12] = {
        "Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
    };
    const char* d = __DATE__;
    const char* t = __TIME__;
    char mon[4] = {d[0], d[1], d[2], '\0'};
    int m = 1;
    for (int i = 0; i < 12; ++i) {
        if (std::strcmp(months[i], mon) == 0) { m = i + 1; break; }
    }
    const int day  = (d[4] == ' ') ? (d[5] - '0') : (d[4] - '0') * 10 + (d[5] - '0');
    const int year = (d[7]-'0')*1000 + (d[8]-'0')*100 + (d[9]-'0')*10 + (d[10]-'0');
    return QString("build: %1-%2-%3 %4 - RT")
        .arg(year)
        .arg(m,   2, 10, QChar('0'))
        .arg(day, 2, 10, QChar('0'))
        .arg(t);
}

static QPushButton* makeGreenButton(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setStyleSheet("QPushButton{background:#006666;color:white;}"
                       "QPushButton:hover{background:#008888;}");
    return btn;
}

static QPushButton* makeRedButton(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setStyleSheet("QPushButton{background:#660000;color:white;}"
                       "QPushButton:hover{background:#880000;}");
    return btn;
}

static QPushButton* makeLabelButton(const QString& text, QWidget* parent)
{
    auto* btn = new QPushButton(text, parent);
    btn->setEnabled(false);
    btn->setStyleSheet("QPushButton{background:#2a2a2a;color:#aaaaaa;font-weight:bold;}");
    return btn;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new WindowManagerController(this))
    , m_windowTable(nullptr)
    , m_selectExeButton(nullptr)
    , m_launchButton(nullptr)
    , m_stabilizerButton(nullptr)
    , m_killButton(nullptr)
    , m_focusButton(nullptr)
    , m_viewConfigButton(nullptr)
    , m_moveAppButton(nullptr)
    , m_destroyButton(nullptr)
    , m_hideButton(nullptr)
    , m_showButton(nullptr)
    , m_minimizeButton(nullptr)
    , m_maximizeButton(nullptr)
    , m_windowedButton(nullptr)
    , m_borderlessButton(nullptr)
    , m_scanButton(nullptr)
    , m_restoreButton(nullptr)
    , m_fitScreenButton(nullptr)
    , m_topmostButton(nullptr)
    , m_toolWindowButton(nullptr)
    , m_layeredButton(nullptr)
    , m_noActivateButton(nullptr)
    , m_overscanButton(nullptr)
    , m_applySavedButton(nullptr)
    , m_applyHOverscanButton(nullptr)
    , m_overscanSpin(nullptr)
    , m_applyVOverscanButton(nullptr)
    , m_nudgeUpButton(nullptr)
    , m_nudgeDownButton(nullptr)
    , m_nudgeLeftButton(nullptr)
    , m_nudgeRightButton(nullptr)
    , m_stepSpin(nullptr)
    , m_growWButton(nullptr)
    , m_growHButton(nullptr)
    , m_shrinkWButton(nullptr)
    , m_shrinkHButton(nullptr)
    , m_savePositionButton(nullptr)
    , m_widthSpin(nullptr)
    , m_heightSpin(nullptr)
    , m_setSizeButton(nullptr)
    , m_preset1920x1080Button(nullptr)
    , m_preset1920x1200Button(nullptr)
    , m_preset1920x1440Button(nullptr)
    , m_preset2048x1152Button(nullptr)
    , m_preset2048x1536Button(nullptr)
    , m_preset2560x1440Button(nullptr)
    , m_preset2560x1600Button(nullptr)
    , m_preset2880x1800Button(nullptr)
    , m_preset3840x2160Button(nullptr)
    , m_preset4096x2160Button(nullptr)
    , m_preset5120x2880Button(nullptr)
    , m_preset6016x3384Button(nullptr)
    , m_preset7680x4320Button(nullptr)
    , m_label1080pButton(nullptr)
    , m_btm1080pButton(nullptr)
    , m_bl1080pButton(nullptr)
    , m_ffs1080pButton(nullptr)
    , m_ffsa1080pButton(nullptr)
    , m_label1440pButton(nullptr)
    , m_btm1440pButton(nullptr)
    , m_bl1440pButton(nullptr)
    , m_ffs1440pButton(nullptr)
    , m_ffsa1440pButton(nullptr)
    , m_infoButton(nullptr)
    , m_aboutButton(nullptr)
    , m_monitor1Button(nullptr)
    , m_monitor2Button(nullptr)
    , m_resetAllButton(nullptr)
    , m_statusExeLabel(nullptr)
    , m_statusDetailLabel(nullptr)
{
    buildUi();
    refreshStatus();

    // Top-right exe controls
    connect(m_selectExeButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this, "Select Target Executable", QString(), "Executables (*.exe)");
        if (!path.isEmpty())
            m_controller->setTargetExecutable(path);
    });
    connect(m_launchButton,     &QPushButton::clicked, m_controller, &WindowManagerController::launchTarget);
    connect(m_stabilizerButton, &QPushButton::clicked, m_controller, &WindowManagerController::toggleStabilizer);
    connect(m_killButton,       &QPushButton::clicked, m_controller, &WindowManagerController::killTarget);
    connect(m_focusButton,      &QPushButton::clicked, m_controller, &WindowManagerController::focusTarget);
    connect(m_viewConfigButton, &QPushButton::clicked, this, []() {
        const QString configPath = AppConfig::instance().configPath();
        QProcess::startDetached("notepad.exe", {configPath});
    });
    connect(m_infoButton, &QPushButton::clicked, this, [this]() {
        auto *dlg = new QDialog(this);
        dlg->setWindowTitle("WindowManager - Info");
        dlg->resize(760, 600);
        auto *browser = new QTextBrowser(dlg);
        browser->setOpenExternalLinks(false);
        browser->setMarkdown(
            "# WindowManager\n\n"
            "### The main buttons:\n\n"
            "| # | Button | Description |\n"
            "|---|--------|-------------|\n"
            "| 1 | **Select** | **Select the executable.** |\n"
            "| 2 | **Launch** | **Launch the selected executable.** |\n"
            "| 3 | **Scan** | **Scan for windows or refresh the windows list.** |\n"
            "| 4 | **Reset All** | **Reset to default.** |\n"
            "| 5 | **Monitor 1** | **Switch to monitor 1.** |\n"
            "| 6 | **Monitor 2** | **Switch to monitor 2.** |\n"
            "| 7 | **Stability** | **Make sure the correct monitor is selected.** |\n"
            "| 8 | **Kill Process** | **Shutdown the executable.** |\n"
            "| 9 | **Focus** | **Bring back focus to the exe when lost.** |\n"
            "| 10 | **Config** | **Inspect the saved settings.** |\n\n"
            "### The main windows screen:\n\n"
            "Here all windows used by the executable are shown. After selecting a window, actions can be performed on it.\n"
            "The columns represent the window properties and status.\n\n"
            "| # | Button | Description |\n"
            "|---|--------|-------------|\n"
            "| 1 | **Destroy** | **Destroys the selected window.** |\n"
            "| 2 | **Hidden** | **Hide the selected window.** |\n"
            "| 3 | **Show** | **Shows the selected window.** |\n"
            "| 4 | **Minimized** | **Minimizes the selected window.** |\n"
            "| 5 | **Maximized** | **Maximizes the selected window.** |\n"
            "| 6 | **Windowed** | **Show the selected window in windowed modus.** |\n"
            "| 7 | **True Borderless FS** | **Fullscreen without any borders.** |\n"
            "| 8 | **Move App - Monitor** | **Switch between monitors with the app.** |\n"
            "| 9 | **Restore** | **Restores the selected screen to its original state.** |\n"
            "| 10 | **Fit Screen** | **Make sure the whole screen is used.** |\n"
            "| 11 | **Topmost** | **Bring the selected screen to the front.** |\n\n"
            "### Predefined sizes (for testing purposes):\n\n"
            "| # | Name | Resolution |\n"
            "|---|------|------------|\n"
            "| 1 | **Full HD (FHD)** | **1920 x 1080** |\n"
            "| 2 | **Quad HD (QHD / 2K)** | **2560 x 1440** |\n"
            "| 3 | **4K Ultra HD (UHD)** | **3840 x 2160** |\n"
            "| 4 | **5K** | **5120 x 2880** |\n"
            "| 5 | **6K** | **6016 x 3384** |\n"
            "| 6 | **8K Ultra HD (UHD-2)** | **7680 x 4320** |\n\n"
            "### Common Window Modes:\n\n"
            "| # | Mode | Description |\n"
            "|---|------|-------------|\n"
            "| 1 | **Fullscreen** | **Covers the entire screen, often exclusive mode for games.** |\n"
            "| 2 | **Windowed** | **Standard resizable window with title bar and borders.** |\n"
            "| 3 | **Borderless** | **Looks fullscreen but technically a window without borders.** |\n"
            "| 4 | **Hidden** | **Window exists but is invisible.** |\n\n"
            "### Window States (WinAPI):\n\n"
            "| # | State | WinAPI constant | Description |\n"
            "|---|-------|-----------------|-------------|\n"
            "| 1 | **Normal / Restored** | **SW_SHOWNORMAL / SW_RESTORE** | **Standard window size.** |\n"
            "| 2 | **Minimized** | **SW_MINIMIZE** | **Shrunk to taskbar.** |\n"
            "| 3 | **Maximized** | **SW_SHOWMAXIMIZED** | **Fills the screen.** |\n"
            "| 4 | **Hidden** | **SW_HIDE** | **Window exists but invisible.** |\n"
            "| 5 | **Shown / Activated** | **SW_SHOW / SW_SHOWNA** | **Visible and optionally activated.** |\n\n"
            "### Window Styles:\n\n"
            "| # | Style | Description |\n"
            "|---|-------|-------------|\n"
            "| 1 | **WS_OVERLAPPEDWINDOW** | **Typical app window: border, title bar, min/max buttons.** |\n"
            "| 2 | **WS_POPUP** | **Borderless window, often used for fullscreen.** |\n"
            "| 3 | **WS_BORDER** | **Thin border around the window.** |\n"
            "| 4 | **WS_CAPTION** | **Adds title bar.** |\n"
            "| 5 | **WS_SYSMENU** | **Adds system menu.** |\n"
            "| 6 | **WS_MINIMIZEBOX** | **Adds minimize/maximize buttons.** |\n"
            "| 7 | **WS_SIZEBOX / WS_THICKFRAME** | **Allows resizing by dragging edges.** |\n"
            "| 8 | **WS_DISABLED** | **Window cannot receive input.** |\n"
            "| 9 | **WS_VISIBLE** | **Initially visible.** |\n\n"
            "### Extended Window Styles:\n\n"
            "| # | Style | Description |\n"
            "|---|-------|-------------|\n"
            "| 1 | **WS_EX_TOPMOST** | **Window stays above all others.** |\n"
            "| 2 | **WS_EX_TOOLWINDOW** | **Small title bar, used for floating tool windows.** |\n"
            "| 3 | **WS_EX_APPWINDOW** | **Forces a window to appear in the taskbar.** |\n"
            "| 4 | **WS_EX_NOACTIVATE** | **Window shows without stealing focus.** |\n"
            "| 5 | **WS_EX_LAYERED** | **Allows transparency and alpha blending.** |\n"
        );
        auto *btnBox = new QDialogButtonBox(QDialogButtonBox::Close, dlg);
        connect(btnBox, &QDialogButtonBox::rejected, dlg, &QDialog::accept);
        auto *lay = new QVBoxLayout(dlg);
        lay->addWidget(browser);
        lay->addWidget(btnBox);
        dlg->exec();
    });
    connect(m_aboutButton, &QPushButton::clicked, this, [this]() {
        const int year = QDate::currentDate().year();
        QMessageBox::about(
            this,
            "About WindowManager",
            QString("WindowManager - Version: 1.0.0 - %1 - RobertoTorino\n\n"
                    "Features:\n"
                    "* Window management.\n"
                    "* True borderless fullscreen window.\n"
                    "* Fit screen.\n"
                    "* Reposition the window.\n"
                    "* Move window between monitors.")
                .arg(QString("%1 %2").arg(QChar(0x00A9)).arg(year)));
    });
    connect(m_moveAppButton,    &QPushButton::clicked, this, &MainWindow::moveAppToOppositeMonitor);

    // Row 1
    connect(m_destroyButton,    &QPushButton::clicked, m_controller, &WindowManagerController::destroyWindow);
    connect(m_hideButton,       &QPushButton::clicked, m_controller, &WindowManagerController::hideWindow);
    connect(m_showButton,       &QPushButton::clicked, m_controller, &WindowManagerController::showWindow);
    connect(m_minimizeButton,   &QPushButton::clicked, m_controller, &WindowManagerController::minimizeWindow);
    connect(m_maximizeButton,   &QPushButton::clicked, m_controller, &WindowManagerController::maximizeWindow);
    connect(m_windowedButton,   &QPushButton::clicked, m_controller, &WindowManagerController::setWindowed);
    connect(m_borderlessButton, &QPushButton::clicked, m_controller, &WindowManagerController::setBorderlessFullscreen);
    connect(m_scanButton,       &QPushButton::clicked, m_controller, &WindowManagerController::scanWindows);

    // Row 2
    connect(m_restoreButton,    &QPushButton::clicked, m_controller, &WindowManagerController::restoreWindow);
    connect(m_fitScreenButton,  &QPushButton::clicked, m_controller, &WindowManagerController::fitToScreen);
    connect(m_topmostButton,    &QPushButton::clicked, m_controller, &WindowManagerController::toggleTopmost);
    connect(m_toolWindowButton, &QPushButton::clicked, m_controller, &WindowManagerController::toggleToolWindow);
    connect(m_layeredButton,    &QPushButton::clicked, m_controller, &WindowManagerController::toggleLayered);
    connect(m_noActivateButton, &QPushButton::clicked, m_controller, &WindowManagerController::toggleNoActivate);
    connect(m_overscanButton,   &QPushButton::clicked, this, [this]() {
        m_controller->applyHorizontalOverscan(m_overscanSpin->value());
        m_controller->applyVerticalOverscan(m_overscanSpin->value());
    });
    connect(m_applySavedButton, &QPushButton::clicked, m_controller, &WindowManagerController::applySavedSettings);

    // Row 3
    connect(m_applyHOverscanButton, &QPushButton::clicked, this, [this]() {
        m_controller->applyHorizontalOverscan(m_overscanSpin->value());
    });
    connect(m_applyVOverscanButton, &QPushButton::clicked, this, [this]() {
        m_controller->applyVerticalOverscan(m_overscanSpin->value());
    });
    connect(m_nudgeUpButton,    &QPushButton::clicked, this, [this]() { m_controller->nudgeUp(m_stepSpin->value()); });
    connect(m_nudgeDownButton,  &QPushButton::clicked, this, [this]() { m_controller->nudgeDown(m_stepSpin->value()); });
    connect(m_nudgeLeftButton,  &QPushButton::clicked, this, [this]() { m_controller->nudgeLeft(m_stepSpin->value()); });
    connect(m_nudgeRightButton, &QPushButton::clicked, this, [this]() { m_controller->nudgeRight(m_stepSpin->value()); });
    connect(m_growWButton,      &QPushButton::clicked, this, [this]() { m_controller->growWidth(m_stepSpin->value()); });
    connect(m_growHButton,      &QPushButton::clicked, this, [this]() { m_controller->growHeight(m_stepSpin->value()); });
    connect(m_shrinkWButton,    &QPushButton::clicked, this, [this]() { m_controller->shrinkWidth(m_stepSpin->value()); });
    connect(m_shrinkHButton,    &QPushButton::clicked, this, [this]() { m_controller->shrinkHeight(m_stepSpin->value()); });
    connect(m_savePositionButton, &QPushButton::clicked, m_controller, &WindowManagerController::saveCurrentPosition);
    connect(m_setSizeButton,    &QPushButton::clicked, this, [this]() {
        m_controller->setCustomSize(m_widthSpin->value(), m_heightSpin->value());
    });

    // Row 4 presets
    connect(m_preset1920x1080Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(1920, 1080); });
    connect(m_preset1920x1200Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(1920, 1200); });
    connect(m_preset1920x1440Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(1920, 1440); });
    connect(m_preset2048x1152Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(2048, 1152); });
    connect(m_preset2048x1536Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(2048, 1536); });
    connect(m_preset2560x1440Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(2560, 1440); });
    connect(m_preset2560x1600Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(2560, 1600); });
    connect(m_preset2880x1800Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(2880, 1800); });
    connect(m_preset3840x2160Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(3840, 2160); });

    // Row 5: ultra-high + 1080P variants
    connect(m_preset4096x2160Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(4096, 2160); });
    connect(m_preset5120x2880Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(5120, 2880); });
    connect(m_preset6016x3384Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(6016, 3384); });
    connect(m_preset7680x4320Button, &QPushButton::clicked, this, [this]() { m_controller->setCustomSize(7680, 4320); });
    connect(m_btm1080pButton,  &QPushButton::clicked, m_controller, &WindowManagerController::borderlessTM1080p);
    connect(m_bl1080pButton,   &QPushButton::clicked, m_controller, &WindowManagerController::borderlessL1080p);
    connect(m_ffs1080pButton,  &QPushButton::clicked, m_controller, &WindowManagerController::fakeFS1080p);
    connect(m_ffsa1080pButton, &QPushButton::clicked, m_controller, &WindowManagerController::fakeFSA1080p);

    // Row 6: 1440P variants
    connect(m_btm1440pButton,  &QPushButton::clicked, m_controller, &WindowManagerController::borderlessTM1440p);
    connect(m_bl1440pButton,   &QPushButton::clicked, m_controller, &WindowManagerController::borderlessL1440p);
    connect(m_ffs1440pButton,  &QPushButton::clicked, m_controller, &WindowManagerController::fakeFS1440p);
    connect(m_ffsa1440pButton, &QPushButton::clicked, m_controller, &WindowManagerController::fakeFSA1440p);

    // Bottom row
    connect(m_monitor1Button, &QPushButton::clicked, m_controller, &WindowManagerController::moveToMonitor1);
    connect(m_monitor2Button, &QPushButton::clicked, m_controller, &WindowManagerController::moveToMonitor2);
    connect(m_resetAllButton, &QPushButton::clicked, m_controller, &WindowManagerController::resetAll);

    // Table row selection
    connect(m_windowTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
        const int row = current.row();
        if (row < 0) return;
        const auto& wins = m_controller->scannedWindows();
        if (row >= static_cast<int>(wins.size())) return;
        m_controller->setActiveWindow(wins[static_cast<std::size_t>(row)].hwnd);
    });

    connect(m_controller, &WindowManagerController::statusChanged, this, &MainWindow::refreshStatus);

    if (!m_controller->targetExecutable().isEmpty())
        m_controller->scanWindows();
}

void MainWindow::buildUi()
{
    setWindowTitle(QString("WindowManager - %1").arg(buildTimestamp()));
    setWindowIcon(QIcon(":/window-manager-icon.png"));
    setFixedSize(800, 556);
    statusBar()->hide();

    auto* monitorToggleShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_S), this);
    connect(monitorToggleShortcut, &QShortcut::activated, this, [this]() {
        const int monitor = m_controller->activeWindowMonitorIndex();
        if (monitor == 1) {
            m_controller->moveToMonitor2();
        } else {
            m_controller->moveToMonitor1();
        }
    });

    auto *central    = new QWidget(this);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    // ── Row 0: top controls ────────────────────────────────────────────────
    m_selectExeButton  = new QPushButton("Select",            central);
    m_launchButton     = new QPushButton("Launch",            central);
    m_scanButton       = new QPushButton("Scan",              central);
    m_resetAllButton   = makeRedButton("Reset All",           central);
    m_monitor1Button   = new QPushButton("Monitor 1",         central);
    m_monitor2Button   = new QPushButton("Monitor 2",         central);
    m_stabilizerButton = new QPushButton("Stability",         central);
    m_killButton       = makeRedButton("Kill Process",        central);
    m_focusButton      = new QPushButton("Focus",             central);
    m_viewConfigButton = new QPushButton("Config",            central);
    m_moveAppButton    = new QPushButton("Move App | Monitor",   central);
    m_selectExeButton->setMinimumHeight(44);
    m_launchButton->setMinimumHeight(44);
    m_scanButton->setMinimumHeight(44);
    m_resetAllButton->setMinimumHeight(44);
    m_monitor1Button->setMinimumHeight(44);
    m_monitor2Button->setMinimumHeight(44);
    m_stabilizerButton->setMinimumHeight(44);
    m_killButton->setMinimumHeight(44);
    m_focusButton->setMinimumHeight(44);
    m_viewConfigButton->setMinimumHeight(44);

    auto *topControls = new QHBoxLayout();
    topControls->setSpacing(4);
    topControls->setContentsMargins(0, 0, 0, 0);
    topControls->addWidget(m_selectExeButton, 1);
    topControls->addWidget(m_launchButton, 1);
    topControls->addWidget(m_scanButton, 1);
    topControls->addWidget(m_resetAllButton, 1);
    topControls->addWidget(m_monitor1Button, 1);
    topControls->addWidget(m_monitor2Button, 1);
    topControls->addWidget(m_stabilizerButton, 1);
    topControls->addWidget(m_killButton, 1);
    topControls->addWidget(m_focusButton, 1);
    topControls->addWidget(m_viewConfigButton, 1);

    // ── Window table ───────────────────────────────────────────────────────
    m_windowTable = new QTableWidget(central);
    m_windowTable->setColumnCount(9);
    m_windowTable->setHorizontalHeaderLabels({"HWND","Title","Class","Status","PID","X","Y","W","H"});
    m_windowTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_windowTable->setColumnWidth(0, 95);   // HWND
    m_windowTable->setColumnWidth(1, 200);  // Title
    m_windowTable->setColumnWidth(2, 180);  // Class
    m_windowTable->setColumnWidth(3, 90);   // Status
    m_windowTable->setColumnWidth(4, 90);   // PID
    m_windowTable->setColumnWidth(5, 75);   // X
    m_windowTable->setColumnWidth(6, 75);   // Y
    m_windowTable->setColumnWidth(7, 75);   // W
    m_windowTable->setColumnWidth(8, 75);   // H
    m_windowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_windowTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_windowTable->verticalHeader()->setVisible(false);
    m_windowTable->setMinimumHeight(220);

    // ── Row 1: window state ────────────────────────────────────────────────
    m_destroyButton    = makeRedButton("Destroy",                   central);
    m_hideButton       = new QPushButton("Hidden",                  central);
    m_showButton       = new QPushButton("Show",                    central);
    m_minimizeButton   = new QPushButton("Minimized",               central);
    m_maximizeButton   = new QPushButton("Maximized",               central);
    m_windowedButton   = new QPushButton("Windowed",                central);
    m_borderlessButton = new QPushButton("True Borderless FS",      central);
    m_destroyButton->setFixedWidth(86);
    m_hideButton->setFixedWidth(86);
    m_showButton->setFixedWidth(86);
    m_minimizeButton->setFixedWidth(86);
    m_maximizeButton->setFixedWidth(86);
    m_windowedButton->setFixedWidth(86);
    auto *row1 = new QHBoxLayout();
    row1->setSpacing(4);
    row1->setContentsMargins(0, 0, 0, 0);
    row1->addWidget(m_destroyButton);
    row1->addWidget(m_hideButton);
    row1->addWidget(m_showButton);
    row1->addWidget(m_minimizeButton);
    row1->addWidget(m_maximizeButton);
    row1->addWidget(m_windowedButton);
    row1->addWidget(m_borderlessButton);
    row1->addWidget(m_moveAppButton);

    // ── Row 2: more state + profile ────────────────────────────────────────
    m_restoreButton    = new QPushButton("Restore",                 central);
    m_fitScreenButton  = new QPushButton("Fit Screen",              central);
    m_topmostButton    = new QPushButton("Topmost",                 central);
    m_toolWindowButton = new QPushButton("Tool Window",             central);
    m_layeredButton    = new QPushButton("Layered",                 central);
    m_noActivateButton = new QPushButton("No Activate",             central);
    m_overscanButton   = new QPushButton("Overscan",                central);
    m_applySavedButton = makeGreenButton("Force Saved Settings",    central);
    auto *row2 = new QHBoxLayout();
    row2->setSpacing(4);
    row2->setContentsMargins(0, 0, 0, 0);
    row2->addWidget(m_restoreButton);
    row2->addWidget(m_fitScreenButton);
    row2->addWidget(m_topmostButton);
    row2->addWidget(m_toolWindowButton);
    row2->addWidget(m_layeredButton);
    row2->addWidget(m_noActivateButton);
    row2->addWidget(m_overscanButton);
    row2->addWidget(m_applySavedButton);

    // ── Row 3: overscan + nudge ────────────────────────────────────────────
    m_applyHOverscanButton = makeGreenButton("Apply H-Overscan", central);
    m_overscanSpin = new QSpinBox(central);
    m_overscanSpin->setRange(0, 500);
    m_overscanSpin->setValue(0);
    m_overscanSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_overscanSpin->setAlignment(Qt::AlignCenter);
    m_overscanSpin->setFixedWidth(96);
    m_applyVOverscanButton = makeGreenButton("Apply V-Overscan", central);
    m_nudgeUpButton    = new QPushButton("Up", central);
    m_nudgeDownButton  = new QPushButton("Down", central);
    m_nudgeLeftButton  = new QPushButton("Left", central);
    m_nudgeRightButton = new QPushButton("Right", central);
    m_nudgeUpButton->setFixedWidth(56);
    m_nudgeDownButton->setFixedWidth(56);
    m_nudgeLeftButton->setFixedWidth(56);
    m_nudgeRightButton->setFixedWidth(56);
    m_stepSpin = new QSpinBox(central);
    m_stepSpin->setRange(1, 500);
    m_stepSpin->setValue(1);
    m_stepSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_stepSpin->setAlignment(Qt::AlignCenter);
    m_stepSpin->setFixedWidth(96);
    auto *row3 = new QHBoxLayout();
    row3->setSpacing(4);
    row3->setContentsMargins(0, 0, 0, 0);
    row3->addWidget(m_applyHOverscanButton);
    row3->addWidget(m_overscanSpin);
    row3->addWidget(m_applyVOverscanButton);
    row3->addWidget(m_nudgeUpButton);
    row3->addWidget(m_nudgeDownButton);
    row3->addWidget(m_nudgeLeftButton);
    row3->addWidget(m_nudgeRightButton);
    row3->addWidget(m_stepSpin);

    // ── Row 4: resize + save + set size ────────────────────────────────────
    m_growWButton   = new QPushButton("W ++", central);
    m_growHButton   = new QPushButton("H ++", central);
    m_shrinkWButton = new QPushButton("W --",  central);
    m_shrinkHButton = new QPushButton("H --",  central);
    m_growWButton->setFixedWidth(74);
    m_growHButton->setFixedWidth(74);
    m_shrinkWButton->setFixedWidth(74);
    m_shrinkHButton->setFixedWidth(74);
    m_savePositionButton = makeGreenButton("Save", central);
    m_widthSpin  = new QSpinBox(central);
    m_heightSpin = new QSpinBox(central);
    m_widthSpin->setRange(100, 10000);
    m_heightSpin->setRange(100, 10000);
    m_widthSpin->setValue(1920);
    m_heightSpin->setValue(1080);
    m_widthSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_heightSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_widthSpin->setAlignment(Qt::AlignCenter);
    m_heightSpin->setAlignment(Qt::AlignCenter);
    m_widthSpin->setFixedWidth(108);
    m_heightSpin->setFixedWidth(108);
    m_setSizeButton = makeGreenButton("Set", central);
    auto *row4 = new QHBoxLayout();
    row4->setSpacing(4);
    row4->setContentsMargins(0, 0, 0, 0);
    row4->addWidget(m_growWButton);
    row4->addWidget(m_growHButton);
    row4->addWidget(m_shrinkWButton);
    row4->addWidget(m_shrinkHButton);
    row4->addWidget(m_savePositionButton);
    row4->addWidget(m_widthSpin);
    row4->addWidget(m_heightSpin);
    row4->addWidget(m_setSizeButton);

    // ── Row 5: common resolution presets ───────────────────────────────────
    m_preset1920x1080Button = new QPushButton("1920x1080", central);
    m_preset1920x1200Button = new QPushButton("1920x1200", central);
    m_preset1920x1440Button = new QPushButton("1920x1440", central);
    m_preset2048x1152Button = new QPushButton("2048x1152", central);
    m_preset2048x1536Button = new QPushButton("2048x1536", central);
    m_preset2560x1440Button = new QPushButton("2560x1440", central);
    m_preset2560x1600Button = new QPushButton("2560x1600", central);
    m_preset2880x1800Button = new QPushButton("2880x1800", central);
    m_preset3840x2160Button = new QPushButton("3840x2160", central);
    auto *row5 = new QHBoxLayout();
    row5->setSpacing(4);
    row5->setContentsMargins(0, 0, 0, 0);
    row5->addWidget(m_preset1920x1080Button);
    row5->addWidget(m_preset1920x1200Button);
    row5->addWidget(m_preset1920x1440Button);
    row5->addWidget(m_preset2048x1152Button);
    row5->addWidget(m_preset2048x1536Button);
    row5->addWidget(m_preset2560x1440Button);
    row5->addWidget(m_preset2560x1600Button);

    // ── Row 6: remaining predefined presets ────────────────────────────────
    auto *row6 = new QHBoxLayout();
    row6->setSpacing(4);
    row6->setContentsMargins(0, 0, 0, 0);
    row6->addWidget(m_preset2880x1800Button);
    row6->addWidget(m_preset3840x2160Button);

    // ── Continue Row 6 with ultra-high presets ─────────────────────────────
    m_preset4096x2160Button = new QPushButton("4096x2160",     central);
    m_preset5120x2880Button = new QPushButton("5120x2880",     central);
    m_preset6016x3384Button = new QPushButton("6016x3384",     central);
    m_preset7680x4320Button = new QPushButton("7680x4320",     central);
    row6->addWidget(m_preset4096x2160Button);
    row6->addWidget(m_preset5120x2880Button);
    row6->addWidget(m_preset6016x3384Button);
    row6->addWidget(m_preset7680x4320Button);

    // ── Row 7: 1080P mode variants ─────────────────────────────────────────
    m_label1080pButton      = makeLabelButton("1080P",          central);
    m_btm1080pButton        = new QPushButton("Borderless Top Most", central);
    m_bl1080pButton         = new QPushButton("Borderless Layered",  central);
    m_ffs1080pButton        = new QPushButton("Fake Fullscreen",      central);
    m_ffsa1080pButton       = new QPushButton("Fake Fullscreen All",  central);
    m_label1080pButton->setFixedWidth(70);
    m_btm1080pButton->setFixedWidth(145);
    m_bl1080pButton->setFixedWidth(145);
    m_ffs1080pButton->setFixedWidth(135);
    m_ffsa1080pButton->setFixedWidth(145);
    auto *row7 = new QHBoxLayout();
    row7->setSpacing(4);
    row7->setContentsMargins(0, 0, 0, 0);
    row7->addWidget(m_label1080pButton);
    row7->addWidget(m_btm1080pButton);
    row7->addWidget(m_bl1080pButton);
    row7->addWidget(m_ffs1080pButton);
    row7->addWidget(m_ffsa1080pButton);
    row7->addStretch();

    // ── Row 8: 1440P mode variants ─────────────────────────────────────────
    m_label1440pButton = makeLabelButton("1440P",          central);
    m_btm1440pButton   = new QPushButton("Borderless Top Most", central);
    m_bl1440pButton    = new QPushButton("Borderless Layered",  central);
    m_ffs1440pButton   = new QPushButton("Fake Fullscreen",      central);
    m_ffsa1440pButton  = new QPushButton("Fake Fullscreen All",  central);
    m_infoButton       = new QPushButton("Info",  central);
    m_aboutButton      = new QPushButton("About", central);
    m_label1440pButton->setFixedWidth(70);
    m_btm1440pButton->setFixedWidth(145);
    m_bl1440pButton->setFixedWidth(145);
    m_ffs1440pButton->setFixedWidth(135);
    m_ffsa1440pButton->setFixedWidth(145);
    m_infoButton->setFixedWidth(50);
    m_aboutButton->setFixedWidth(60);
    auto *row8 = new QHBoxLayout();
    row8->setSpacing(4);
    row8->setContentsMargins(0, 0, 0, 0);
    row8->addWidget(m_label1440pButton);
    row8->addWidget(m_btm1440pButton);
    row8->addWidget(m_bl1440pButton);
    row8->addWidget(m_ffs1440pButton);
    row8->addWidget(m_ffsa1440pButton);
    row8->addStretch();
    row8->addWidget(m_infoButton);
    row8->addWidget(m_aboutButton);

    // Normalize button geometry so rows line up tightly and consistently.
    const auto allButtons = central->findChildren<QPushButton*>();
    for (QPushButton* button : allButtons) {
        button->setMinimumHeight(24);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    m_selectExeButton->setMinimumHeight(44);
    m_launchButton->setMinimumHeight(44);
    m_scanButton->setMinimumHeight(44);
    m_resetAllButton->setMinimumHeight(44);
    m_monitor1Button->setMinimumHeight(44);
    m_monitor2Button->setMinimumHeight(44);
    m_stabilizerButton->setMinimumHeight(44);
    m_killButton->setMinimumHeight(44);
    m_focusButton->setMinimumHeight(44);
    m_viewConfigButton->setMinimumHeight(44);
    m_infoButton->setMinimumHeight(24);
    m_aboutButton->setMinimumHeight(24);

    // ── Assemble ───────────────────────────────────────────────────────────
    mainLayout->addLayout(topControls);
    mainLayout->addWidget(m_windowTable, 1);
    mainLayout->addLayout(row1);
    mainLayout->addLayout(row2);
    mainLayout->addLayout(row3);
    mainLayout->addLayout(row4);
    mainLayout->addLayout(row5);
    mainLayout->addLayout(row6);
    mainLayout->addLayout(row7);
    mainLayout->addLayout(row8);

    // ── Two-line status area ───────────────────────────────────────────────
    const QString statusStyle =
        "QLabel { background:#1a1a1a; color:#cccccc; padding:2px 4px; font-size:11px; }";
    m_statusExeLabel    = new QLabel("Target EXE: <none>", central);
    m_statusDetailLabel = new QLabel("Ready.",              central);
    m_statusExeLabel->setStyleSheet(statusStyle);
    m_statusDetailLabel->setStyleSheet(statusStyle);
    m_statusExeLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_statusDetailLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(m_statusExeLabel);
    mainLayout->addWidget(m_statusDetailLabel);

    setCentralWidget(central);
}

void MainWindow::refreshStatus()
{
    m_stabilizerButton->setText("Stability");
    const QString exe = m_controller->targetExecutable().isEmpty()
        ? QString("<none>")
        : m_controller->targetExecutable();
    m_statusExeLabel->setText(QString("Target EXE: %1").arg(exe));
    m_statusDetailLabel->setText(m_controller->statusText());
    refreshWindowTable();
}

void MainWindow::refreshWindowTable()
{
    // Guard against re-entrant calls: selectRow() → currentRowChanged → setActiveWindow
    // → publishStatus → statusChanged → refreshStatus → here. Without this guard the
    // recursion corrupts Qt's internal QList and triggers the QList::at assert.
    if (m_refreshingTable) return;
    m_refreshingTable = true;

    const auto& windows   = m_controller->scannedWindows();
    const quintptr activeHwnd = m_controller->activeWindow();

    // Clear the selection BEFORE changing the row count. If a row that is currently
    // selected is about to be removed, QItemSelectionModel::rowsAboutToBeRemoved will
    // iterate its persistent-index list and can hit an out-of-bounds QList::at assert.
    m_windowTable->clearSelection();
    m_windowTable->setRowCount(static_cast<int>(windows.size()));

    int selectRow = -1;
    for (int i = 0; i < static_cast<int>(windows.size()); ++i) {
        const auto& win = windows[static_cast<std::size_t>(i)];
        auto *hwndItem = new QTableWidgetItem(QString::number(win.hwnd));
        auto *titleItem = new QTableWidgetItem(win.title);
        auto *classItem = new QTableWidgetItem(win.className);
        auto *statusItem = new QTableWidgetItem(win.visible ? "Visible" : "Hidden");
        auto *pidItem = new QTableWidgetItem(QString::number(win.pid));
        auto *xItem = new QTableWidgetItem(QString::number(win.x));
        auto *yItem = new QTableWidgetItem(QString::number(win.y));
        auto *wItem = new QTableWidgetItem(QString::number(win.w));
        auto *hItem = new QTableWidgetItem(QString::number(win.h));

        pidItem->setTextAlignment(Qt::AlignCenter);
        xItem->setTextAlignment(Qt::AlignCenter);
        yItem->setTextAlignment(Qt::AlignCenter);
        wItem->setTextAlignment(Qt::AlignCenter);
        hItem->setTextAlignment(Qt::AlignCenter);

        m_windowTable->setItem(i, 0, hwndItem);
        m_windowTable->setItem(i, 1, titleItem);
        m_windowTable->setItem(i, 2, classItem);
        m_windowTable->setItem(i, 3, statusItem);
        m_windowTable->setItem(i, 4, pidItem);
        m_windowTable->setItem(i, 5, xItem);
        m_windowTable->setItem(i, 6, yItem);
        m_windowTable->setItem(i, 7, wItem);
        m_windowTable->setItem(i, 8, hItem);
        if (win.hwnd == activeHwnd)
            selectRow = i;
    }

    if (selectRow >= 0)
        m_windowTable->selectRow(selectRow);
    else
        m_windowTable->clearSelection();

    m_refreshingTable = false;
}

void MainWindow::moveAppToOppositeMonitor()
{
    // Find which monitor the active game window is on
    const int gameMonitor = m_controller->activeWindowMonitorIndex(); // 0 = unknown

    const auto monitors = MonitorHelper::enumerateMonitors();
    if (monitors.empty()) return;

    // Pick the first monitor that isn't the game's monitor.
    // If game monitor is unknown (0) or only one monitor exists, just use monitor 1.
    const MonitorGeometry* target = nullptr;
    for (const auto& m : monitors) {
        if (m.index != gameMonitor) {
            target = &m;
            break;
        }
    }
    // Fallback: if every monitor IS the game monitor (single-monitor setup), use index 1
    if (!target) target = &monitors[0];

    // Move this window to the top-left of the chosen monitor
    move(target->x, target->y);
}
