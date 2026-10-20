#include "DisplayPanel.h"

#include "Theme.h"
#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "display/DisplayBackend.h"
#include "display/DisplaySession.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr int kKeepSeconds = 15;

QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    box->setLayout(layout);
    return box;
}

QLabel* label(const QString& text, QWidget* parent)
{
    return new QLabel(text, parent);
}

QString sizeText(DisplayModel::Size s)
{
    return QString("%1 x %2").arg(s.width).arg(s.height);
}

QString monitorText(const DisplayModel::Monitor& m, int number)
{
    QString text = QString("%1: %2").arg(number).arg(m.name);
    if (m.primary)
        text += "  (main display)";
    return text;
}

// Selects the item whose data equals value; returns false if there is none.
bool selectData(QComboBox* combo, const QVariant& value)
{
    const int index = combo->findData(value);
    if (index < 0)
        return false;
    combo->setCurrentIndex(index);
    return true;
}

} // namespace

DisplayPanel::DisplayPanel(WindowManagerController* controller, DisplaySession* session, QWidget* parent)
    : ControlPanel(controller, parent)
    , m_session(session)
    , m_content(new QWidget(this))
    , m_brightnessTimer(new QTimer(this))
{
    m_unsupported = new QLabel("Display settings are not available on this system yet (Windows only).", this);
    m_unsupported->setObjectName("hint");
    m_unsupported->setWordWrap(true);

    // ----- This PC's displays ---------------------------------------------------
    QWidget* c = m_content;
    m_monitor     = new QComboBox(c);
    m_resolution  = new QComboBox(c);
    m_refresh     = new QComboBox(c);
    m_orientation = new QComboBox(c);
    m_scale       = new QComboBox(c);
    m_profile     = new QComboBox(c);
    m_topology    = new QComboBox(c);
    m_applyMode   = Theme::button("Apply", c, "accent", "Set this resolution, refresh rate and orientation");
    m_hdr         = Theme::button("HDR", c, nullptr, "Switch HDR on or off for this display");
    m_reload      = Theme::button("Re-read", c, nullptr, "Read the current settings from Windows again");
    m_nightLight  = Theme::button("Night light...", c, nullptr, "Open the night light page in Windows Settings");
    m_brightness  = new QSlider(Qt::Horizontal, c);
    m_brightness->setRange(0, 100);
    m_brightnessValue = new QLabel(c);
    m_brightnessValue->setMinimumWidth(40);
    m_brightnessTimer->setSingleShot(true);
    m_brightnessTimer->setInterval(250);   // a monitor takes ~50 ms per DDC/CI command: don't send every step

    for (auto o : {DisplayModel::Orientation::Landscape, DisplayModel::Orientation::Portrait,
                   DisplayModel::Orientation::LandscapeFlipped, DisplayModel::Orientation::PortraitFlipped})
        m_orientation->addItem(DisplayModel::orientationName(o), static_cast<int>(o));
    for (auto t : {DisplayModel::Topology::Internal, DisplayModel::Topology::Clone, DisplayModel::Topology::Extend,
                   DisplayModel::Topology::External})
        m_topology->addItem(DisplayModel::topologyName(t), static_cast<int>(t));
    m_profile->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_profile->setMinimumContentsLength(16);
    m_monitor->setToolTip("The display these settings are for");
    m_scale->setToolTip("Size of text, apps and other items (applied at once)");
    m_profile->setToolTip("The colour profile Windows uses for this display (applied at once).\n"
                          "Add profiles under Settings > Display > Color profile.");
    m_topology->setToolTip("How Windows uses more than one display (like Win+P)");

    auto* brightness = new QHBoxLayout;
    brightness->setContentsMargins(0, 0, 0, 0);
    brightness->addWidget(m_brightness, 1);
    brightness->addWidget(m_brightnessValue);

    auto* pc = new QGridLayout;
    pc->addWidget(label("Monitor", c), 0, 0);
    pc->addWidget(m_monitor, 0, 1, 1, 5);
    pc->addWidget(m_reload, 0, 6);
    pc->addWidget(label("Resolution", c), 1, 0);
    pc->addWidget(m_resolution, 1, 1);
    pc->addWidget(label("Refresh rate", c), 1, 2);
    pc->addWidget(m_refresh, 1, 3);
    pc->addWidget(label("Orientation", c), 1, 4);
    pc->addWidget(m_orientation, 1, 5);
    pc->addWidget(m_applyMode, 1, 6);
    pc->addWidget(label("Scale", c), 2, 0);
    pc->addWidget(m_scale, 2, 1);
    pc->addWidget(label("HDR", c), 2, 2);
    pc->addWidget(m_hdr, 2, 3);
    pc->addWidget(label("Brightness", c), 2, 4);
    pc->addLayout(brightness, 2, 5, 1, 2);
    pc->addWidget(label("Color profile", c), 3, 0);
    pc->addWidget(m_profile, 3, 1);
    pc->addWidget(label("Multiple displays", c), 3, 2);
    pc->addWidget(m_topology, 3, 3);
    pc->addWidget(m_nightLight, 3, 6);
    pc->setColumnStretch(1, 2);
    pc->setColumnStretch(3, 1);
    pc->setColumnStretch(5, 1);

    // ----- While the game runs -------------------------------------------------------
    m_gameEnabled    = new QCheckBox(c);
    m_gameMonitor    = new QComboBox(c);
    m_gameResolution = new QComboBox(c);
    m_gameRefresh    = new QComboBox(c);
    m_gameHdr        = new QComboBox(c);
    m_gameScale      = new QComboBox(c);
    m_gameBrightness = new QComboBox(c);
    m_gameStatus     = new QLabel(c);
    m_gameStatus->setObjectName("hint");
    m_gameStatus->setWordWrap(true);
    m_gameRestore = Theme::button("Restore now", c, nullptr, "Put the desktop's display settings back now");
    m_gameEnabled->setToolTip("Applied when the game is started from this app (Start / Restart),\n"
                              "undone when the game closes.");

    m_gameHdr->addItem("Keep", -1);
    m_gameHdr->addItem("On", 1);
    m_gameHdr->addItem("Off", 0);
    m_gameBrightness->addItem("Keep", -1);
    for (int p = 0; p <= 100; p += 10)
        m_gameBrightness->addItem(QString("%1 %").arg(p), p);

    auto* game = new QGridLayout;
    game->addWidget(m_gameEnabled, 0, 0, 1, 4);
    game->addWidget(label("Monitor", c), 0, 4);
    game->addWidget(m_gameMonitor, 0, 5, 1, 2);
    game->addWidget(label("Resolution", c), 1, 0);
    game->addWidget(m_gameResolution, 1, 1);
    game->addWidget(label("Refresh rate", c), 1, 2);
    game->addWidget(m_gameRefresh, 1, 3);
    game->addWidget(label("HDR", c), 1, 4);
    game->addWidget(m_gameHdr, 1, 5);
    game->addWidget(label("Scale", c), 2, 0);
    game->addWidget(m_gameScale, 2, 1);
    game->addWidget(label("Brightness", c), 2, 2);
    game->addWidget(m_gameBrightness, 2, 3);
    game->addWidget(m_gameStatus, 3, 0, 1, 6);
    game->addWidget(m_gameRestore, 3, 6);
    game->setColumnStretch(1, 2);
    game->setColumnStretch(3, 1);
    game->setColumnStretch(5, 1);

    auto* content = new QVBoxLayout(m_content);
    content->setContentsMargins(0, 0, 0, 0);
    content->setSpacing(10);
    content->addWidget(group("This PC's displays", pc, c));
    content->addWidget(group("While the game runs", game, c));

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(10, 10, 10, 10);
    page->addWidget(m_unsupported);
    page->addWidget(m_content);

    const bool supported = displayBackend().supported();
    m_unsupported->setVisible(!supported);
    m_content->setVisible(supported);

    connect(m_monitor, qOverload<int>(&QComboBox::currentIndexChanged), this, &DisplayPanel::showMonitor);
    connect(m_orientation, qOverload<int>(&QComboBox::currentIndexChanged), this, &DisplayPanel::fillResolutions);
    connect(m_resolution, qOverload<int>(&QComboBox::currentIndexChanged), this, &DisplayPanel::fillRefreshRates);
    connect(m_applyMode, &QPushButton::clicked, this, &DisplayPanel::applyMode);
    connect(m_hdr, &QPushButton::clicked, this, &DisplayPanel::toggleHdr);
    connect(m_reload, &QPushButton::clicked, this, &DisplayPanel::reload);
    connect(m_scale, qOverload<int>(&QComboBox::activated), this, &DisplayPanel::applyScale);
    connect(m_profile, qOverload<int>(&QComboBox::activated), this, &DisplayPanel::applyColorProfile);
    connect(m_topology, qOverload<int>(&QComboBox::activated), this, &DisplayPanel::applyTopology);
    connect(m_brightness, &QSlider::valueChanged, this, [this](int value) {
        m_brightnessValue->setText(QString("%1 %").arg(value));
        if (!m_loading)
            m_brightnessTimer->start();
    });
    connect(m_brightnessTimer, &QTimer::timeout, this, &DisplayPanel::applyBrightness);
    connect(m_nightLight, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl(displayBackend().nightLightUrl()));
    });

    connect(m_gameEnabled, &QCheckBox::toggled, this, &DisplayPanel::saveGame);
    connect(m_gameMonitor, qOverload<int>(&QComboBox::activated), this, [this]() {
        fillGameChoices();
        saveGame();
    });
    connect(m_gameResolution, qOverload<int>(&QComboBox::activated), this, [this]() {
        fillGameRefreshRates();
        saveGame();
    });
    for (QComboBox* combo : {m_gameRefresh, m_gameHdr, m_gameScale, m_gameBrightness})
        connect(combo, qOverload<int>(&QComboBox::activated), this, &DisplayPanel::saveGame);
    connect(m_gameRestore, &QPushButton::clicked, this, [this]() { m_session->restore("Restore now"); });
    connect(m_session, &DisplaySession::changed, this, &DisplayPanel::updateGameStatus);
}

// ---------------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------------

void DisplayPanel::showEvent(QShowEvent* event)
{
    ControlPanel::showEvent(event);
    if (!event->spontaneous())   // not when the app window is merely restored
        reload();
}

void DisplayPanel::refresh()
{
    const QString exe = m_controller->targetExecutable();
    if (exe != m_gameExe) {
        m_gameExe = exe;
        loadGame();
    }
}

void DisplayPanel::reload()
{
    DisplayBackend& backend = displayBackend();
    if (!backend.supported())
        return;

    const QString keep = selected() ? selected()->id : QString();
    m_monitors = backend.monitors();
    {
        QSignalBlocker block(m_monitor);
        m_monitor->clear();
        for (size_t i = 0; i < m_monitors.size(); ++i)
            m_monitor->addItem(monitorText(m_monitors[i], static_cast<int>(i) + 1), m_monitors[i].id);
        const int index = DisplayModel::pickMonitor(m_monitors, keep);
        m_monitor->setCurrentIndex(index);
    }
    showMonitor();

    const bool several = backend.connectedDisplays() >= 2;
    {
        QSignalBlocker block(m_topology);
        if (!selectData(m_topology, static_cast<int>(backend.topology())))
            m_topology->setCurrentIndex(-1);
    }
    m_topology->setEnabled(several);
    m_topology->setToolTip(several ? QString("How Windows uses more than one display (like Win+P)")
                                   : QString("Only one display is connected."));
    m_nightLight->setEnabled(!backend.nightLightUrl().isEmpty());

    loadGame();
}

const DisplayModel::Monitor* DisplayPanel::selected() const
{
    const int index = m_monitor->currentIndex();
    if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        return nullptr;
    return &m_monitors[static_cast<size_t>(index)];
}

void DisplayPanel::showMonitor()
{
    const DisplayModel::Monitor* m = selected();
    m_loading = true;
    for (QWidget* w : std::initializer_list<QWidget*>{m_resolution, m_refresh, m_orientation, m_applyMode,
                                                      m_scale, m_hdr, m_brightness, m_profile})
        w->setEnabled(m != nullptr);
    if (!m) {
        m_loading = false;
        return;
    }

    {
        QSignalBlocker block(m_orientation);
        selectData(m_orientation, static_cast<int>(m->orientation));
    }
    fillResolutions();

    {
        QSignalBlocker block(m_scale);
        m_scale->clear();
        for (int p : m->scaleOptions)
            m_scale->addItem(p == m->recommendedScale ? QString("%1 % (recommended)").arg(p)
                                                      : QString("%1 %").arg(p), p);
        if (!selectData(m_scale, m->scale)) {
            m_scale->addItem(QString("%1 %").arg(m->scale), m->scale);
            m_scale->setCurrentIndex(m_scale->count() - 1);
        }
        m_scale->setEnabled(m->scaleKnown);
    }

    m_hdr->setEnabled(m->hdrSupported);
    m_hdr->setText(!m->hdrSupported ? QString("Not supported") : m->hdrOn ? QString("On") : QString("Off"));
    Theme::setActive(m_hdr, m->hdrSupported && m->hdrOn);

    m_brightness->setEnabled(m->brightnessSupported);
    m_brightness->setValue(m->brightnessSupported ? m->brightness : 0);
    m_brightnessValue->setText(m->brightnessSupported ? QString("%1 %").arg(m->brightness) : QString("n/a"));
    m_brightness->setToolTip(m->brightnessSupported
                                 ? QString("Brightness of this display")
                                 : m->internal ? QString("Windows does not report a brightness for this display.")
                                               : QString("This monitor does not take brightness commands "
                                                         "(DDC/CI). Some monitors have it switched off in their menu."));

    {
        QSignalBlocker block(m_profile);
        m_profile->clear();
        for (const QString& profile : m->colorProfiles)
            m_profile->addItem(QFileInfo(profile).fileName(), profile);
        if (!m->colorProfile.isEmpty() && !selectData(m_profile, m->colorProfile)) {
            m_profile->addItem(QFileInfo(m->colorProfile).fileName(), m->colorProfile);
            m_profile->setCurrentIndex(m_profile->count() - 1);
        }
        if (m->colorProfile.isEmpty())
            m_profile->setCurrentIndex(-1);
        m_profile->setEnabled(m_profile->count() > 0);
        m_profile->setPlaceholderText("No profiles");
    }
    m_loading = false;
}

// The resolution list in the orientation chosen in the combo (data: the landscape size).
void DisplayPanel::fillResolutions()
{
    const DisplayModel::Monitor* m = selected();
    if (!m)
        return;
    const auto orientation = static_cast<DisplayModel::Orientation>(m_orientation->currentData().toInt());
    const QSize keep = m_resolution->count() > 0
        ? m_resolution->currentData().toSize()
        : QSize();
    const DisplayModel::Size current = DisplayModel::isPortrait(m->orientation)
        ? DisplayModel::Size{m->mode.height, m->mode.width}
        : m->mode.size();

    {
        QSignalBlocker block(m_resolution);
        m_resolution->clear();
        for (const auto& size : DisplayModel::sizes(m->modes))
            m_resolution->addItem(sizeText(DisplayModel::oriented(size, orientation)), QSize(size.width, size.height));
        // what the user picked when only the orientation changed, else the monitor's own mode
        if (m_loading || !selectData(m_resolution, keep))
            selectData(m_resolution, QSize(current.width, current.height));
    }
    fillRefreshRates();
}

void DisplayPanel::fillRefreshRates()
{
    const DisplayModel::Monitor* m = selected();
    if (!m)
        return;
    const QSize native = m_resolution->currentData().toSize();
    const int keep = m_refresh->currentData().toInt();
    QSignalBlocker block(m_refresh);
    m_refresh->clear();
    for (int hz : DisplayModel::refreshRates(m->modes, {native.width(), native.height()}))
        m_refresh->addItem(QString("%1 Hz").arg(hz), hz);
    if (!(keep > 0 && !m_loading && selectData(m_refresh, keep)) && !selectData(m_refresh, m->mode.refresh))
        m_refresh->setCurrentIndex(0);   // the highest
}

// ---------------------------------------------------------------------------
// Changing the PC's settings
// ---------------------------------------------------------------------------

void DisplayPanel::report(const QString& text, bool failed)
{
    if (failed)
        Logger::instance().warn(text.toStdString(), "Display");
    else
        Logger::instance().info(text.toStdString(), "Display");
    emit message(text);
}

// Like Windows: a new mode or arrangement is undone unless it is confirmed in time,
// in case the display shows nothing.
bool DisplayPanel::askToKeep()
{
    QMessageBox box(QMessageBox::Question, "Keep these display settings?", QString(), QMessageBox::NoButton,
                    window());
    auto* keep   = box.addButton("Keep changes", QMessageBox::AcceptRole);
    auto* revert = box.addButton("Revert", QMessageBox::RejectRole);
    box.setDefaultButton(keep);
    box.setEscapeButton(revert);
    int left = kKeepSeconds;
    const auto update = [&]() {
        box.setText(QString("Going back to the previous settings in %1 seconds.").arg(left));
    };
    update();
    QTimer timer;
    timer.setInterval(1000);
    connect(&timer, &QTimer::timeout, &box, [&]() {
        if (--left <= 0)
            revert->click();
        else
            update();
    });
    timer.start();
    box.exec();
    return box.clickedButton() == keep;
}

void DisplayPanel::applyMode()
{
    const DisplayModel::Monitor* m = selected();
    if (!m)
        return;
    const QSize native = m_resolution->currentData().toSize();
    const auto orientation = static_cast<DisplayModel::Orientation>(m_orientation->currentData().toInt());
    const DisplayModel::Size shown = DisplayModel::oriented({native.width(), native.height()}, orientation);
    const DisplayModel::Mode mode{shown.width, shown.height, m_refresh->currentData().toInt()};
    if (mode == m->mode && orientation == m->orientation) {
        emit message("The display already uses these settings.");
        return;
    }

    const QString id = m->id;
    const DisplayModel::Mode oldMode = m->mode;
    const DisplayModel::Orientation oldOrientation = m->orientation;
    const QString what = QString("%1 @ %2 Hz, %3")
                             .arg(sizeText(shown)).arg(mode.refresh).arg(DisplayModel::orientationName(orientation));
    QString error;
    if (!displayBackend().setMode(id, mode, orientation, false, &error)) {
        report(error, true);
        reload();
        return;
    }
    if (askToKeep()) {
        report("Display set to " + what + '.');
    } else if (displayBackend().setMode(id, oldMode, oldOrientation, false, &error)) {
        report("Display settings reverted.");
    } else {
        report("Reverting failed: " + error, true);
    }
    reload();
}

void DisplayPanel::toggleHdr()
{
    const DisplayModel::Monitor* m = selected();
    if (!m)
        return;
    const bool on = !m->hdrOn;
    QString error;
    if (displayBackend().setHdr(m->id, on, &error))
        report(QString("HDR %1 for %2.").arg(on ? "on" : "off", m->name));
    else
        report(error, true);
    reload();
}

void DisplayPanel::applyScale()
{
    const DisplayModel::Monitor* m = selected();
    const int percent = m_scale->currentData().toInt();
    if (!m || percent == m->scale)
        return;
    QString error;
    if (displayBackend().setScale(m->id, percent, &error))
        report(QString("Scale %1 % for %2. Some apps only follow after a restart.").arg(percent).arg(m->name));
    else
        report(error, true);
    reload();
}

void DisplayPanel::applyBrightness()
{
    const int index = m_monitor->currentIndex();
    if (index < 0 || index >= static_cast<int>(m_monitors.size()))
        return;
    DisplayModel::Monitor& m = m_monitors[static_cast<size_t>(index)];
    const int percent = m_brightness->value();
    QString error;
    if (displayBackend().setBrightness(m.id, percent, &error)) {
        m.brightness = percent;
        emit message(QString("Brightness %1 % for %2.").arg(percent).arg(m.name));
    } else {
        report(error, true);
    }
}

void DisplayPanel::applyColorProfile()
{
    const DisplayModel::Monitor* m = selected();
    const QString profile = m_profile->currentData().toString();
    if (!m || profile.isEmpty() || profile == m->colorProfile)
        return;
    QString error;
    if (displayBackend().setColorProfile(m->id, profile, &error))
        report(QString("Color profile %1 for %2.").arg(QFileInfo(profile).fileName(), m->name));
    else
        report(error, true);
    reload();
}

void DisplayPanel::applyTopology()
{
    DisplayBackend& backend = displayBackend();
    const auto wanted = static_cast<DisplayModel::Topology>(m_topology->currentData().toInt());
    const DisplayModel::Topology old = backend.topology();
    if (wanted == old)
        return;
    QString error;
    if (!backend.setTopology(wanted, &error)) {
        report(error, true);
        reload();
        return;
    }
    if (askToKeep())
        report("Displays: " + DisplayModel::topologyName(wanted) + '.');
    else if (old != DisplayModel::Topology::Unknown && backend.setTopology(old, &error))
        report("Display arrangement reverted.");
    else
        report("Reverting failed: " + (error.isEmpty() ? QString("the previous arrangement is unknown") : error),
               true);
    reload();
}

// ---------------------------------------------------------------------------
// The selected game's settings
// ---------------------------------------------------------------------------

void DisplayPanel::loadGame()
{
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    for (QWidget* w : std::initializer_list<QWidget*>{m_gameEnabled, m_gameMonitor, m_gameResolution,
                                                      m_gameRefresh, m_gameHdr, m_gameScale, m_gameBrightness})
        w->setEnabled(game.has_value());
    m_loading = true;
    const DisplayModel::GameSettings s = game ? game->display : DisplayModel::GameSettings{};
    const QString name = !game ? QString()
                       : game->savedName.isEmpty() ? QFileInfo(m_gameExe).completeBaseName()
                                                   : game->savedName;
    m_gameEnabled->setText(game ? QString("Use these settings while %1 runs").arg(name)
                                : QString("Select a game to give it its own display settings"));
    m_gameEnabled->setChecked(s.enabled);

    {
        QSignalBlocker block(m_gameMonitor);
        m_gameMonitor->clear();
        m_gameMonitor->addItem("Main display", QString());
        for (size_t i = 0; i < m_monitors.size(); ++i)
            m_gameMonitor->addItem(monitorText(m_monitors[i], static_cast<int>(i) + 1), m_monitors[i].id);
        if (!selectData(m_gameMonitor, s.monitorId)) {
            m_gameMonitor->addItem("Saved display (not connected)", s.monitorId);
            m_gameMonitor->setCurrentIndex(m_gameMonitor->count() - 1);
        }
    }
    selectData(m_gameHdr, s.hdr);
    if (!selectData(m_gameBrightness, s.brightness)) {
        m_gameBrightness->addItem(QString("%1 %").arg(s.brightness), s.brightness);
        m_gameBrightness->setCurrentIndex(m_gameBrightness->count() - 1);
    }
    fillGameChoices();
    m_loading = false;
    updateGameStatus();
}

// Resolution, refresh rate and scale choices for the chosen monitor, keeping the saved values.
void DisplayPanel::fillGameChoices()
{
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    const DisplayModel::GameSettings s = game ? game->display : DisplayModel::GameSettings{};
    const int index = DisplayModel::pickMonitor(m_monitors, m_gameMonitor->currentData().toString());
    const DisplayModel::Monitor* m = index >= 0 ? &m_monitors[static_cast<size_t>(index)] : nullptr;

    {
        QSignalBlocker block(m_gameResolution);
        m_gameResolution->clear();
        m_gameResolution->addItem("Keep", QSize());
        if (m)
            for (const auto& size : DisplayModel::sizes(DisplayModel::orientedModes(*m)))
                m_gameResolution->addItem(sizeText(size), QSize(size.width, size.height));
        const QSize saved(s.width, s.height);
        if (s.width > 0 && !selectData(m_gameResolution, saved)) {
            m_gameResolution->addItem(sizeText({s.width, s.height}), saved);
            m_gameResolution->setCurrentIndex(m_gameResolution->count() - 1);
        } else if (s.width <= 0) {
            m_gameResolution->setCurrentIndex(0);
        }
    }
    fillGameRefreshRates();

    {
        QSignalBlocker block(m_gameScale);
        m_gameScale->clear();
        m_gameScale->addItem("Keep", 0);
        const std::vector<int>& steps = m && m->scaleKnown ? m->scaleOptions : DisplayModel::kScaleSteps;
        for (int p : steps)
            m_gameScale->addItem(QString("%1 %").arg(p), p);
        if (!selectData(m_gameScale, s.scale)) {
            m_gameScale->addItem(QString("%1 %").arg(s.scale), s.scale);
            m_gameScale->setCurrentIndex(m_gameScale->count() - 1);
        }
    }
    m_gameHdr->setToolTip(m && !m->hdrSupported ? QString("This display does not support HDR.") : QString());
    m_gameBrightness->setToolTip(m && !m->brightnessSupported
                                     ? QString("This display does not take brightness commands.")
                                     : QString());
}

void DisplayPanel::fillGameRefreshRates()
{
    const int index = DisplayModel::pickMonitor(m_monitors, m_gameMonitor->currentData().toString());
    const DisplayModel::Monitor* m = index >= 0 ? &m_monitors[static_cast<size_t>(index)] : nullptr;
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    const int saved = m_loading && game ? game->display.refresh : m_gameRefresh->currentData().toInt();

    QSignalBlocker block(m_gameRefresh);
    m_gameRefresh->clear();
    m_gameRefresh->addItem("Keep", 0);
    if (m) {
        const auto modes = DisplayModel::orientedModes(*m);
        const QSize size = m_gameResolution->currentData().toSize();
        std::vector<int> rates;
        if (size.isValid()) {
            rates = DisplayModel::refreshRates(modes, {size.width(), size.height()});
        } else {   // "Keep" resolution: every rate the display has
            for (const auto& mode : modes)
                if (mode.refresh > 0 && std::find(rates.begin(), rates.end(), mode.refresh) == rates.end())
                    rates.push_back(mode.refresh);
            std::sort(rates.rbegin(), rates.rend());
        }
        for (int hz : rates)
            m_gameRefresh->addItem(QString("%1 Hz").arg(hz), hz);
    }
    if (!selectData(m_gameRefresh, saved))
        m_gameRefresh->setCurrentIndex(0);
}

void DisplayPanel::saveGame()
{
    if (m_loading || m_gameExe.isEmpty())
        return;
    auto game = AppConfig::instance().gameByPath(m_gameExe);
    if (!game)
        return;
    DisplayModel::GameSettings s;
    s.enabled    = m_gameEnabled->isChecked();
    s.monitorId  = m_gameMonitor->currentData().toString();
    const QSize size = m_gameResolution->currentData().toSize();
    if (size.isValid()) {
        s.width  = size.width();
        s.height = size.height();
    }
    s.refresh    = m_gameRefresh->currentData().toInt();
    s.hdr        = m_gameHdr->currentData().toInt();
    s.scale      = m_gameScale->currentData().toInt();
    s.brightness = m_gameBrightness->currentData().toInt();
    if (s == game->display)
        return;
    game->display = s;
    AppConfig::instance().saveGame(*game);
    Logger::instance().info(QString("Display settings for %1: %2%3")
                                .arg(game->savedName, DisplayModel::describe(s), s.enabled ? "" : " (off)")
                                .toStdString(),
                            "Display");
    updateGameStatus();
}

void DisplayPanel::updateGameStatus()
{
    const bool active = m_session->isActive();
    m_gameRestore->setEnabled(active);
    if (active) {
        m_gameStatus->setText(m_session->description());
        return;
    }
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    if (!game) {
        m_gameStatus->setText(QString());
        return;
    }
    const DisplayModel::GameSettings& s = game->display;
    m_gameStatus->setText(
        !s.enabled      ? QString("Off: the game starts with the desktop's display settings.")
        : s.isEmpty()   ? QString("Pick what to change; \"Keep\" leaves a setting as it is.")
                        : QString("On Start / Restart: %1. Undone when the game closes.").arg(DisplayModel::describe(s)));
}
