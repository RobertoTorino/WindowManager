#pragma once

#include <functional>
#include <optional>

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>

#include "magpie/MagpieRelease.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

/**
 * Keeps the app's own portable copy of Magpie (<app dir>/magpie): finds the newest release, downloads
 * it and checks its SHA-256, unpacks it, replaces an older copy (its settings, shader cache and logs are
 * kept) and starts it once so Magpie writes its default settings. A Magpie the user installed is never
 * touched; since only one Magpie can run, the set-up stops when another one is running.
 *
 * One job at a time, asynchronous, reported through signals. Decisions live in MagpieRelease.
 */
class MagpieSetup : public QObject
{
    Q_OBJECT

public:
    enum class Phase { Idle, Checking, Downloading, Extracting, Installing, FirstStart };

    explicit MagpieSetup(QObject* parent = nullptr);

    static bool supported();   // Windows only

    Phase   phase() const { return m_phase; }
    bool    busy() const { return m_phase != Phase::Idle; }
    QString status() const { return m_status; }
    int     percent() const { return m_percent; }   // -1 when unknown

    QString dir() const { return Magpie::installDir(); }
    std::optional<Magpie::Version> installedVersion() const { return Magpie::installedVersion(dir()); }
    std::optional<Magpie::Release> latest() const { return m_latest; }
    bool    updateAvailable() const;
    QString lastError() const { return m_lastError; }

public slots:
    void checkForUpdate();
    void installOrUpdate();
    void cancel();

signals:
    void stateChanged();
    void updateChecked(bool ok);
    void installFinished(bool ok, const QString& text);

private:
    void setPhase(Phase phase, const QString& status, int percent = -1);
    void note(const QString& line);
    void fail(const QString& text);

    void download();
    void extract();
    void stopOurs(std::function<void()> next);   // quits the app's Magpie if it runs; fails on another one
    void replace();
    void firstStart();
    void finish();
    // Calls done(true) as soon as `ready` holds, done(false) after `timeoutMs`; checks every 250 ms.
    void waitFor(std::function<bool()> ready, int timeoutMs, std::function<void(bool)> done);

    QNetworkAccessManager* m_net;
    QPointer<QNetworkReply> m_reply;
    QProcess* m_process{nullptr};

    Phase   m_phase{Phase::Idle};
    QString m_status;
    int     m_percent{-1};
    QString m_lastError;
    bool    m_installAfterCheck{false};
    bool    m_cancelled{false};

    std::optional<Magpie::Release> m_latest;
    QFile   m_zip;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_unpacked;   // the new copy, before it replaces the installed one
};
