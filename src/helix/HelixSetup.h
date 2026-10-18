#pragma once

#include <optional>

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>

#include "helix/HelixPaths.h"
#include "helix/HelixRelease.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

/**
 * Runs the HelixSR setup for the user: finds the newest release, downloads and verifies it,
 * lets HelixSR's own setup build the network on this PC, removes what is no longer needed,
 * and installs HelixSR into (or removes it from) a game's FSR 3.1 DLL.
 *
 * One job at a time; everything is asynchronous and reported through signals. The decisions
 * (versions, which DLL, what to delete) live in the pure helix/ units.
 */
class HelixSetup : public QObject
{
    Q_OBJECT

public:
    enum class Phase { Idle, Checking, Downloading, Extracting, Building, Cleaning, Installing, Removing };

    explicit HelixSetup(QObject* parent = nullptr);

    Phase   phase() const { return m_phase; }
    bool    busy() const { return m_phase != Phase::Idle; }
    QString status() const { return m_status; }
    int     percent() const { return m_percent; }   // -1 when unknown

    // The built version in use, the newest one on GitHub (after a check), and the last check's error.
    std::optional<Helix::Version> installedVersion() const;
    std::optional<Helix::Release> latest() const { return m_latest; }
    bool    updateAvailable() const;
    QString lastError() const { return m_lastError; }
    bool    ready() const;   // built, and Python is there to install into games

    // The HelixSR folder games are installed from.
    QString packageDir() const { return m_package ? m_package->dir : QString(); }

public slots:
    void checkForUpdate();
    void installOrUpdate();   // download, build, clean up
    void cancel();
    void installIntoGame(const QString& exePath);
    void removeFromGame(const QString& exePath);

signals:
    void stateChanged();
    void updateChecked(bool ok);
    void buildFinished(bool ok, const QString& text);
    void gameJobFinished(const QString& exePath, bool install, bool ok, const QString& text);

private:
    void setPhase(Phase phase, const QString& status, int percent = -1);
    void note(const QString& line);   // log it and show it as the status
    void fail(const QString& text);
    void abortJob();

    void fetchExpectedHash();
    void downloadZip();
    void extractZip();
    void buildNetwork();
    void finishBuild();
    void cleanUp();

    void runGameJob(const QString& exePath, bool install);
    void handleBuildOutput(const QByteArray& data);

    QNetworkAccessManager* m_net;
    QPointer<QNetworkReply> m_reply;
    QProcess* m_process{nullptr};

    Phase   m_phase{Phase::Idle};
    QString m_status;
    int     m_percent{-1};
    QString m_lastError;

    std::optional<Helix::Release> m_latest;
    std::optional<Helix::Package> m_package;
    bool m_installAfterCheck{false};
    bool m_cancelled{false};
    bool m_networkBuilt{false};

    QString m_expectedSha;
    QFile   m_zip;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_zipPath;
    QString m_buildDir;       // the package being built; removed again if the build fails
    QByteArray m_lineBuffer;
};
