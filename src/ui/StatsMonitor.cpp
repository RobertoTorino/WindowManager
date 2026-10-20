#include "StatsMonitor.h"

#include "WindowManagerController.h"
#include "core/Logger.h"
#include "stats/StatsBackend.h"
#include "window/WindowManager.h"

#include <QPointer>
#include <QThreadPool>

#include <algorithm>

namespace {

constexpr int    kIntervalMs    = 1000;
constexpr qint64 kPidSearchMs   = 5000;
constexpr qint64 kFirstLogMs    = 10'000;   // the first summary, once FPS and CPU have settled
constexpr qint64 kLogIntervalMs = 60'000;

} // namespace

StatsMonitor::StatsMonitor(WindowManagerController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
{
    m_timer.setInterval(kIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &StatsMonitor::tick);
}

void StatsMonitor::setActive(bool active)
{
    if (active == m_timer.isActive())
        return;
    if (active) {
        m_timer.start();
        tick();
    } else {
        m_timer.stop();
    }
}

qint64 StatsMonitor::gamePid()
{
    const QString exe = m_controller->targetExecutable();
    if (exe != m_exe) {
        m_exe = exe;
        m_foundPid = 0;
        m_pidSearch.invalidate();
    }
    const qint64 active = m_controller->activeWindowPid();
    if (active && active != m_deadPid)
        return active;
    if (m_foundPid && m_foundPid != m_deadPid)
        return m_foundPid;
    // Started outside this app, or the window list is stale: look for the target's process.
    if (exe.isEmpty() || (m_pidSearch.isValid() && m_pidSearch.elapsed() < kPidSearchMs))
        return 0;
    m_pidSearch.restart();
    m_foundPid = WindowManager::runningPid(exe);
    return m_foundPid;
}

void StatsMonitor::tick()
{
    if (m_busy)
        return;
    m_busy = true;
    const qint64 pid = gamePid();
    const bool needAdapters = m_adapters.empty();
    QPointer<StatsMonitor> self(this);
    QThreadPool::globalInstance()->start([self, pid, needAdapters]() {
        StatsBackend &backend = statsBackend();
        const auto adapters = needAdapters ? backend.adapters() : std::vector<Stats::Adapter>{};
        const Stats::RawSample raw = backend.sample(pid);
        if (!self)
            return;
        QMetaObject::invokeMethod(self.data(), [self, pid, raw, adapters]() {
            if (!self)
                return;
            self->m_busy = false;
            if (!adapters.empty())
                self->m_adapters = adapters;
            self->m_last = self->m_sampler.update(pid, raw, self->m_adapters);
            if (pid && !self->m_last.running)
                self->m_deadPid = pid;
            self->log(self->m_last);
            emit self->statsUpdated(self->m_last);
            const qint64 running = self->m_last.running ? pid : 0;
            if (running != self->m_reportedPid) {
                self->m_reportedPid = running;
                emit self->gameChanged(running);
            }
        }, Qt::QueuedConnection);
    });
}

void StatsMonitor::log(const Stats::GameStats &stats)
{
    Logger &logger = Logger::instance();
    if (!stats.running) {
        m_running.invalidate();
        m_loggedWarnings.clear();
        return;
    }
    if (!m_running.isValid()) {
        m_running.start();
        m_nextLogMs = kFirstLogMs;
    }

    if (!m_loggedFpsSource && !stats.fpsError.isEmpty()) {   // once per app run
        m_loggedFpsSource = true;
        logger.info("Frame counter: " + stats.fpsError.toStdString(), "Stats");
    }
    for (const auto &w : stats.warnings) {   // each warning once per game run
        if (std::find(m_loggedWarnings.begin(), m_loggedWarnings.end(), w) == m_loggedWarnings.end()) {
            m_loggedWarnings.push_back(w);
            logger.warn(w.text.toStdString() + ": " + w.detail.toStdString(), "Stats");
        }
    }
    if (m_running.elapsed() >= m_nextLogMs) {
        m_nextLogMs = m_running.elapsed() + kLogIntervalMs;
        logger.info(Stats::summaryLine(stats).toStdString(), "Stats");
    }
}
