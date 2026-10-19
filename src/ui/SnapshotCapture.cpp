#include "SnapshotCapture.h"

#include "GameRegistry.h"
#include "WindowManagerController.h"
#include "core/Logger.h"
#include "platform/PlatformBackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPixmap>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

SnapshotCapture::SnapshotCapture(WindowManagerController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(80); // rapid burst cadence
    connect(m_timer, &QTimer::timeout, this, &SnapshotCapture::onTick);
}

void SnapshotCapture::start(int count)
{
    if (isRunning())
        return;

    m_hwnd = m_controller->activeWindow();
    if (!m_hwnd) {
        emit message("Snapshot: no active window selected.");
        return;
    }

    const QString exePath = GameRegistry::normalizePath(m_controller->targetExecutable());
    m_saveDir = GameRegistry::snapshotDirFor(exePath);
    QDir().mkpath(m_saveDir);
    m_baseName = exePath.isEmpty() ? QString("Window") : QFileInfo(exePath).completeBaseName();

    m_remaining = qMax(1, count);
    m_saved = 0;
    emit runningChanged(true);

    captureOne(); // first shot immediately
    if (--m_remaining > 0)
        m_timer->start();
    else
        finish();
}

void SnapshotCapture::onTick()
{
    captureOne();
    if (--m_remaining <= 0) {
        m_timer->stop();
        finish();
    }
}

void SnapshotCapture::finish()
{
    m_remaining = 0;
    m_hwnd = 0;
    emit runningChanged(false);
    if (m_saved == 0)
        emit message("Snapshot failed: the window could not be captured (see the log).");
    else
        emit message(QString("%1 snapshot(s) saved to %2").arg(m_saved).arg(QDir::toNativeSeparators(m_saveDir)));
}

void SnapshotCapture::openFolder() const
{
    const QString dir = GameRegistry::snapshotDirFor(m_controller->targetExecutable());
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void SnapshotCapture::captureOne()
{
    const QPixmap pix = m_hwnd ? QPixmap::fromImage(platform().captureWindow(m_hwnd)) : QPixmap();
    if (pix.isNull()) {
        Logger::instance().error("Snapshot: capturing the window failed", "SnapshotCapture");
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss_zzz");
    QString filename = m_saveDir + "/" + m_baseName + "_" + ts + ".png";
    for (int i = 1; QFile::exists(filename); ++i)
        filename = m_saveDir + "/" + m_baseName + "_" + ts + "_" + QString::number(i) + ".png";

    if (pix.save(filename, "PNG")) {
        ++m_saved;
        Logger::instance().info("Snapshot saved: " + filename.toStdString(), "SnapshotCapture");
    }
    else
        Logger::instance().error("Snapshot save failed: " + filename.toStdString(), "SnapshotCapture");
}
