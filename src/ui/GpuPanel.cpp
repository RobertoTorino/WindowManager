#include "GpuPanel.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "stats/DriverModel.h"
#include "stats/StatsBackend.h"
#include "ui/Theme.h"
#include "window/WindowManager.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProcess>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStringList>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QGroupBox* group(const QString& title, QLayout* layout, QWidget* parent)
{
    auto* box = new QGroupBox(title, parent);
    box->setLayout(layout);
    return box;
}

QString dateText(const QDate& date)
{
    return date.isValid() ? QLocale::c().toString(date, "d MMM yyyy") : QString();
}

} // namespace

GpuPanel::GpuPanel(WindowManagerController* controller, QWidget* parent)
    : ControlPanel(controller, parent)
    , m_driverCheck(new DriverCheck(this))
{
    StatsBackend& backend = statsBackend();

    m_unsupported = new QLabel("Graphics card and power settings are not available on this system yet (Windows only).",
                               this);
    m_unsupported->setObjectName("hint");
    m_unsupported->setWordWrap(true);

    m_content = new QWidget(this);
    QWidget* c = m_content;

    // ----- Graphics cards and drivers -----------------------------------------------------
    auto* cards = new QGridLayout;
    buildAdapters(cards);
    m_checkDrivers = Theme::button("Check for driver updates", c, nullptr, "Ask NVIDIA for its newest driver");
    auto* windowsSettings = Theme::button("Windows graphics settings...", c, nullptr,
                                          "Open Settings > System > Display > Graphics");
    auto* cardButtons = new QHBoxLayout;
    cardButtons->addWidget(m_checkDrivers);
    cardButtons->addStretch(1);
    cardButtons->addWidget(windowsSettings);
    cards->addLayout(cardButtons, int(m_drivers.size()), 0, 1, 4);
    cards->setColumnStretch(2, 1);

    // ----- The game ------------------------------------------------------------------------
    m_gameTitle = new QLabel("Graphics card", c);
    m_preference = new QComboBox(c);
    for (auto p : {Stats::GpuPreference::Auto, Stats::GpuPreference::PowerSaving, Stats::GpuPreference::HighPerformance}) {
        const QString adapter = backend.adapterFor(p);
        m_preference->addItem(Stats::gpuPreferenceName(p) + (adapter.isEmpty() ? QString() : " (" + adapter + ")"),
                              int(p));
    }
    m_preference->setToolTip("Windows' graphics setting for the game's exe, the same as in Windows Settings.\n"
                             "Applies when the game starts next.");
    m_status = new QLabel(c);
    m_status->setObjectName("hint");
    m_status->setWordWrap(true);
    m_restart = Theme::button("Restart game", c, nullptr, "Close the game and start it again with this setting");
    auto* game = new QGridLayout;
    game->addWidget(m_gameTitle, 0, 0);
    game->addWidget(m_preference, 0, 1);
    game->addWidget(m_restart, 0, 2);
    game->addWidget(m_status, 1, 0, 1, 3);
    game->setColumnStretch(1, 1);

    // ----- Power ---------------------------------------------------------------------------
    m_plan = new QComboBox(c);
    m_plan->setToolTip("Windows' power plan. Also lists the plans Control Panel hides under \"Show additional plans\".");
    m_mode = new QComboBox(c);
    for (auto mode : {Stats::PowerMode::Efficiency, Stats::PowerMode::Balanced, Stats::PowerMode::Performance})
        m_mode->addItem(Stats::powerModeName(mode), int(mode));
    m_mode->setToolTip("Windows' power mode (Settings > System > Power & battery).\n"
                       "Windows only offers it with the Balanced plan.");
    m_source = new QLabel(c);
    m_source->setObjectName("hint");
    auto* powerOptions = Theme::button("Power options...", c, nullptr, "Open Control Panel > Power Options");
    auto* power = new QGridLayout;
    power->addWidget(new QLabel("Power plan", c), 0, 0);
    power->addWidget(m_plan, 0, 1);
    power->addWidget(new QLabel("Power mode", c), 0, 2);
    power->addWidget(m_mode, 0, 3);
    power->addWidget(powerOptions, 0, 4);
    power->addWidget(m_source, 1, 0, 1, 5);
    power->setColumnStretch(1, 1);
    power->setColumnStretch(3, 1);

    auto* content = new QVBoxLayout(c);
    content->setContentsMargins(0, 0, 0, 0);
    content->addWidget(group("Graphics cards in this PC", cards, c));
    content->addWidget(group("The selected game", game, c));
    content->addWidget(group("Power", power, c));

    auto* page = new QVBoxLayout(this);
    page->addWidget(m_unsupported);
    page->addWidget(m_content);
    page->addStretch(1);

    const bool supported = backend.gpuPreferenceSupported();
    m_unsupported->setVisible(!supported);
    m_content->setVisible(supported);

    connect(windowsSettings, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("ms-settings:display-advancedgraphics"));
    });
    connect(m_checkDrivers, &QPushButton::clicked, this, &GpuPanel::checkDrivers);
    connect(m_driverCheck, &DriverCheck::finished, this, &GpuPanel::driverChecked);
    connect(m_preference, qOverload<int>(&QComboBox::activated), this, &GpuPanel::choosePreference);
    connect(m_restart, &QPushButton::clicked, this, [this]() {
        m_changedWhileRunning = false;
        emit restartRequested();
    });
    connect(m_plan, qOverload<int>(&QComboBox::activated), this, &GpuPanel::choosePlan);
    connect(m_mode, qOverload<int>(&QComboBox::activated), this, &GpuPanel::chooseMode);
    connect(powerOptions, &QPushButton::clicked, this, []() {
        QProcess::startDetached("control.exe", {"/name", "Microsoft.PowerOptions"});
    });
}

// ---------------------------------------------------------------------------
// Graphics cards and drivers
// ---------------------------------------------------------------------------

void GpuPanel::buildAdapters(QGridLayout* grid)
{
    const auto adapters = statsBackend().adapters();
    const Stats::Adapter* dedicated = Stats::dedicatedAdapter(adapters);
    if (adapters.empty())
        grid->addWidget(new QLabel("No graphics card found.", m_content), 0, 0);
    for (const auto& a : adapters) {
        const int row = int(m_drivers.size());
        DriverRow d;
        d.adapter = a;
        auto* name = new QLabel(QString("%1  (%2)").arg(a.name, &a == dedicated
                                                                  ? "dedicated, " + Stats::formatBytes(a.dedicatedBytes)
                                                                  : QString("integrated")),
                                m_content);
        QString driver = "Driver: not found";
        if (!a.driverVersion.isEmpty()) {
            driver = "Driver " + Driver::displayVersion(a.vendorId, a.driverVersion);
            if (a.driverDate.isValid())
                driver += ", " + dateText(a.driverDate);
        }
        auto* version = new QLabel(driver, m_content);
        version->setToolTip("Windows' driver version: " + a.driverVersion);
        d.status = new QLabel(m_content);
        d.status->setObjectName("statValue");
        d.page = Theme::button("Driver page", m_content, nullptr, "Open the vendor's driver download page");
        d.url = Driver::driverPage(a.vendorId);
        d.page->setVisible(!d.url.isEmpty());
        if (a.vendorId != Driver::kNvidia && !d.url.isEmpty())
            d.status->setText(QString("%1 has no update check: see its driver page").arg(Driver::vendorName(a.vendorId)));
        grid->addWidget(name, row, 0);
        grid->addWidget(version, row, 1);
        grid->addWidget(d.status, row, 2);
        grid->addWidget(d.page, row, 3);
        m_drivers.append(d);
        connect(d.page, &QPushButton::clicked, this, [this, row]() {
            QDesktopServices::openUrl(QUrl(m_drivers[row].url));
        });
    }
}

void GpuPanel::checkDrivers()
{
    m_driversChecked = true;
    const bool notebook = statsBackend().power().batteryPercent >= 0;
    for (auto& d : m_drivers) {
        if (d.adapter.vendorId != Driver::kNvidia)
            continue;
        d.status->setText("Checking...");
        Theme::setWarning(d.status, false);
        m_driverCheck->check(d.adapter, notebook);
    }
}

void GpuPanel::driverChecked(const DriverCheck::Result& result)
{
    for (auto& d : m_drivers) {
        if (d.adapter.luid != result.luid || d.adapter.vendorId != Driver::kNvidia)
            continue;
        const QString installed = Driver::displayVersion(d.adapter.vendorId, d.adapter.driverVersion);
        if (!result.checked) {
            d.status->setText("Could not check: " + result.error);
            Logger::instance().warn("Driver check for " + d.adapter.name.toStdString() + " failed: "
                                        + result.error.toStdString(), "GPU");
            return;
        }
        const Driver::Latest& latest = result.latest;
        const bool outdated = Driver::compareVersions(installed, latest.version) < 0;
        d.status->setText(outdated ? QString("Update available: %1 (%2)").arg(latest.version, dateText(latest.date))
                                   : QString("Up to date (newest is %1)").arg(latest.version));
        d.status->setToolTip(latest.name);
        Theme::setWarning(d.status, outdated);
        if (!latest.detailsUrl.isEmpty())
            d.url = latest.detailsUrl;
        Logger::instance().info(QString("%1: driver %2 installed, newest %3 (%4)")
                                    .arg(d.adapter.name, installed, latest.version, dateText(latest.date))
                                    .toStdString(),
                                "GPU");
        if (outdated)
            emit message(QString("A newer %1 driver is out: %2.").arg(d.adapter.name, latest.version));
        return;
    }
}

void GpuPanel::showEvent(QShowEvent* event)
{
    ControlPanel::showEvent(event);
    if (!statsBackend().gpuPreferenceSupported())
        return;
    loadPower();
    if (!m_driversChecked)   // once per run; the button checks again
        checkDrivers();
}

// ---------------------------------------------------------------------------
// The game's graphics setting
// ---------------------------------------------------------------------------

QString GpuPanel::gameExe() const
{
    const QString real = WindowManager::realExePath(m_target);
    return real.trimmed().isEmpty() ? QString() : QDir::toNativeSeparators(QFileInfo(real).absoluteFilePath());
}

void GpuPanel::refresh()
{
    const QString target = m_controller->targetExecutable();
    const bool otherGame = target != m_target;
    m_target = target;
    const QString exe = gameExe();
    if (otherGame || exe != m_exe) {   // another game, or a script's exe became known
        m_exe = exe;
        m_changedWhileRunning = false;
        loadPreference();
    }
    updateStatus();
}

void GpuPanel::loadPreference()
{
    const auto game = m_target.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_target);
    const QString name = !game ? QString()
                         : game->savedName.isEmpty() ? QFileInfo(m_target).completeBaseName() : game->savedName;
    m_gameTitle->setText(name.isEmpty() ? QString("Graphics card") : "Graphics card for " + name);
    m_preference->setEnabled(!m_exe.isEmpty());
    const auto preference = m_exe.isEmpty() ? Stats::GpuPreference::Auto : statsBackend().gpuPreference(m_exe);
    m_preference->setCurrentIndex(m_preference->findData(int(preference)));
}

void GpuPanel::choosePreference()
{
    if (m_exe.isEmpty())
        return;
    const auto preference = static_cast<Stats::GpuPreference>(m_preference->currentData().toInt());
    QString error;
    if (!statsBackend().setGpuPreference(m_exe, preference, &error)) {
        Logger::instance().warn("GPU setting failed for " + m_exe.toStdString() + ": " + error.toStdString(), "GPU");
        emit message(error);
        loadPreference();
        return;
    }
    const QString text = QString("Graphics card for %1: %2.").arg(QFileInfo(m_exe).fileName(),
                                                                  m_preference->currentText());
    Logger::instance().info(text.toStdString(), "GPU");
    m_changedWhileRunning = m_stats.running;
    emit message(m_stats.running ? text + " Restart the game to switch." : text);
    updateStatus();
}

void GpuPanel::showStats(const Stats::GameStats& stats)
{
    const bool changed = stats.running != m_stats.running || stats.gpuName != m_stats.gpuName;
    const bool powerChanged = stats.power.plan != m_stats.power.plan || stats.power.mode != m_stats.power.mode
                              || stats.power.onBattery != m_stats.power.onBattery;
    m_stats = stats;
    if (!stats.running)
        m_changedWhileRunning = false;
    if (changed)
        updateStatus();
    if (powerChanged && isVisible())   // changed in Windows' own settings
        loadPower();
}

void GpuPanel::updateStatus()
{
    QString text;
    if (m_target.isEmpty())
        text = "No game selected.";
    else if (m_exe.isEmpty())
        text = "The game is started by a script: start it once from this app, so it learns which exe to set.";
    else if (!m_stats.running)
        text = "The game is not running. The setting is used the next time it starts.";
    else if (m_stats.gpuName.isEmpty())
        text = "The game is running; which card it uses shows once it draws.";
    else
        text = "Renders on now: " + m_stats.gpuName + ".";
    if (m_changedWhileRunning)
        text += " Restart the game to switch.";
    m_status->setText(text);
    m_restart->setVisible(m_changedWhileRunning);
}

// ---------------------------------------------------------------------------
// Power
// ---------------------------------------------------------------------------

void GpuPanel::loadPower()
{
    StatsBackend& backend = statsBackend();
    {
        const QSignalBlocker block(m_plan);
        m_plan->clear();
        for (const auto& plan : backend.powerPlans()) {
            m_plan->addItem(plan.name, plan.id);
            if (plan.active)
                m_plan->setCurrentIndex(m_plan->count() - 1);
        }
    }
    const Stats::Power power = backend.power();
    m_mode->setEnabled(power.powerMode.has_value());
    if (power.powerMode)
        m_mode->setCurrentIndex(m_mode->findData(int(*power.powerMode)));
    else
        m_mode->setCurrentIndex(-1);

    QStringList source;
    if (power.known)
        source << (power.onBattery ? "On battery" : "Plugged in");
    if (power.batteryPercent >= 0)
        source << QString("battery %1 %").arg(power.batteryPercent);
    if (power.batterySaver)
        source << "battery saver on";
    if (!power.powerMode)
        source << "power mode: only with the Balanced plan";
    m_source->setText(source.join(", ") + '.');
}

void GpuPanel::choosePlan()
{
    const QString name = m_plan->currentText();
    QString error;
    if (!statsBackend().setPowerPlan(m_plan->currentData().toString(), &error)) {
        Logger::instance().warn("Power plan " + name.toStdString() + " failed: " + error.toStdString(), "Power");
        emit message(error);
    } else {
        Logger::instance().info("Power plan: " + name.toStdString(), "Power");
        emit message("Power plan: " + name + ".");
    }
    loadPower();
}

void GpuPanel::chooseMode()
{
    const auto mode = static_cast<Stats::PowerMode>(m_mode->currentData().toInt());
    QString error;
    if (!statsBackend().setPowerMode(mode, &error)) {
        Logger::instance().warn("Power mode failed: " + error.toStdString(), "Power");
        emit message(error);
    } else {
        Logger::instance().info("Power mode: " + Stats::powerModeName(mode).toStdString(), "Power");
        emit message("Power mode: " + Stats::powerModeName(mode) + ".");
    }
    loadPower();
}
