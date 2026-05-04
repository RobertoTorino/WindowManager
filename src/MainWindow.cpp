#include "MainWindow.h"

#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include "WindowManagerController.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_controller(new WindowManagerController(this))
    , m_targetExeLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_windowTable(nullptr)
    , m_selectExeButton(nullptr)
    , m_scanButton(nullptr)
    , m_applySavedButton(nullptr)
    , m_savePositionButton(nullptr)
    , m_monitor1Button(nullptr)
    , m_monitor2Button(nullptr)
    , m_stabilizerButton(nullptr)
    , m_overscanSpin(nullptr)
    , m_stepSpin(nullptr)
    , m_widthSpin(nullptr)
    , m_heightSpin(nullptr)
    , m_applyHOverscanButton(nullptr)
    , m_applyVOverscanButton(nullptr)
    , m_nudgeUpButton(nullptr)
    , m_nudgeDownButton(nullptr)
    , m_nudgeLeftButton(nullptr)
    , m_nudgeRightButton(nullptr)
    , m_growWButton(nullptr)
    , m_shrinkWButton(nullptr)
    , m_growHButton(nullptr)
    , m_shrinkHButton(nullptr)
    , m_setSizeButton(nullptr)
    , m_preset1080Button(nullptr)
    , m_preset1440Button(nullptr)
    , m_preset4kButton(nullptr)
    , m_launchButton(nullptr)
    , m_killButton(nullptr)
    , m_showButton(nullptr)
    , m_hideButton(nullptr)
    , m_minimizeButton(nullptr)
    , m_maximizeButton(nullptr)
    , m_restoreButton(nullptr)
    , m_destroyButton(nullptr)
    , m_windowedButton(nullptr)
    , m_borderlessButton(nullptr)
    , m_fitScreenButton(nullptr)
    , m_topmostButton(nullptr)
    , m_toolWindowButton(nullptr)
    , m_layeredButton(nullptr)
    , m_noActivateButton(nullptr)
    , m_resetAllButton(nullptr)
{
    buildUi();
    refreshStatus();

    connect(m_selectExeButton, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getOpenFileName(
            this,
            "Select Target Executable",
            QString(),
            "Executables (*.exe)"
        );
        if (!path.isEmpty()) {
            m_controller->setTargetExecutable(path);
        }
    });
    connect(m_scanButton, &QPushButton::clicked, m_controller, &WindowManagerController::scanWindows);
    connect(m_applySavedButton, &QPushButton::clicked, m_controller, &WindowManagerController::applySavedSettings);
    connect(m_savePositionButton, &QPushButton::clicked, m_controller, &WindowManagerController::saveCurrentPosition);
    connect(m_monitor1Button, &QPushButton::clicked, m_controller, &WindowManagerController::moveToMonitor1);
    connect(m_monitor2Button, &QPushButton::clicked, m_controller, &WindowManagerController::moveToMonitor2);
    connect(m_stabilizerButton, &QPushButton::clicked, m_controller, &WindowManagerController::toggleStabilizer);

    connect(m_applyHOverscanButton, &QPushButton::clicked, this, [this]() {
        m_controller->applyHorizontalOverscan(m_overscanSpin->value());
    });
    connect(m_applyVOverscanButton, &QPushButton::clicked, this, [this]() {
        m_controller->applyVerticalOverscan(m_overscanSpin->value());
    });

    connect(m_nudgeLeftButton, &QPushButton::clicked, this, [this]() {
        m_controller->nudgeLeft(m_stepSpin->value());
    });
    connect(m_nudgeRightButton, &QPushButton::clicked, this, [this]() {
        m_controller->nudgeRight(m_stepSpin->value());
    });
    connect(m_nudgeUpButton, &QPushButton::clicked, this, [this]() {
        m_controller->nudgeUp(m_stepSpin->value());
    });
    connect(m_nudgeDownButton, &QPushButton::clicked, this, [this]() {
        m_controller->nudgeDown(m_stepSpin->value());
    });

    connect(m_growWButton, &QPushButton::clicked, this, [this]() {
        m_controller->growWidth(m_stepSpin->value());
    });
    connect(m_shrinkWButton, &QPushButton::clicked, this, [this]() {
        m_controller->shrinkWidth(m_stepSpin->value());
    });
    connect(m_growHButton, &QPushButton::clicked, this, [this]() {
        m_controller->growHeight(m_stepSpin->value());
    });
    connect(m_shrinkHButton, &QPushButton::clicked, this, [this]() {
        m_controller->shrinkHeight(m_stepSpin->value());
    });

    connect(m_setSizeButton, &QPushButton::clicked, this, [this]() {
        m_controller->setCustomSize(m_widthSpin->value(), m_heightSpin->value());
    });

    connect(m_preset1080Button, &QPushButton::clicked, this, [this]() {
        m_controller->setCustomSize(1920, 1080);
    });
    connect(m_preset1440Button, &QPushButton::clicked, this, [this]() {
        m_controller->setCustomSize(2560, 1440);
    });
    connect(m_preset4kButton, &QPushButton::clicked, this, [this]() {
        m_controller->setCustomSize(3840, 2160);
    });

    connect(m_windowTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex&) {
        const int row = current.row();
        if (row < 0) return;
        const auto& wins = m_controller->scannedWindows();
        if (row >= static_cast<int>(wins.size())) return;
        m_controller->setActiveWindow(wins[static_cast<std::size_t>(row)].hwnd);
    });

    connect(m_controller, &WindowManagerController::statusChanged, this, &MainWindow::refreshStatus);

    // Auto-scan on startup if a target exe was previously saved
    if (!m_controller->targetExecutable().isEmpty()) {
        m_controller->scanWindows();
    }
}

void MainWindow::buildUi()
{
    setWindowTitle("Window Manager");
    resize(1040, 720);

    auto *centralWidget = new QWidget(this);
    auto *layout = new QVBoxLayout(centralWidget);

    auto *heading = new QLabel("Window Manager Migration Shell", centralWidget);
    heading->setObjectName("headingLabel");

    auto *description = new QLabel(
        "Select an executable, scan for matching windows, then apply or save profile settings.",
        centralWidget);
    description->setWordWrap(true);

    m_targetExeLabel = new QLabel("Target EXE: <none>", centralWidget);
    m_targetExeLabel->setWordWrap(true);

    m_statusLabel = new QLabel(centralWidget);
    m_statusLabel->setWordWrap(true);

    m_windowTable = new QTableWidget(centralWidget);
    m_windowTable->setColumnCount(8);
    m_windowTable->setHorizontalHeaderLabels({"HWND", "Title", "Class", "PID", "X", "Y", "W", "H"});
    m_windowTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_windowTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_windowTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_windowTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_windowTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_windowTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_windowTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_windowTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_windowTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_windowTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // Row 1: EXE picker + Launch + Kill
    auto *row1 = new QHBoxLayout();
    m_selectExeButton  = new QPushButton("Select Executable", centralWidget);
    m_launchButton     = new QPushButton("Launch", centralWidget);
    m_killButton       = new QPushButton("Kill", centralWidget);
    row1->addWidget(m_selectExeButton);
    row1->addWidget(m_launchButton);
    row1->addWidget(m_killButton);
    row1->addStretch();

    // Row 2: Scan + Profile
    auto *row2 = new QHBoxLayout();
    m_scanButton        = new QPushButton("Refresh List", centralWidget);
    m_applySavedButton  = new QPushButton("Force Saved Settings", centralWidget);
    m_savePositionButton = new QPushButton("Save Layout", centralWidget);
    row2->addWidget(m_scanButton);
    row2->addWidget(m_applySavedButton);
    row2->addWidget(m_savePositionButton);
    row2->addStretch();

    // Row 3: Monitor + Display layout
    auto *row3 = new QHBoxLayout();
    m_monitor1Button   = new QPushButton("Monitor 01", centralWidget);
    m_monitor2Button   = new QPushButton("Monitor 02", centralWidget);
    m_fitScreenButton  = new QPushButton("Fit Screen", centralWidget);
    m_borderlessButton = new QPushButton("True Borderless", centralWidget);
    m_windowedButton   = new QPushButton("Windowed", centralWidget);
    m_stabilizerButton = new QPushButton("Enable Stabilizer", centralWidget);
    row3->addWidget(m_monitor1Button);
    row3->addWidget(m_monitor2Button);
    row3->addWidget(m_fitScreenButton);
    row3->addWidget(m_borderlessButton);
    row3->addWidget(m_windowedButton);
    row3->addStretch();
    row3->addWidget(m_stabilizerButton);

    // Row 4: Window state controls
    auto *row4 = new QHBoxLayout();
    m_showButton      = new QPushButton("Show", centralWidget);
    m_hideButton      = new QPushButton("Hidden", centralWidget);
    m_minimizeButton  = new QPushButton("Minimized", centralWidget);
    m_maximizeButton  = new QPushButton("Maximized", centralWidget);
    m_restoreButton   = new QPushButton("Restore", centralWidget);
    m_destroyButton   = new QPushButton("Destroy", centralWidget);
    m_topmostButton   = new QPushButton("Topmost", centralWidget);
    m_toolWindowButton = new QPushButton("Tool Window", centralWidget);
    m_layeredButton   = new QPushButton("Layered", centralWidget);
    m_noActivateButton = new QPushButton("No Activate", centralWidget);
    m_resetAllButton  = new QPushButton("Reset All", centralWidget);
    row4->addWidget(m_showButton);
    row4->addWidget(m_hideButton);
    row4->addWidget(m_minimizeButton);
    row4->addWidget(m_maximizeButton);
    row4->addWidget(m_restoreButton);
    row4->addWidget(m_destroyButton);
    row4->addStretch();
    row4->addWidget(m_topmostButton);
    row4->addWidget(m_toolWindowButton);
    row4->addWidget(m_layeredButton);
    row4->addWidget(m_noActivateButton);
    row4->addWidget(m_resetAllButton);

    // Row 5: Overscan + Nudge
    m_overscanSpin = new QSpinBox(centralWidget);
    m_overscanSpin->setRange(0, 500);
    m_overscanSpin->setValue(0);
    m_stepSpin = new QSpinBox(centralWidget);
    m_stepSpin->setRange(1, 100);
    m_stepSpin->setValue(5);
    m_applyHOverscanButton = new QPushButton("H-Overscan", centralWidget);
    m_applyVOverscanButton = new QPushButton("V-Overscan", centralWidget);
    m_nudgeUpButton    = new QPushButton("U", centralWidget);
    m_nudgeDownButton  = new QPushButton("D", centralWidget);
    m_nudgeLeftButton  = new QPushButton("L", centralWidget);
    m_nudgeRightButton = new QPushButton("R", centralWidget);
    auto *row5 = new QHBoxLayout();
    row5->addWidget(m_applyHOverscanButton);
    row5->addWidget(m_applyVOverscanButton);
    row5->addWidget(new QLabel("px:", centralWidget));
    row5->addWidget(m_overscanSpin);
    row5->addStretch();
    row5->addWidget(m_nudgeUpButton);
    row5->addWidget(m_nudgeDownButton);
    row5->addWidget(m_nudgeLeftButton);
    row5->addWidget(m_nudgeRightButton);
    row5->addWidget(new QLabel("Step:", centralWidget));
    row5->addWidget(m_stepSpin);

    // Row 6: W/H resize + custom size + save
    m_growWButton  = new QPushButton("W ++", centralWidget);
    m_shrinkWButton = new QPushButton("W --", centralWidget);
    m_growHButton  = new QPushButton("H ++", centralWidget);
    m_shrinkHButton = new QPushButton("H --", centralWidget);
    m_widthSpin  = new QSpinBox(centralWidget);
    m_heightSpin = new QSpinBox(centralWidget);
    m_widthSpin->setRange(100, 10000);
    m_heightSpin->setRange(100, 10000);
    m_widthSpin->setValue(1920);
    m_heightSpin->setValue(1080);
    m_setSizeButton = new QPushButton("Set Size", centralWidget);
    auto *row6 = new QHBoxLayout();
    row6->addWidget(m_growWButton);
    row6->addWidget(m_shrinkWButton);
    row6->addWidget(m_growHButton);
    row6->addWidget(m_shrinkHButton);
    row6->addStretch();
    row6->addWidget(new QLabel("W:", centralWidget));
    row6->addWidget(m_widthSpin);
    row6->addWidget(new QLabel("H:", centralWidget));
    row6->addWidget(m_heightSpin);
    row6->addWidget(m_setSizeButton);

    // Row 7: Resolution presets
    m_preset1080Button = new QPushButton("1920x1080", centralWidget);
    m_preset1440Button = new QPushButton("2560x1440", centralWidget);
    m_preset4kButton   = new QPushButton("3840x2160", centralWidget);
    auto *row7 = new QHBoxLayout();
    row7->addWidget(new QLabel("Presets:", centralWidget));
    row7->addWidget(m_preset1080Button);
    row7->addWidget(m_preset1440Button);
    row7->addWidget(m_preset4kButton);
    row7->addStretch();

    layout->addWidget(heading);
    layout->addWidget(description);
    layout->addWidget(m_targetExeLabel);
    layout->addWidget(m_statusLabel);
    layout->addWidget(m_windowTable);
    layout->addLayout(row1);
    layout->addLayout(row2);
    layout->addLayout(row3);
    layout->addLayout(row4);
    layout->addLayout(row5);
    layout->addLayout(row6);
    layout->addLayout(row7);

    setCentralWidget(centralWidget);
    statusBar()->showMessage("Ready.");
}

void MainWindow::refreshStatus()
{
    m_statusLabel->setText(m_controller->statusText());
    m_stabilizerButton->setText(m_controller->stabilizerEnabled() ? "Disable Stabilizer" : "Enable Stabilizer");
    const QString exe = m_controller->targetExecutable();
    m_targetExeLabel->setText("Target EXE: " + (exe.isEmpty() ? QString("<none>") : exe));
    refreshWindowTable();
}

void MainWindow::refreshWindowTable()
{
    const auto& windows = m_controller->scannedWindows();
    const quintptr activeHwnd = m_controller->activeWindow();

    // Block signals while rebuilding rows so selection changes don't trigger setActiveWindow
    m_windowTable->blockSignals(true);
    m_windowTable->setRowCount(static_cast<int>(windows.size()));

    int selectRow = -1;
    for (int i = 0; i < static_cast<int>(windows.size()); ++i) {
        const auto& win = windows[static_cast<std::size_t>(i)];
        m_windowTable->setItem(i, 0, new QTableWidgetItem(QString::number(win.hwnd)));
        m_windowTable->setItem(i, 1, new QTableWidgetItem(win.title));
        m_windowTable->setItem(i, 2, new QTableWidgetItem(win.className));
        m_windowTable->setItem(i, 3, new QTableWidgetItem(QString::number(win.pid)));
        m_windowTable->setItem(i, 4, new QTableWidgetItem(QString::number(win.x)));
        m_windowTable->setItem(i, 5, new QTableWidgetItem(QString::number(win.y)));
        m_windowTable->setItem(i, 6, new QTableWidgetItem(QString::number(win.w)));
        m_windowTable->setItem(i, 7, new QTableWidgetItem(QString::number(win.h)));
        if (win.hwnd == activeHwnd) {
            selectRow = i;
        }
    }

    m_windowTable->blockSignals(false);

    if (selectRow >= 0) {
        m_windowTable->selectRow(selectRow);
    }
    connect(m_launchButton,      &QPushButton::clicked, m_controller, &WindowManagerController::launchTarget);
    connect(m_killButton,        &QPushButton::clicked, m_controller, &WindowManagerController::killTarget);
    connect(m_fitScreenButton,   &QPushButton::clicked, m_controller, &WindowManagerController::fitToScreen);
    connect(m_borderlessButton,  &QPushButton::clicked, m_controller, &WindowManagerController::setBorderlessFullscreen);
    connect(m_windowedButton,    &QPushButton::clicked, m_controller, &WindowManagerController::setWindowed);
    connect(m_showButton,        &QPushButton::clicked, m_controller, &WindowManagerController::showWindow);
    connect(m_hideButton,        &QPushButton::clicked, m_controller, &WindowManagerController::hideWindow);
    connect(m_minimizeButton,    &QPushButton::clicked, m_controller, &WindowManagerController::minimizeWindow);
    connect(m_maximizeButton,    &QPushButton::clicked, m_controller, &WindowManagerController::maximizeWindow);
    connect(m_restoreButton,     &QPushButton::clicked, m_controller, &WindowManagerController::restoreWindow);
    connect(m_destroyButton,     &QPushButton::clicked, m_controller, &WindowManagerController::destroyWindow);
    connect(m_topmostButton,     &QPushButton::clicked, m_controller, &WindowManagerController::toggleTopmost);
    connect(m_toolWindowButton,  &QPushButton::clicked, m_controller, &WindowManagerController::toggleToolWindow);
    connect(m_layeredButton,     &QPushButton::clicked, m_controller, &WindowManagerController::toggleLayered);
    connect(m_noActivateButton,  &QPushButton::clicked, m_controller, &WindowManagerController::toggleNoActivate);
    connect(m_resetAllButton,    &QPushButton::clicked, m_controller, &WindowManagerController::resetAll);
}