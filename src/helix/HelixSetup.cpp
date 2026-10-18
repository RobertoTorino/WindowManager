#include "HelixSetup.h"

#include "core/Logger.h"
#include "helix/HelixGame.h"

#include <memory>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>

namespace {

const QUrl kReleasesUrl("https://api.github.com/repos/lonewolf0622/HelixSR/releases?per_page=15");

void logInfo(const QString& text)  { Logger::instance().info(text.toStdString(), "HelixSR"); }
void logWarn(const QString& text)  { Logger::instance().warn(text.toStdString(), "HelixSR"); }
void logError(const QString& text) { Logger::instance().error(text.toStdString(), "HelixSR"); }

QNetworkRequest makeRequest(const QUrl& url)
{
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("WindowManager/") + APP_VERSION_STR);
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    r.setTransferTimeout(30000);   // no data for 30 s
    return r;
}

QString megabytes(qint64 bytes)
{
    return QString::number(bytes / 1048576.0, 'f', 0);
}

// Kills the process and everything it started (the setup runs Python under PowerShell).
void killTree(QProcess* p)
{
    if (!p || p->state() == QProcess::NotRunning)
        return;
    QProcess::execute("taskkill", {"/PID", QString::number(p->processId()), "/T", "/F"});
    p->kill();
}

bool removePath(const QString& path)
{
    const QFileInfo info(path);
    return info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
}

// The last line of the process output that says something, for an error message.
QString lastLine(const QString& output)
{
    const QStringList lines = output.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts);
    return lines.isEmpty() ? QString() : lines.last().trimmed();
}

} // namespace

HelixSetup::HelixSetup(QObject* parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_package(Helix::builtPackage(Helix::dataDir()))
{
}

std::optional<Helix::Version> HelixSetup::installedVersion() const
{
    return m_package ? std::optional<Helix::Version>(m_package->version) : std::nullopt;
}

bool HelixSetup::updateAvailable() const
{
    return m_latest && m_package && Helix::isUpdate(installedVersion(), m_latest->version);
}

bool HelixSetup::ready() const
{
    return m_package && QFile::exists(Helix::pythonExe(Helix::setupDataDir()));
}

void HelixSetup::setPhase(Phase phase, const QString& status, int percent)
{
    m_phase = phase;
    m_status = status;
    m_percent = percent;
    emit stateChanged();
}

void HelixSetup::note(const QString& line)
{
    logInfo(line);
    m_status = line;
    emit stateChanged();
}

// ---------------------------------------------------------------------------
// Update check
// ---------------------------------------------------------------------------

void HelixSetup::checkForUpdate()
{
    if (busy())
        return;
    m_cancelled = false;
    setPhase(Phase::Checking, "Checking for a new HelixSR version...");

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
            m_latest = Helix::latestRelease(reply->readAll());
            if (!m_latest)
                error = "no HelixSR release with a setup zip was found";
        }
        m_lastError = error;
        if (!error.isEmpty()) {
            logWarn("Update check failed: " + error);
            setPhase(Phase::Idle, "Update check failed: " + error);
            if (wantInstall)
                emit buildFinished(false, "Could not find the latest HelixSR: " + error);
            else
                emit updateChecked(false);
            return;
        }

        logInfo(QString("Latest HelixSR is %1; installed: %2")
                    .arg(m_latest->version.toString(),
                         m_package ? m_package->version.toString() : QString("none")));
        if (wantInstall) {
            m_phase = Phase::Downloading;   // continues the install without going through Idle
            fetchExpectedHash();
            return;
        }
        setPhase(Phase::Idle, updateAvailable() ? QString("HelixSR %1 is available.").arg(m_latest->version.toString())
                                                : QString("HelixSR is up to date."));
        emit updateChecked(true);
    });
}

// ---------------------------------------------------------------------------
// Download, build, clean up
// ---------------------------------------------------------------------------

void HelixSetup::installOrUpdate()
{
    if (busy())
        return;
    m_cancelled = false;
    m_networkBuilt = false;
    if (!m_latest) {
        m_installAfterCheck = true;
        checkForUpdate();
        return;
    }
    m_phase = Phase::Downloading;
    fetchExpectedHash();
}

void HelixSetup::fetchExpectedHash()
{
    const Helix::Release& r = *m_latest;
    if (!r.sha256.isEmpty()) {
        m_expectedSha = r.sha256;
        downloadZip();
        return;
    }
    if (r.sumsUrl.isEmpty()) {
        fail("The release publishes no checksum, so the download cannot be verified.");
        return;
    }

    setPhase(Phase::Downloading, "Getting the checksum...");
    QNetworkReply* reply = m_net->get(makeRequest(QUrl(r.sumsUrl)));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail("Could not get the checksum: " + reply->errorString());
            return;
        }
        m_expectedSha = Helix::hashFromSums(QString::fromUtf8(reply->readAll()), m_latest->zipName);
        if (m_expectedSha.isEmpty()) {
            fail("The checksum list has no entry for " + m_latest->zipName + ".");
            return;
        }
        downloadZip();
    });
}

void HelixSetup::downloadZip()
{
    const Helix::Release r = *m_latest;
    if (!QDir().mkpath(Helix::dataDir())) {
        fail("Could not create " + Helix::dataDir());
        return;
    }
    m_zipPath = Helix::dataDir() + "/" + r.zipName;
    m_zip.setFileName(m_zipPath + ".part");
    if (!m_zip.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Could not write " + m_zip.fileName());
        return;
    }
    m_hash.reset();

    const QString title = QString("Downloading HelixSR %1").arg(r.version.toString());
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
        m_status = QString("%1: %2 of %3 MB").arg(title, megabytes(got), megabytes(size));
        emit stateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
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
        if (actual != m_expectedSha) {
            fail("The download does not match its published checksum, so it was discarded.");
            return;
        }
        logInfo("Download verified (SHA-256 " + actual + ")");
        QFile::remove(m_zipPath);
        if (!QFile::rename(m_zipPath + ".part", m_zipPath)) {
            fail("Could not rename the downloaded file.");
            return;
        }
        extractZip();
    });
}

void HelixSetup::extractZip()
{
    setPhase(Phase::Extracting, "Unpacking HelixSR...");
    const QString temp = Helix::dataDir() + "/extract";
    QDir(temp).removeRecursively();
    QDir().mkpath(temp);

    // tar.exe ships with Windows 10 and later and reads zip archives.
    const QString tar = qEnvironmentVariable("SystemRoot") + "/System32/tar.exe";
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, [this, temp](int code, QProcess::ExitStatus) {
        const QString output = QString::fromLocal8Bit(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        if (m_cancelled) {
            QDir(temp).removeRecursively();
            fail("Cancelled.");
            return;
        }
        if (code != 0) {
            QDir(temp).removeRecursively();
            fail("Unpacking failed: " + lastLine(output));
            return;
        }

        const QString name = "HelixSR-" + m_latest->version.toString();
        m_buildDir = Helix::dataDir() + "/" + name;
        QDir(m_buildDir).removeRecursively();   // a half-built earlier attempt
        // the zip holds one folder, normally named like the package
        QString inner = temp + "/" + name;
        if (!QFileInfo(inner).isDir()) {
            const auto dirs = QDir(temp).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
            inner = dirs.size() == 1 ? dirs.first().absoluteFilePath() : QString();
        }
        if (inner.isEmpty() || !QDir().rename(inner, m_buildDir)) {
            QDir(temp).removeRecursively();
            fail("The unpacked HelixSR folder was not what was expected.");
            return;
        }
        QDir(temp).removeRecursively();
        QFile::remove(m_zipPath);   // no longer needed
        buildNetwork();
    });
    m_process->start(tar, {"-xf", m_zipPath, "-C", temp});
}

void HelixSetup::buildNetwork()
{
    setPhase(Phase::Building, "Building the network on this PC (about 5 to 6 minutes)...", 0);
    logInfo("Running the HelixSR setup in " + m_buildDir);
    m_lineBuffer.clear();

    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    m_process->setWorkingDirectory(m_buildDir);
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    m_process->setProcessEnvironment(env);
    connect(m_process, &QProcess::readyRead, this, [this]() { handleBuildOutput(m_process->readAll()); });
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        handleBuildOutput(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        const bool built = status == QProcess::NormalExit && (code == 0 || m_networkBuilt);
        if (!built) {
            fail(QString("The HelixSR setup failed (exit code %1). See the log for its output.").arg(code));
            return;
        }
        if (code != 0)
            logWarn(QString("The setup exited with code %1 after building the network; carrying on.").arg(code));
        finishBuild();
    });

    // -Yes answers the one question the setup asks (the download of NVIDIA's DLSS DLL); the
    // user agreed to it in this app. The setup's game question at the end gets no input and
    // is skipped; games are installed from here instead.
    m_process->start("powershell.exe", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                        m_buildDir + "/helixsr-setup.ps1", "-Yes"});
    m_process->closeWriteChannel();
}

void HelixSetup::handleBuildOutput(const QByteArray& data)
{
    static const QRegularExpression bar(R"(\]\s+(\d+)%)");
    m_lineBuffer += data;
    for (;;) {
        // a line ends with \n (or a lone \r)
        int end = -1;
        for (int i = 0; i < m_lineBuffer.size() && end < 0; ++i) {
            if (m_lineBuffer[i] == '\n' || m_lineBuffer[i] == '\r')
                end = i;
        }
        if (end < 0)
            break;
        QString line = QString::fromUtf8(m_lineBuffer.left(end)).trimmed();
        m_lineBuffer.remove(0, end + 1);
        if (line.isEmpty())
            continue;
        if (line.contains("now carries the network"))
            m_networkBuilt = true;
        const auto m = bar.match(line);
        if (m.hasMatch())
            m_percent = m.captured(1).toInt();
        line.remove(QRegularExpression(R"(^\[helixsr-setup\]\s*)"));
        note(line);
    }
}

void HelixSetup::finishBuild()
{
    const std::optional<Helix::Version> built =
        m_latest ? std::optional<Helix::Version>(m_latest->version) : std::nullopt;
    const QString dll = m_buildDir + "/" + Helix::kFsrGenericDll;
    if (!built || !QFile::exists(dll) || !Helix::markBuilt(m_buildDir, *built)) {
        fail("The setup finished, but HelixSR's DLL is missing.");
        return;
    }
    cleanUp();
}

void HelixSetup::cleanUp()
{
    setPhase(Phase::Cleaning, "Removing what is no longer needed...");
    qint64 count = 0;
    QStringList doomed = Helix::buildLeftovers(m_buildDir, Helix::setupDataDir());
    doomed += Helix::otherPackages(Helix::dataDir(), m_buildDir);
    for (const QString& path : doomed) {
        if (removePath(path))
            ++count;
        else
            logWarn("Could not remove " + path);
    }
    logInfo(QString("Removed %1 leftover item(s)").arg(count));

    m_package = Helix::builtPackage(Helix::dataDir());
    m_buildDir.clear();
    const QString text = QString("HelixSR %1 is ready.").arg(m_package ? m_package->version.toString() : "?");
    setPhase(Phase::Idle, text);
    emit buildFinished(true, text);
}

void HelixSetup::cancel()
{
    // Installing into a game or removing from one is short and must finish its file moves.
    if (!busy() || m_phase == Phase::Installing || m_phase == Phase::Removing)
        return;
    m_cancelled = true;
    m_installAfterCheck = false;
    if (m_reply)
        m_reply->abort();
    killTree(m_process);   // its finished handler reports the cancellation
    if (!m_reply && !m_process)
        abortJob();
}

void HelixSetup::abortJob()
{
    if (m_zip.isOpen())
        m_zip.close();
    QFile::remove(m_zip.fileName());
    if (!m_buildDir.isEmpty()) {
        QDir(m_buildDir).removeRecursively();
        m_buildDir.clear();
    }
    setPhase(Phase::Idle, "Cancelled.");
}

void HelixSetup::fail(const QString& text)
{
    logError(text);
    if (m_zip.isOpen())
        m_zip.close();
    if (!m_zip.fileName().isEmpty())
        QFile::remove(m_zip.fileName());
    if (!m_buildDir.isEmpty()) {
        QDir(m_buildDir).removeRecursively();   // never leave a half-built package behind
        m_buildDir.clear();
    }
    setPhase(Phase::Idle, text);
    emit buildFinished(false, text);
}

// ---------------------------------------------------------------------------
// Games
// ---------------------------------------------------------------------------

void HelixSetup::installIntoGame(const QString& exePath) { runGameJob(exePath, true); }
void HelixSetup::removeFromGame(const QString& exePath)  { runGameJob(exePath, false); }

void HelixSetup::runGameJob(const QString& exePath, bool install)
{
    auto reject = [&](const QString& text) {
        logWarn(text);
        emit gameJobFinished(exePath, install, false, text);
    };
    if (busy())
        return reject("Another HelixSR job is running.");
    if (!ready())
        return reject("HelixSR is not set up yet.");

    const Helix::GameScan scan = Helix::scanGame(exePath);
    if (scan.targets.isEmpty())
        return reject("No AMD FSR 3.1 DLL (amd_fidelityfx_dx12.dll) was found in the game's folder.");

    // The driver is kept next to the data; written fresh so it always matches this app.
    QFile resource(":/helix/driver.py");
    const QString driver = Helix::dataDir() + "/helixsr_driver.py";
    QDir().mkpath(Helix::dataDir());
    QFile out(driver);
    if (!resource.open(QIODevice::ReadOnly) || !out.open(QIODevice::WriteOnly | QIODevice::Truncate)
        || out.write(resource.readAll()) < 0)
        return reject("Could not write the HelixSR helper script.");
    out.close();

    const QString verb = install ? "Installing HelixSR into" : "Removing HelixSR from";
    const QString name = QFileInfo(exePath).dir().dirName();
    setPhase(install ? Phase::Installing : Phase::Removing, QString("%1 %2...").arg(verb, name));
    logInfo(QString("%1 %2 (%3)").arg(verb, exePath, scan.root));

    auto output = std::make_shared<QString>();
    auto* p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    p->setWorkingDirectory(Helix::dataDir());
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    p->setProcessEnvironment(env);
    connect(p, &QProcess::readyRead, this, [p, output]() {
        const QString text = QString::fromUtf8(p->readAll());
        *output += text;
        for (const QString& line : text.split(QRegularExpression("[\r\n]+"), Qt::SkipEmptyParts))
            logInfo(line.trimmed());
    });
    connect(p, &QProcess::finished, this, [this, p, output, exePath, install](int, QProcess::ExitStatus) {
        *output += QString::fromUtf8(p->readAll());
        p->deleteLater();

        // The files decide, not the exit code.
        const auto after = Helix::scanGame(exePath);
        const bool installed = after.status == Helix::GameScan::Status::Installed;
        const bool ok = install ? installed : !installed;
        QString text;
        if (ok) {
            text = install ? "HelixSR is installed. Select AMD FSR in the game's graphics settings."
                           : "HelixSR was removed and the game's own DLL is back.";
        } else {
            text = lastLine(*output);
            if (text.isEmpty())
                text = install ? "The install did not complete." : "HelixSR could not be removed.";
        }
        setPhase(Phase::Idle, text);
        emit gameJobFinished(exePath, install, ok, text);
    });
    p->start(Helix::pythonExe(Helix::setupDataDir()),
             {driver, m_package->dir, install ? "install" : "remove", scan.root});
    p->closeWriteChannel();
}
