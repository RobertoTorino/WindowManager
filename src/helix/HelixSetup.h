#pragma once

#include <functional>
#include <optional>

#include <QCryptographicHash>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>

#include "helix/HelixBridge.h"
#include "helix/HelixPaths.h"
#include "helix/HelixRelease.h"

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

/**
 * Runs the HelixSR setup for the user: finds the newest release, downloads and verifies it,
 * downloads the setup's tools (Python, numpy, DXC) and lets HelixSR's own setup script build the
 * network on this PC, removes what is no longer needed, and installs HelixSR into (or removes it
 * from) a game's FSR 3.1 DLL. HelixSR's helixsr-setup.ps1 is never run: its last step installs into
 * every game it finds.
 *
 * One job at a time; everything is asynchronous and reported through signals. The decisions
 * (versions, which DLL, what to delete) live in the pure helix/ units.
 */
class HelixSetup : public QObject
{
    Q_OBJECT

public:
    enum class Phase { Idle, Checking, Downloading, Extracting, Building, Cleaning, Testing, Installing, Removing };

    explicit HelixSetup(QObject* parent = nullptr);

    Phase   phase() const { return m_phase; }
    bool    busy() const { return m_phase != Phase::Idle; }
    QString status() const { return m_status; }
    int     percent() const { return m_percent; }   // -1 when unknown

    // The built version in use, the newest one on GitHub (after a check), and the last check's error.
    std::optional<Helix::Version> installedVersion() const;
    std::optional<Helix::Release> latest() const { return m_latest; }
    bool    updateAvailable() const;
    // A newer HelixSR than latest() whose major version this app does not set up (see kSupportedMajor).
    std::optional<Helix::Version> unsupportedNewer() const;
    QString lastError() const { return m_lastError; }
    bool    ready() const;   // built, and Python is there to install into games
    // The installer (HelixSR's bridge for 2.x, resources/helixsr_driver.py for 1.x) has passed its
    // self-test against the built package. When it has not (HelixSR changed its installer),
    // driverProblem() says why and games cannot be installed into.
    bool    driverOk() const { return m_driverOk; }
    QString driverProblem() const { return m_driverProblem; }
    bool    canInstall() const { return ready() && m_driverOk; }

    // The HelixSR folder games are installed from.
    QString packageDir() const { return m_package ? m_package->dir : QString(); }

public slots:
    void checkForUpdate();
    void installOrUpdate();   // download, build, clean up
    void cancel();
    void installIntoGame(const QString& exePath);
    void removeFromGame(const QString& exePath);
    void verifyDriver();      // self-test on a throw-away game folder (runs by itself when needed)

signals:
    void stateChanged();
    void updateChecked(bool ok);
    void buildFinished(bool ok, const QString& text);
    void gameJobFinished(const QString& exePath, bool install, bool ok, const QString& text);
    void driverChecked(bool ok, const QString& text);

private:
    void setPhase(Phase phase, const QString& status, int percent = -1);
    void note(const QString& line);   // log it and show it as the status
    void fail(const QString& text);
    void abortJob();

    void fetchExpectedHash();
    // Downloads `url` to `dest` (through dest.part), checks its SHA-256 and calls `next`; fails the job otherwise.
    void fetchFile(const QString& url, const QString& dest, const QString& sha256, const QString& title,
                   qint64 sizeHint, std::function<void()> next);
    // Unpacks a zip (or wheel) into `destDir` with tar.exe; done(ok, output).
    void unpack(const QString& archive, const QString& destDir, std::function<void(bool, const QString&)> done);
    void downloadZip();
    void extractZip();
    void prepareTools();   // Python + numpy and DXC for the setup, as helixsr-setup.ps1 gets them
    void buildNetwork();
    void finishBuild();
    void cleanUp();

    void runGameJob(const QString& exePath, bool install);
    // Runs Python of the HelixSR setup with `args`; done(exitCode, output), -1 if it could not run.
    void runPython(const QStringList& args, std::function<void(int, const QString&)> done);
    // Installs into or removes from the game in `folder` with the package's installer.
    void runInstaller(bool install, const QString& folder, std::function<void(int, const QString&)> done);
    void testDriver(std::function<void(bool, const QString&)> done);
    static QByteArray driverScript();
    QString installerFingerprint() const;
    static QString describeFailure(int exitCode, const QString& output);
    void handleBuildOutput(const QByteArray& data);

    QNetworkAccessManager* m_net;
    QPointer<QNetworkReply> m_reply;
    QProcess* m_process{nullptr};

    Phase   m_phase{Phase::Idle};
    QString m_status;
    int     m_percent{-1};
    QString m_lastError;

    std::optional<Helix::Release> m_latest;
    std::optional<Helix::Version> m_newest;   // newest of any major version
    std::optional<Helix::Package> m_package;
    bool m_installAfterCheck{false};
    bool m_cancelled{false};
    bool m_driverOk{false};
    QString m_driverProblem;

    QString m_expectedSha;
    QFile   m_zip;
    QCryptographicHash m_hash{QCryptographicHash::Sha256};
    QString m_zipPath;
    QString m_buildDir;       // the package being built; removed again if the build fails
    QMap<QString, Helix::ToolDownload> m_tools;   // from the release's helixsr-setup.ps1
    QByteArray m_lineBuffer;
};
