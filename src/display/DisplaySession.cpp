#include "DisplaySession.h"

#include "DisplayBackend.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "window/WindowManager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSettings>
#include <QTimer>

namespace {

constexpr int kTickMs        = 2000;
constexpr int kStartWaitTicks = 90;   // 3 minutes for the game to appear (Steam can be slow)
constexpr int kGoneTicks     = 3;    // gone this long = closed (a launcher may restart the game)

QSettings uiSettings()
{
    return QSettings(QCoreApplication::applicationDirPath() + "/windowmanager.ini", QSettings::IniFormat);
}

void log(const QString& text, bool warning = false)
{
    if (warning)
        Logger::instance().warn(text.toStdString(), "Display");
    else
        Logger::instance().info(text.toStdString(), "Display");
}

} // namespace

DisplaySession::DisplaySession(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(kTickMs);
    connect(m_timer, &QTimer::timeout, this, &DisplaySession::tick);
}

QString DisplaySession::description() const
{
    if (!m_active)
        return {};
    return QString("Display settings for %1 are on. The desktop's come back when it closes.").arg(m_gameName);
}

void DisplaySession::gameStarting(const QString& exePath)
{
    if (m_active && exePath.compare(m_exePath, Qt::CaseInsensitive) != 0)
        restore("another game is starting");

    const auto game = AppConfig::instance().gameByPath(exePath);
    if (!game || !game->display.enabled || game->display.isEmpty()) {
        if (m_active)   // restarted after its settings were switched off
            restore("its display settings are off");
        return;
    }
    DisplayBackend& backend = displayBackend();
    if (!backend.supported())
        return;

    const DisplayModel::GameSettings& want = game->display;
    auto monitors = backend.monitors();
    const int index = DisplayModel::pickMonitor(monitors, want.monitorId);
    if (index < 0)
        return;

    if (!m_active) {
        m_restore = {};
        m_gameName = game->savedName.isEmpty() ? QFileInfo(exePath).completeBaseName() : game->savedName;
    }
    m_exePath = exePath;
    log(QString("Applying %1's display settings (%2) on %3")
            .arg(m_gameName, DisplayModel::describe(want), monitors[index].name));

    // The mode first: the scale steps depend on the resolution.
    DisplayModel::GameSettings modeOnly;
    modeOnly.width   = want.width;
    modeOnly.height  = want.height;
    modeOnly.refresh = want.refresh;
    const DisplayModel::Changes modeChange = DisplayModel::plan(monitors[index], modeOnly);
    DisplayModel::merge(m_restore, DisplayModel::undoFor(monitors[index], modeChange));
    persist();

    QStringList failures;
    apply(monitors[index], modeChange, failures);

    DisplayModel::Monitor now = monitors[index];
    if (modeChange.mode) {
        monitors = backend.monitors();
        const int again = DisplayModel::pickMonitor(monitors, now.id);
        if (again >= 0)
            now = monitors[again];
    }
    DisplayModel::GameSettings rest = want;
    rest.width = rest.height = rest.refresh = 0;
    const DisplayModel::Changes others = DisplayModel::plan(now, rest);
    DisplayModel::merge(m_restore, DisplayModel::undoFor(now, others));
    persist();
    apply(now, others, failures);

    if (m_restore.isEmpty()) {
        log("The display already matches; nothing to change.");
        return;
    }
    m_active      = true;
    m_seenRunning = false;
    m_waitTicks   = 0;
    m_goneTicks   = 0;
    m_timer->start();

    const QString text = failures.isEmpty()
        ? QString("Display settings for %1: %2.").arg(m_gameName, DisplayModel::describe(want))
        : QString("Some display settings for %1 failed: %2").arg(m_gameName, failures.join(' '));
    log(text, !failures.isEmpty());
    emit message(text);
    emit changed();
}

bool DisplaySession::apply(const DisplayModel::Monitor& monitor, const DisplayModel::Changes& changes,
                           QStringList& failures)
{
    DisplayBackend& backend = displayBackend();
    const int before = failures.size();
    QString error;
    if (changes.mode && !backend.setMode(monitor.id, *changes.mode, monitor.orientation, true, &error))
        failures << error;
    if (changes.hdr && !backend.setHdr(monitor.id, *changes.hdr, &error))
        failures << error;
    if (changes.scale && !backend.setScale(monitor.id, *changes.scale, &error))
        failures << error;
    if (changes.brightness && !backend.setBrightness(monitor.id, *changes.brightness, &error))
        failures << error;
    return failures.size() == before;
}

void DisplaySession::restore(const QString& reason)
{
    if (!m_active)
        return;
    m_timer->stop();
    m_active = false;

    DisplayBackend& backend = displayBackend();
    const DisplayModel::Restore r = m_restore;
    QStringList failures;
    QString error;
    // The mode first, so the scale is set at the desktop's resolution.
    if (r.mode && !backend.resetMode(r.monitorId, &error))
        failures << error;
    if (r.hdr && !backend.setHdr(r.monitorId, *r.hdr, &error))
        failures << error;
    if (r.scale && !backend.setScale(r.monitorId, *r.scale, &error))
        failures << error;
    if (r.brightness && !backend.setBrightness(r.monitorId, *r.brightness, &error))
        failures << error;

    m_restore = {};
    QSettings settings = uiSettings();
    settings.remove("display/restore");
    settings.remove("display/game");

    const QString text = failures.isEmpty()
        ? QString("Desktop display settings restored (%1).").arg(reason)
        : QString("Restoring the desktop display settings failed: %1").arg(failures.join(' '));
    log(text + (failures.isEmpty() ? QString() : QString(" (%1)").arg(reason)), !failures.isEmpty());
    emit message(text);
    emit changed();
}

void DisplaySession::recoverFromCrash()
{
    QSettings settings = uiSettings();
    const QByteArray json = settings.value("display/restore").toByteArray();
    if (json.isEmpty())
        return;
    m_restore  = DisplayModel::restoreFromJson(QJsonDocument::fromJson(json).object());
    m_gameName = settings.value("display/game").toString();
    m_active   = !m_restore.isEmpty();
    if (m_active)
        restore(QString("left over from %1 in the last session").arg(m_gameName));
    else
        settings.remove("display/restore");
}

void DisplaySession::persist()
{
    QSettings settings = uiSettings();
    if (m_restore.isEmpty()) {
        settings.remove("display/restore");
        settings.remove("display/game");
        return;
    }
    settings.setValue("display/restore", QJsonDocument(DisplayModel::toJson(m_restore)).toJson(QJsonDocument::Compact));
    settings.setValue("display/game", m_gameName);
}

void DisplaySession::tick()
{
    if (WindowManager::isExeRunning(m_exePath)) {
        m_seenRunning = true;
        m_goneTicks = 0;
        return;
    }
    if (m_seenRunning) {
        if (++m_goneTicks >= kGoneTicks)
            restore(m_gameName + " closed");
        return;
    }
    if (++m_waitTicks >= kStartWaitTicks)
        restore(m_gameName + " did not start");
}
