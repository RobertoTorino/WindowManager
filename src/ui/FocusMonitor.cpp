#include "FocusMonitor.h"

#include "WindowManagerController.h"
#include "platform/PlatformBackend.h"

#include <QDir>
#include <QTimer>

FocusMonitor::FocusMonitor(WindowManagerController *controller, QObject *parent)
    : QObject(parent)
    , m_controller(controller)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(250);
    connect(m_timer, &QTimer::timeout, this, &FocusMonitor::poll);
    m_timer->start();
}

void FocusMonitor::poll()
{
    PlatformBackend &os = platform();
    const quintptr window = os.foregroundWindow();
    const qint64 pid = window ? os.windowPid(window) : 0;
    const QString exe = m_controller->targetExecutable();
    if (pid != m_lastPid || exe != m_lastExe) {
        m_lastPid = pid;
        m_lastExe = exe;
        const QString image = pid ? os.processImagePath(pid) : QString();
        m_lastRunsGame = !image.isEmpty() && !exe.isEmpty()
                         && QDir::cleanPath(image).compare(QDir::cleanPath(exe), Qt::CaseInsensitive) == 0;
    }

    const auto owner = FocusSwitch::ownerOf(pid, os.currentPid(), m_controller->activeWindowPid(),
                                            !exe.isEmpty() && m_lastRunsGame);
    if (owner == m_owner)
        return;
    m_owner = owner;
    emit ownerChanged(owner);
}
