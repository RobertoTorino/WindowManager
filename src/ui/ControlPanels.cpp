#include "ControlPanels.h"

#include "ScaledView.h"
#include "SnapshotCapture.h"
#include "Theme.h"
#include "WindowManagerController.h"
#include "window/MonitorHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {

QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    box->setLayout(layout);
    return box;
}

QSpinBox* spin(int min, int max, int value, const QString& suffix, QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setValue(value);
    s->setSuffix(suffix);
    s->setAlignment(Qt::AlignCenter);
    s->setKeyboardTracking(false);
    return s;
}

// Page layout: groups laid out in a grid, remaining space left empty.
QGridLayout* pageLayout(QWidget* page)
{
    auto* grid = new QGridLayout(page);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setSpacing(10);
    return grid;
}

} // namespace

// ---------------------------------------------------------------------------
// StatePanel
// ---------------------------------------------------------------------------

StatePanel::StatePanel(WindowManagerController* c, QWidget* parent)
    : ControlPanel(c, parent)
{
    m_show     = Theme::button("Show", this);
    m_hide     = Theme::button("Hide", this);
    m_minimize = Theme::button("Minimize", this);
    m_maximize = Theme::button("Maximize", this);
    auto* restore = Theme::button("Restore", this);

    m_windowed   = Theme::button("Windowed", this, nullptr, "Bordered, resizable window");
    m_borderless = Theme::button("True Borderless FS", this, nullptr,
                                 "Remove borders and cover the whole monitor");
    auto* fit = Theme::button("Fit Screen", this, nullptr, "Fill the monitor's work area (excludes taskbar)");
    auto* fitMonitor = Theme::button("Fit to monitor...", this, "accent",
                                     "Measure the window and fit its picture exactly to a monitor");

    m_topmost    = Theme::button("Topmost", this, nullptr, "Keep above all other windows");
    m_toolWindow = Theme::button("Tool Window", this, nullptr, "Hide from taskbar / Alt+Tab");
    m_layered    = Theme::button("Layered", this, nullptr, "Allow transparency");
    m_noActivate = Theme::button("No Activate", this, nullptr, "Never take focus when shown");

    auto* vis = new QHBoxLayout;
    for (auto* b : {m_show, m_hide, m_minimize, m_maximize, restore})
        vis->addWidget(b);
    auto* mode = new QHBoxLayout;
    for (auto* b : {m_windowed, m_borderless, fit, fitMonitor})
        mode->addWidget(b);
    auto* styles = new QHBoxLayout;
    for (auto* b : {m_topmost, m_toolWindow, m_layered, m_noActivate})
        styles->addWidget(b);

    auto* grid = pageLayout(this);
    grid->addWidget(group("Visibility", vis, this), 0, 0);
    grid->addWidget(group("Window mode", mode, this), 0, 1);
    grid->addWidget(group("Extended styles (toggle)", styles, this), 1, 0, 1, 2);
    grid->setRowStretch(2, 1);

    connect(m_show,     &QPushButton::clicked, c, &WindowManagerController::showWindow);
    connect(m_hide,     &QPushButton::clicked, c, &WindowManagerController::hideWindow);
    connect(m_minimize, &QPushButton::clicked, c, &WindowManagerController::minimizeWindow);
    connect(m_maximize, &QPushButton::clicked, c, &WindowManagerController::maximizeWindow);
    connect(restore,    &QPushButton::clicked, c, &WindowManagerController::restoreWindow);
    connect(m_windowed,   &QPushButton::clicked, c, &WindowManagerController::setWindowed);
    connect(m_borderless, &QPushButton::clicked, c, &WindowManagerController::setBorderlessFullscreen);
    connect(fit,          &QPushButton::clicked, c, &WindowManagerController::fitToScreen);
    connect(fitMonitor,   &QPushButton::clicked, c, &WindowManagerController::requestFitCheck);
    connect(m_topmost,    &QPushButton::clicked, c, &WindowManagerController::toggleTopmost);
    connect(m_toolWindow, &QPushButton::clicked, c, &WindowManagerController::toggleToolWindow);
    connect(m_layered,    &QPushButton::clicked, c, &WindowManagerController::toggleLayered);
    connect(m_noActivate, &QPushButton::clicked, c, &WindowManagerController::toggleNoActivate);
}

void StatePanel::refresh()
{
    const bool has = m_controller->activeWindow() != 0;
    const auto vis   = m_controller->queryActiveWindowVisibilityState();
    const auto style = m_controller->queryActiveWindowStyleState();

    Theme::setActive(m_show,       has && vis.visible);
    Theme::setActive(m_hide,       has && !vis.visible);
    Theme::setActive(m_minimize,   has && vis.minimized);
    Theme::setActive(m_maximize,   has && vis.maximized);
    Theme::setActive(m_windowed,   has && vis.windowed);
    Theme::setActive(m_borderless, has && !vis.windowed);
    Theme::setActive(m_topmost,    has && style.topmost);
    Theme::setActive(m_toolWindow, has && style.toolWindow);
    Theme::setActive(m_layered,    has && style.layered);
    Theme::setActive(m_noActivate, has && style.noActivate);
}

// ---------------------------------------------------------------------------
// PositionPanel
// ---------------------------------------------------------------------------

PositionPanel::PositionPanel(WindowManagerController* c, QWidget* parent)
    : ControlPanel(c, parent)
{
    m_step     = spin(1, 500, 1, " px", this);
    m_width    = spin(100, 10000, 1920, " px", this);
    m_height   = spin(100, 10000, 1080, " px", this);
    m_overscan = spin(0, 500, 0, " px", this);
    m_step->setToolTip("Distance used by the move and resize buttons");

    // Move: arrow pad + step
    auto* up    = Theme::button(QString::fromUtf8("\xE2\x96\xB2"), this, nullptr, "Move up");
    auto* down  = Theme::button(QString::fromUtf8("\xE2\x96\xBC"), this, nullptr, "Move down");
    auto* left  = Theme::button(QString::fromUtf8("\xE2\x97\x80"), this, nullptr, "Move left");
    auto* right = Theme::button(QString::fromUtf8("\xE2\x96\xB6"), this, nullptr, "Move right");
    auto* pad = new QGridLayout;
    pad->addWidget(up, 0, 1);
    pad->addWidget(left, 1, 0);
    pad->addWidget(right, 1, 2);
    pad->addWidget(down, 2, 1);
    pad->addWidget(new QLabel("Step", this), 0, 3);
    pad->addWidget(m_step, 1, 3);
    pad->setColumnStretch(4, 1);

    // Resize
    auto* wPlus  = Theme::button("Width +",  this, nullptr, "Grow width by step (centred)");
    auto* wMinus = Theme::button("Width -",  this, nullptr, "Shrink width by step (centred)");
    auto* hPlus  = Theme::button("Height +", this, nullptr, "Grow height by step (centred)");
    auto* hMinus = Theme::button("Height -", this, nullptr, "Shrink height by step (centred)");
    auto* resize = new QGridLayout;
    resize->addWidget(wPlus, 0, 0);
    resize->addWidget(wMinus, 0, 1);
    resize->addWidget(hPlus, 1, 0);
    resize->addWidget(hMinus, 1, 1);

    // Exact size
    auto* set = Theme::button("Set size", this, "accent");
    auto* exact = new QGridLayout;
    exact->addWidget(new QLabel("Width", this), 0, 0);
    exact->addWidget(m_width, 0, 1);
    exact->addWidget(new QLabel("Height", this), 1, 0);
    exact->addWidget(m_height, 1, 1);
    exact->addWidget(set, 0, 2, 2, 1);

    // Overscan
    auto* hOver = Theme::button("Horizontal", this, nullptr, "Shrink width by 2x amount, centred on monitor 1");
    auto* vOver = Theme::button("Vertical", this, nullptr, "Shrink height by 2x amount, centred on monitor 1");
    auto* bOver = Theme::button("Both", this);
    auto* over = new QHBoxLayout;
    over->addWidget(m_overscan);
    over->addWidget(hOver);
    over->addWidget(vOver);
    over->addWidget(bOver);

    // Profile
    auto* save  = Theme::button("Save position", this, nullptr, "Remember the current rectangle for this game");
    auto* apply = Theme::button("Force saved settings", this, nullptr, "Re-apply the remembered rectangle");
    auto* profile = new QHBoxLayout;
    profile->addWidget(save);
    profile->addWidget(apply);

    auto* grid = pageLayout(this);
    grid->addWidget(group("Move", pad, this), 0, 0);
    grid->addWidget(group("Resize (uses step)", resize, this), 0, 1);
    grid->addWidget(group("Exact size", exact, this), 0, 2);
    grid->addWidget(group("Overscan", over, this), 1, 0, 1, 2);
    grid->addWidget(group("Saved profile", profile, this), 1, 2);
    grid->setRowStretch(2, 1);

    connect(up,    &QPushButton::clicked, this, [this, c]() { c->nudgeUp(m_step->value()); });
    connect(down,  &QPushButton::clicked, this, [this, c]() { c->nudgeDown(m_step->value()); });
    connect(left,  &QPushButton::clicked, this, [this, c]() { c->nudgeLeft(m_step->value()); });
    connect(right, &QPushButton::clicked, this, [this, c]() { c->nudgeRight(m_step->value()); });
    connect(wPlus,  &QPushButton::clicked, this, [this, c]() { c->growWidth(m_step->value()); });
    connect(wMinus, &QPushButton::clicked, this, [this, c]() { c->shrinkWidth(m_step->value()); });
    connect(hPlus,  &QPushButton::clicked, this, [this, c]() { c->growHeight(m_step->value()); });
    connect(hMinus, &QPushButton::clicked, this, [this, c]() { c->shrinkHeight(m_step->value()); });
    connect(set, &QPushButton::clicked, this, [this, c]() { c->setCustomSize(m_width->value(), m_height->value()); });
    connect(hOver, &QPushButton::clicked, this, [this, c]() { c->applyHorizontalOverscan(m_overscan->value()); });
    connect(vOver, &QPushButton::clicked, this, [this, c]() { c->applyVerticalOverscan(m_overscan->value()); });
    connect(bOver, &QPushButton::clicked, this, [this, c]() {
        c->applyHorizontalOverscan(m_overscan->value());
        c->applyVerticalOverscan(m_overscan->value());
    });
    connect(save,  &QPushButton::clicked, c, &WindowManagerController::saveCurrentPosition);
    connect(apply, &QPushButton::clicked, c, &WindowManagerController::applySavedSettings);
}

// ---------------------------------------------------------------------------
// PresetPanel
// ---------------------------------------------------------------------------

PresetPanel::PresetPanel(WindowManagerController* c, QWidget* parent)
    : ControlPanel(c, parent)
{
    struct Preset { int w, h; const char* note; };
    static const Preset common[] = {
        {1920, 1080, "Full HD 16:9"},  {1920, 1200, "16:10"},         {1920, 1440, "4:3"},
        {2048, 1152, "16:9"},          {2048, 1536, "4:3"},           {2560, 1440, "QHD 16:9"},
        {2560, 1600, "16:10"},         {2880, 1800, "16:10"},         {3840, 2160, "4K UHD 16:9"},
    };
    static const Preset high[] = {
        {4096, 2160, "DCI 4K"}, {5120, 2880, "5K"}, {6016, 3384, "6K"}, {7680, 4320, "8K UHD"},
    };

    auto fill = [&](QGridLayout* grid, const Preset* list, int count, int columns) {
        for (int i = 0; i < count; ++i) {
            const Preset p = list[i];
            auto* b = Theme::button(QString("%1 x %2").arg(p.w).arg(p.h), this, nullptr,
                                    QString("%1 - sets the window size, keeps its position").arg(p.note));
            connect(b, &QPushButton::clicked, this, [c, p]() { c->setCustomSize(p.w, p.h); });
            grid->addWidget(b, i / columns, i % columns);
        }
    };

    auto* commonGrid = new QGridLayout;
    fill(commonGrid, common, static_cast<int>(std::size(common)), 3);
    auto* highGrid = new QGridLayout;
    fill(highGrid, high, static_cast<int>(std::size(high)), 2);

    auto* grid = pageLayout(this);
    grid->addWidget(group("Common", commonGrid, this), 0, 0);
    grid->addWidget(group("High resolution", highGrid, this), 0, 1);
    grid->setColumnStretch(0, 3);
    grid->setColumnStretch(1, 2);
    grid->setRowStretch(1, 1);
}

// ---------------------------------------------------------------------------
// CapturePanel
// ---------------------------------------------------------------------------

CapturePanel::CapturePanel(WindowManagerController* c, SnapshotCapture* capture, QWidget* parent)
    : ControlPanel(c, parent)
{
    auto* count = spin(1, 99, 1, " shots", this);
    m_burst = Theme::button("Capture", this, "accent", "Screenshot the active window");
    auto* open = Theme::button("Open folder", this, nullptr, "Show this game's snapshot folder");
    auto* hint = new QLabel("Shots are 80 ms apart and saved as PNG under snapshots/<game>.", this);
    hint->setObjectName("hint");

    auto* row = new QHBoxLayout;
    row->addWidget(count);
    row->addWidget(m_burst);
    row->addWidget(open);
    row->addStretch(1);
    auto* inner = new QVBoxLayout;
    inner->addLayout(row);
    inner->addWidget(hint);

    auto* grid = pageLayout(this);
    grid->addWidget(group("Snapshots", inner, this), 0, 0);
    grid->setColumnStretch(1, 1);
    grid->setRowStretch(1, 1);

    connect(m_burst, &QPushButton::clicked, this, [count, capture]() { capture->start(count->value()); });
    connect(open, &QPushButton::clicked, capture, &SnapshotCapture::openFolder);
    connect(capture, &SnapshotCapture::runningChanged, this, [this](bool running) {
        m_burst->setEnabled(!running);
    });
}

// ---------------------------------------------------------------------------
// DownscalePanel
// ---------------------------------------------------------------------------

DownscalePanel::DownscalePanel(WindowManagerController* c, ScaledView* view, QWidget* parent)
    : ControlPanel(c, parent)
    , m_view(view)
{
    m_monitor = new QComboBox(this);
    m_factor = new QComboBox(this);
    m_aspect = new QComboBox(this);

    m_factor->addItem("1.5x  (light)", 1.5);
    m_factor->addItem("2x  (sharper)", 2.0);
    m_factor->addItem("3x  (very sharp, heavy)", 3.0);
    m_factor->addItem("4x  (heaviest)", 4.0);
    m_factor->setCurrentIndex(1);

    m_aspect->addItem("The game's current shape", 0);
    m_aspect->addItem("The monitor's shape", 1);
    m_aspect->addItem("4 : 3", 2);
    m_aspect->addItem("16 : 9", 3);
    m_aspect->addItem("16 : 10", 4);

    m_preview = new QLabel(this);
    m_preview->setWordWrap(true);
    m_preview->setObjectName("fitDelta");

    auto* hint = new QLabel(
        "Makes the game render more pixels than the monitor has, then shows them scaled down "
        "so nothing falls off the screen. Keyboard and gamepad go to the game; the mouse is "
        "forwarded as window messages. Stop with Ctrl+Alt+D or the Stop button; closing the "
        "game also stops it. Needs a game that re-creates its picture at the window size "
        "(for example RPCS3 does).", this);
    hint->setObjectName("hint");
    hint->setWordWrap(true);

    m_forwardMouse = new QCheckBox("Forward the mouse to the game", this);
    m_forwardMouse->setChecked(true);
    m_forwardMouse->setToolTip("Mouse position and clicks on the picture are sent to the game.\n"
                               "Works for games that read the mouse from their window; games using raw input,\n"
                               "DirectInput or the global cursor position will not see it.");
    m_start = Theme::button("Start downscaled view", this, "accent");
    m_stop = Theme::button("Stop", this, "danger");

    auto* form = new QGridLayout;
    form->addWidget(new QLabel("Show on", this), 0, 0);
    form->addWidget(m_monitor, 0, 1);
    form->addWidget(new QLabel("Sharpness", this), 1, 0);
    form->addWidget(m_factor, 1, 1);
    form->addWidget(new QLabel("Picture shape", this), 2, 0);
    form->addWidget(m_aspect, 2, 1);
    form->addWidget(m_forwardMouse, 3, 1);
    form->setColumnStretch(1, 1);

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_start);
    buttons->addWidget(m_stop);
    buttons->addStretch(1);

    auto* inner = new QVBoxLayout;
    inner->addLayout(form);
    inner->addWidget(m_preview);
    inner->addLayout(buttons);
    inner->addWidget(hint);

    auto* grid = pageLayout(this);
    grid->addWidget(group("Downscaled view (supersampling)", inner, this), 0, 0);
    grid->setColumnStretch(0, 1);

    auto update = [this]() { updatePreview(); };
    connect(m_monitor, qOverload<int>(&QComboBox::currentIndexChanged), this, update);
    connect(m_factor, qOverload<int>(&QComboBox::currentIndexChanged), this, update);
    connect(m_aspect, qOverload<int>(&QComboBox::currentIndexChanged), this, update);
    connect(m_start, &QPushButton::clicked, this, &DownscalePanel::startView);
    connect(m_stop, &QPushButton::clicked, this, [this]() { m_view->stop(); });
    connect(m_view, &ScaledView::runningChanged, this, update);
}

bool DownscalePanel::plan(Supersample::Size& render, Fit::Rect& monitor,
                          int& monitorIndex, quintptr& hwnd)
{
    WindowManager::FitMeasurement m;
    WindowManager::FitRequest request;
    request.monitorIndex = m_monitor->currentData().toInt();
    if (!m_controller->measureFit(request, m)) {
        m_error = m.error;
        return false;
    }

    int aw = m.client.width();
    int ah = m.client.height();
    switch (m_aspect->currentData().toInt()) {
    case 1: aw = m.monitor.width(); ah = m.monitor.height(); break;
    case 2: aw = 4;  ah = 3;  break;
    case 3: aw = 16; ah = 9;  break;
    case 4: aw = 16; ah = 10; break;
    default: break;
    }
    if (aw <= 0 || ah <= 0) {
        aw = m.monitor.width();
        ah = m.monitor.height();
    }

    render = Supersample::renderSize(aw, ah, m.monitor.width(), m.monitor.height(),
                                     m_factor->currentData().toDouble());
    monitor = m.monitor;
    monitorIndex = m.monitorIndex;
    hwnd = m.hwnd;
    return true;
}

void DownscalePanel::updatePreview()
{
    const bool running = m_view->isRunning();
    m_start->setEnabled(false);
    m_stop->setEnabled(running);
    m_monitor->setEnabled(!running);
    m_factor->setEnabled(!running);
    m_aspect->setEnabled(!running);
    m_forwardMouse->setEnabled(!running);

    if (running) {
        m_preview->setText(m_view->description());
        return;
    }
    if (!m_controller->activeWindow()) {
        m_preview->setText("Select the game window first (Scan).");
        return;
    }

    Supersample::Size render;
    Fit::Rect monitor;
    int monitorIndex = 0;
    quintptr hwnd = 0;
    if (!plan(render, monitor, monitorIndex, hwnd)) {
        m_preview->setText(m_error);
        return;
    }
    const auto shown = Supersample::letterbox(render.w, render.h, monitor.width(), monitor.height());
    m_preview->setText(QString("The game will render %1 x %2, shown as %3 x %4 on monitor %5.")
                           .arg(render.w).arg(render.h).arg(shown.width()).arg(shown.height())
                           .arg(monitorIndex));
    m_start->setEnabled(render.w <= 16384 && render.h <= 16384);
}

void DownscalePanel::startView()
{
    Supersample::Size render;
    Fit::Rect monitor;
    int monitorIndex = 0;
    quintptr hwnd = 0;
    if (!plan(render, monitor, monitorIndex, hwnd)) {
        m_preview->setText(m_error);
        return;
    }

    // The stabilizer would drag the window back to where it was.
    if (m_controller->stabilizerEnabled())
        m_controller->toggleStabilizer();

    ScaledView::Options o;
    o.gameHwnd = hwnd;
    o.monitor = monitor;
    o.monitorIndex = monitorIndex;
    o.renderW = render.w;
    o.renderH = render.h;
    o.forwardMouse = m_forwardMouse->isChecked();
    QString error;
    if (!m_view->start(o, &error))
        m_preview->setText(error);
}

void DownscalePanel::refresh()
{
    // Rebuild the monitor list if the number of monitors changed.
    const auto monitors = MonitorHelper::enumerateMonitors();
    if (m_monitor->count() != static_cast<int>(monitors.size()) + 1) {
        QSignalBlocker block(m_monitor);
        const int keep = m_monitor->currentIndex();
        m_monitor->clear();
        m_monitor->addItem("The monitor the game is on", 0);
        for (const auto& m : monitors)
            m_monitor->addItem(QString("Monitor %1  (%2 x %3)").arg(m.index).arg(m.w).arg(m.h), m.index);
        m_monitor->setCurrentIndex(qBound(0, keep, m_monitor->count() - 1));
    }
    if (!m_view->isRunning())
        updatePreview();
}
