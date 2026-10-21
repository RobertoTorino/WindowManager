#include "SessionRecorder.h"

#include "config/AppConfig.h"
#include "core/BuildInfo.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>

namespace {

const QString kTimes = QString::fromUtf8(" × ");
const QString kDot = QString::fromUtf8(" · ");

void logInfo(const QString &text) { Logger::instance().info(text.toStdString(), "Report"); }

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile f(path);
    return f.open(QIODevice::WriteOnly | QIODevice::Truncate) && f.write(data) == data.size();
}

} // namespace

SessionRecorder::SessionRecorder(QObject *parent)
    : QObject(parent)
{
}

SessionRecorder::~SessionRecorder()
{
    finish();
}

QString SessionRecorder::reportsDir()
{
    return QCoreApplication::applicationDirPath() + "/reports";
}

QString SessionRecorder::gameDir(const QString &game)
{
    return reportsDir() + "/" + Report::folderName(game);
}

void SessionRecorder::add(const QString &exePath, const Picture::Facts &facts, const Picture::Result &result,
                          const Stats::GameStats &stats)
{
    if (!stats.running || exePath.isEmpty()) {
        finish();
        return;
    }
    if (m_csv.isOpen() && QDir::cleanPath(exePath).compare(QDir::cleanPath(m_exe), Qt::CaseInsensitive) != 0)
        finish();   // another game
    if (!m_csv.isOpen())
        begin(exePath, facts, stats);
    if (!m_csv.isOpen() || !facts.windowFound)
        return;   // no game window yet (loading) or any more (closing): nothing to say about the picture

    Report::Sample s;
    s.t = int(m_session.start.secsTo(QDateTime::currentDateTime()));
    if (stats.fps.valid) {
        s.fps = stats.fps.fps;
        s.low1 = stats.fps.low1Fps;
    }
    s.cpu = stats.cpu.value_or(-1);
    s.gpu = stats.gpu.value_or(-1);
    s.magpieGpu = stats.magpieGpu.value_or(-1);
    s.ramMb = stats.ram ? double(*stats.ram) / (1024 * 1024) : -1;
    s.vramMb = stats.vram ? double(*stats.vram) / (1024 * 1024) : -1;
    const Report::SetupState state = result.paused                             ? Report::SetupState::Paused
                                     : result.starting                         ? Report::SetupState::Starting
                                     : result.scaler != Picture::Scaler::None ? Report::SetupState::Scaling
                                                                               : Report::SetupState::Idle;
    s.setup = Report::setupName(result.scalerName, state, result.from.w, result.from.h, result.to.w,
                                result.to.h, facts.window.w, facts.window.h);
    s.percent = result.percent;
    s.onBattery = stats.power.known && stats.power.onBattery;
    m_samples.push_back(s);
    m_csv.write((Report::toCsv(s) + "\n").toUtf8());
    m_csv.flush();

    if (m_session.gpuName.isEmpty())
        m_session.gpuName = stats.gpuName;
    if (m_session.monitor.isEmpty() && facts.monitor.valid())
        m_session.monitor = QString::number(facts.monitor.w) + kTimes + QString::number(facts.monitor.h)
                            + (facts.monitorHz > 0 ? kDot + QString::number(facts.monitorHz) + " Hz" : QString());
    for (const Stats::Warning &w : stats.warnings) {
        if (!m_session.warnings.contains(w.text))
            m_session.warnings << w.text;
    }
}

void SessionRecorder::begin(const QString &exePath, const Picture::Facts &facts, const Stats::GameStats &stats)
{
    Q_UNUSED(facts)
    Q_UNUSED(stats)
    const auto game = AppConfig::instance().gameByPath(exePath);
    m_session = {};
    m_samples.clear();
    m_session.game = game && !game->savedName.isEmpty() ? game->savedName : QFileInfo(exePath).completeBaseName();
    m_session.exeName = QFileInfo(exePath).fileName();
    m_session.steamAppId = game ? game->steamAppId : 0;
    m_session.start = QDateTime::currentDateTime();
    m_session.appVersion = BuildInfo::description();
    m_stamp = m_session.start.toString("yyyyMMdd-HHmmss");
    m_session.samplesFile = m_stamp + "-samples.csv";

    const QString dir = gameDir(m_session.game);
    QDir().mkpath(dir);
    m_csv.setFileName(dir + "/" + m_session.samplesFile);
    if (!m_csv.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        Logger::instance().warn("Could not write " + m_csv.fileName().toStdString(), "Report");
        return;
    }
    m_csv.write((Report::samplesHeader() + "\n").toUtf8());
    m_exe = exePath;
}

void SessionRecorder::finish()
{
    if (!m_csv.isOpen())
        return;
    m_csv.close();
    m_session.end = QDateTime::currentDateTime();
    const QString dir = QFileInfo(m_csv.fileName()).absolutePath();
    if (int(m_samples.size()) < Report::kMinSessionSeconds) {
        QFile::remove(m_csv.fileName());
        m_samples.clear();
        return;
    }

    const Report::Session session = Report::summarize(m_session, m_samples);
    m_samples.clear();
    if (!writeFile(dir + "/" + m_stamp + ".json", QJsonDocument(Report::toJson(session)).toJson())) {
        Logger::instance().warn("Could not write the session summary in " + dir.toStdString(), "Report");
        return;
    }
    QFile all(reportsDir() + "/sessions.csv");
    const bool fresh = !all.exists();
    if (all.open(QIODevice::Append)) {
        if (fresh)
            all.write((Report::sessionsHeader() + "\n").toUtf8());
        all.write((Report::sessionsCsvLine(session) + "\n").toUtf8());
    }
    QStringList setups;
    for (const Report::Segment &s : session.segments)
        setups << QString("%1 (%2 s, FPS %3)").arg(s.setup).arg(s.seconds).arg(s.fpsAvg < 0 ? QString("-") : QString::number(qRound(s.fpsAvg)));
    logInfo(QString("Session saved: %1, %2 s; %3").arg(session.game).arg(session.start.secsTo(session.end)).arg(setups.join("; ")));
    emit sessionSaved(session.game);
}
