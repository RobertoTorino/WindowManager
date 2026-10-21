#pragma once

#include "stats/PictureInfo.h"
#include "stats/SessionReport.h"
#include "stats/StatsModel.h"

#include <QFile>
#include <QObject>

#include <vector>

/**
 * Records a game session for the Reports tab: one Report::Sample a second into
 * reports/<game>/<start>-samples.csv while the game runs (written as it goes, so a crash keeps it), and
 * when the game ends (or the app closes) the summary into <start>.json and a row into reports/sessions.csv.
 * Sessions shorter than Report::kMinSessionSeconds are dropped.
 */
class SessionRecorder : public QObject
{
    Q_OBJECT

public:
    explicit SessionRecorder(QObject *parent = nullptr);
    ~SessionRecorder() override;

    static QString reportsDir();   // <app folder>/reports
    static QString gameDir(const QString &game);

    // Each second (MainWindow::updatePicture): starts, continues or ends the session.
    void add(const QString &exePath, const Picture::Facts &facts, const Picture::Result &result,
             const Stats::GameStats &stats);
    void finish();   // the game ended or the app closes

signals:
    void sessionSaved(const QString &game);

private:
    void begin(const QString &exePath, const Picture::Facts &facts, const Stats::GameStats &stats);

    QFile   m_csv;
    QString m_exe;
    QString m_stamp;
    Report::Session m_session;          // the metadata so far
    std::vector<Report::Sample> m_samples;
};
