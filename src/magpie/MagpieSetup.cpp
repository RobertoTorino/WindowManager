#include "MagpieSetup.h"

#include "core/Logger.h"
#include "magpie/MagpieProcess.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

namespace {

const QUrl kReleasesUrl("https://api.github.com/repos/Blinue/Magpie/releases?per_page=10");

void logInfo(const QString& text)  { Logger::instance().info(text.toStdString(), "Magpie"); }
void logWarn(const QString& text)  { Logger::instance().warn(text.toStdString(), "Magpie"); }
void logError(const QString& text) { Logger::instance().error(text.toStdString(), "Magpie"); }

QNetworkRequest makeRequest(const QUrl& url)
{
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WindowManager/") + APP_VERSION_STR);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    r.setTransferTimeout(30000);   // no data for 30 s
    return r;
}

QString lastLine(const QString& output)
{
    const QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

// Next to the install folder, so moving the new copy into place is a rename on the same drive.
QString downloadPath(const QString& zipName) { return Magpie::installDir() + "-download/" + zipName; }
QString unpackDir() { return Magpie::installDir() + "-new"; }

// A running Magpie that is not the app's copy ("" if none): only one Magpie can run at a time.
QString otherMagpie(const QString& dir)
{
    for (const auto& m : Magpie::runningMagpies()) {
        if (!Magpie::isFrom(m, dir))
            return m.path.isEmpty() ? QString("pid %1").arg(m.pid) : QDir::toNativeSeparators(m.path);
    }
    return {};
}

QString otherMagpieText(const QString& other)
{
    return QString("Another Magpie is running (%1). Only one Magpie can run at a time: close it (its tray icon > "
                   "Exit) and try again.").arg(other);
}

bool oursRunning(const QString& dir)
{
    for (const auto& m : Magpie::runningMagpies()) {
        if (Magpie::isFrom(m, dir))
            return true;
    }
    return false;
}

} // namespace

MagpieSetup::MagpieSetup(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

bool MagpieSetup::supported()
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool MagpieSetup::updateAvailable() const
{
    const auto installed = installedVersion();
    return m_latest && installed && m_latest->version > *installed;
}

void MagpieSetup::setPhase(Phase phase, const QString& status, int percent)
{
    m_phase = phase;
    m_status = status;
    m_percent = percent;
    emit stateChanged();
}

void MagpieSetup::note(const QString& line)
{
    logInfo(line);
    m_status = line;
    emit stateChanged();
}

void MagpieSetup::fail(const QString& text)
{
    logError(text);
    if (m_zip.isOpen())
        m_zip.close();
    QDir(Magpie::installDir() + "-download").removeRecursively();
    QDir(unpackDir()).removeRecursively();
    m_unpacked.clear();
    setPhase(Phase::Idle, text);
    emit installFinished(false, text);
}

// ---------------------------------------------------------------------------
// Update check
// ---------------------------------------------------------------------------

void MagpieSetup::checkForUpdate()
{
    if (busy() || !supported())
        return;
    m_cancelled = false;
    setPhase(Phase::Checking, "Checking for a new Magpie version...");

    QNetworkRequest request = makeRequest(kReleasesUrl);
    request.setRawHeader("Accept", "application/vnd.github+json");
    QNetworkReply* reply = m_net->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const bool wantInstall = m_installAfterCheck;
        m_installAfterCheck = false;
        if (m_cancelled) {
            setPhase(Phase::Idle, "Cancelled.");
            return;
        }
        QString error;
        if (reply->error() != QNetworkReply::NoError) {
            error = reply->errorString();
        } else {
            m_latest = Magpie::latestRelease(reply->readAll());
            if (!m_latest)
                error = "no Magpie release with an x64 zip and a SHA-256 digest was found";
        }
        m_lastError = error;
        if (!error.isEmpty()) {
            logWarn("Update check failed: " + error);
            setPhase(Phase::Idle, "Update check failed: " + error);
            if (wantInstall)
                emit installFinished(false, "Could not find the latest Magpie: " + error);
            else
                emit updateChecked(false);
            return;
        }
        const auto installed = installedVersion();
        logInfo(QString("Latest Magpie is %1; installed: %2")
                    .arg(m_latest->version.toString(), installed ? installed->toString() : QString("none")));
        if (wantInstall) {
            download();
            return;
        }
        setPhase(Phase::Idle, updateAvailable() ? QString("Magpie %1 is available.").arg(m_latest->version.toString())
                                                : QString("Magpie is up to date."));
        emit updateChecked(true);
    });
}

// ---------------------------------------------------------------------------
// Download, unpack, replace, first start
// ---------------------------------------------------------------------------

void MagpieSetup::installOrUpdate()
{
    if (busy() || !supported())
        return;
    m_cancelled = false;
    // before anything is downloaded
    if (const QString other = otherMagpie(dir()); !other.isEmpty()) {
        logError(otherMagpieText(other));
        emit installFinished(false, otherMagpieText(other));
        return;
    }
    if (!m_latest) {
        m_installAfterCheck = true;
        checkForUpdate();
        return;
    }
    download();
}

void MagpieSetup::download()
{
    const Magpie::Release r = *m_latest;
    const QString zip = downloadPath(r.zipName);
    QDir().mkpath(QFileInfo(zip).absolutePath());
    m_zip.setFileName(zip + ".part");
    if (!m_zip.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Could not write " + m_zip.fileName());
        return;
    }
    m_hash.reset();
    const QString title = QString("Downloading Magpie %1").arg(r.version.toString());
    setPhase(Phase::Downloading, title + "...", 0);
    logInfo(title + " from " + r.zipUrl);

    QNetworkReply* reply = m_net->get(makeRequest(QUrl(r.zipUrl)));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        const QByteArray chunk = reply->readAll();
        m_hash.addData(chunk);
        if (m_zip.write(chunk) != chunk.size())
            reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, title](qint64 got, qint64 total) {
        const qint64 size = total > 0 ? total : m_latest->zipSize;
        m_percent = size > 0 ? static_cast<int>(got * 100 / size) : -1;
        m_status = QString("%1: %2 of %3 MB").arg(title, QString::number(got / 1048576.0, 'f', 1),
                                                  QString::number(size / 1048576.0, 'f', 1));
        emit stateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, zip]() {
        reply->deleteLater();
        m_zip.close();
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail("Download failed: " + reply->errorString());
            return;
        }
        const QString actual = QString::fromLatin1(m_hash.result().toHex());
        if (actual != m_latest->sha256) {
            fail("The download does not match its published checksum, so it was discarded.");
            return;
        }
        logInfo("Download verified (SHA-256 " + actual + ")");
        QFile::remove(zip);
        if (!QFile::rename(zip + ".part", zip)) {
            fail("Could not rename the downloaded file.");
            return;
        }
        extract();
    });
}

void MagpieSetup::extract()
{
    setPhase(Phase::Extracting, "Unpacking Magpie...");
    const QString zip = downloadPath(m_latest->zipName);
    m_unpacked = unpackDir();
    QDir(m_unpacked).removeRecursively();
    QDir().mkpath(m_unpacked);

    // tar.exe ships with Windows 10 and later and reads zip archives.
    const QString tar = qEnvironmentVariable("SystemRoot") + "/System32/tar.exe";
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        QDir(Magpie::installDir() + "-download").removeRecursively();
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        if (status != QProcess::NormalExit || code != 0) {
            fail("Unpacking failed: " + lastLine(output));
            return;
        }
        // the zip holds Magpie.exe at its top level
        if (!QFile::exists(Magpie::exePath(m_unpacked))) {
            fail("The unpacked Magpie was not what was expected (no Magpie.exe).");
            return;
        }
        stopOurs([this]() { replace(); });
    });
    m_process->start(tar, {"-xf", zip, "-C", m_unpacked});
}

void MagpieSetup::waitFor(std::function<bool()> ready, int timeoutMs, std::function<void(bool)> done)
{
    if (ready()) {
        done(true);
        return;
    }
    if (timeoutMs <= 0 || m_cancelled) {
        done(false);
        return;
    }
    QTimer::singleShot(250, this, [this, ready, timeoutMs, done]() { waitFor(ready, timeoutMs - 250, done); });
}

void MagpieSetup::stopOurs(std::function<void()> next)
{
    const QString ours = dir();
    if (const QString other = otherMagpie(ours); !other.isEmpty()) {   // started during the download
        fail(otherMagpieText(other));
        return;
    }
    if (!oursRunning(ours)) {
        next();
        return;
    }
    setPhase(Phase::Installing, "Closing Magpie for the update...");
    Magpie::postQuit();
    waitFor([ours]() { return !oursRunning(ours); }, 15000, [this, next](bool stopped) {
        if (!stopped) {
            fail("Magpie did not close. Close it (its tray icon > Exit) and try again.");
            return;
        }
        logInfo("Magpie closed for the update");
        next();
    });
}

void MagpieSetup::replace()
{
    if (m_cancelled) {
        fail("Cancelled.");
        return;
    }
    setPhase(Phase::Installing, "Installing Magpie...");
    const QString target = dir();
    QDir().mkpath(target);

    // the old copy's program files go; its settings, shader cache and logs stay
    for (const QFileInfo& e : QDir(target).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (Magpie::keptOnUpdate(e.fileName()))
            continue;
        const bool gone = e.isDir() ? QDir(e.absoluteFilePath()).removeRecursively() : QFile::remove(e.absoluteFilePath());
        if (!gone) {
            fail("Could not replace " + e.absoluteFilePath() + " (is Magpie still running?)");
            return;
        }
    }
    for (const QFileInfo& e : QDir(m_unpacked).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
        const QString to = target + '/' + e.fileName();
        if (Magpie::keptOnUpdate(e.fileName()) && QFileInfo::exists(to))
            continue;   // the zip has none of these today; never overwrite the user's settings
        if (!QDir().rename(e.absoluteFilePath(), to)) {
            fail("Could not move " + e.fileName() + " into " + target);
            return;
        }
    }
    QDir(m_unpacked).removeRecursively();
    m_unpacked.clear();

    // config/config.json makes Magpie portable: it keeps its settings here, not in %LOCALAPPDATA%
    const QString config = Magpie::configPath(target);
    QDir().mkpath(QFileInfo(config).absolutePath());
    if (!QFileInfo::exists(config)) {
        QFile f(config);
        if (!f.open(QIODevice::WriteOnly)) {
            fail("Could not create " + config);
            return;
        }
    }
    firstStart();
}

void MagpieSetup::firstStart()
{
    const QString target = dir();
    const QString config = Magpie::configPath(target);
    if (QFileInfo(config).size() > 0) {
        finish();   // an update: Magpie's settings are there already
        return;
    }
    // Magpie fills an empty config with its defaults (scaling modes, default profile) when it starts;
    // the app's profiles need them.
    setPhase(Phase::FirstStart, "Starting Magpie once so it writes its settings...");
    if (!Magpie::startInTray(target)) {
        fail("Could not start " + Magpie::exePath(target));
        return;
    }
    waitFor([config]() { return QFileInfo(config).size() > 0; }, 20000, [this, target, config](bool written) {
        Magpie::postQuit();
        waitFor([target]() { return !oursRunning(target); }, 15000, [this, written, config](bool stopped) {
            if (!stopped)
                logWarn("Magpie did not close after its first start");
            if (!written) {
                fail("Magpie started but did not write its settings (" + config + ").");
                return;
            }
            finish();
        });
    });
}

void MagpieSetup::finish()
{
    if (!Magpie::markInstalled(dir(), m_latest->version)) {
        fail("Could not write to " + dir());
        return;
    }
    const QString text = QString("Magpie %1 is ready.").arg(m_latest->version.toString());
    logInfo(text + " (" + dir() + ")");
    setPhase(Phase::Idle, text);
    emit installFinished(true, text);
}

void MagpieSetup::cancel()
{
    // Replacing the files and the first start are short and must finish.
    if (!busy() || m_phase == Phase::Installing || m_phase == Phase::FirstStart)
        return;
    m_cancelled = true;
    m_installAfterCheck = false;
    if (m_reply)
        m_reply->abort();
    if (m_process)
        m_process->kill();   // its finished handler reports the cancellation
    if (!m_reply && !m_process)
        fail("Cancelled.");
}
