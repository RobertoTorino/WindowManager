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
#include <QTimer>

namespace {

constexpr int kIncompatibleExit = 2;   // helixsr_driver.py: HelixSR's installer is not what it expects

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

// Kills the process and everything it started (HelixSR's setup starts more Python processes).
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
    // A package whose installer was checked is trusted; a new package, a new helper script
    // (this app was updated) or an older marker is checked once, in the background.
    m_driverOk = m_package && m_package->driver == installerFingerprint();
    if (m_package && !m_driverOk)
        QTimer::singleShot(0, this, &HelixSetup::verifyDriver);
}

std::optional<Helix::Version> HelixSetup::installedVersion() const
{
    return m_package ? std::optional<Helix::Version>(m_package->version) : std::nullopt;
}

bool HelixSetup::updateAvailable() const
{
    return m_latest && m_package && Helix::isUpdate(installedVersion(), m_latest->version);
}

std::optional<Helix::Version> HelixSetup::unsupportedNewer() const
{
    if (m_newest && (!m_latest || *m_newest > m_latest->version))
        return m_newest;
    return std::nullopt;
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
            const QByteArray json = reply->readAll();
            m_latest = Helix::latestRelease(json);
            const auto newest = Helix::latestRelease(json, 0);
            m_newest = newest ? std::optional<Helix::Version>(newest->version) : std::nullopt;
            if (const auto newer = unsupportedNewer())
                logWarn(QString("HelixSR %1 is out, but this app sets up HelixSR up to %2.x only")
                            .arg(newer->toString()).arg(Helix::kSupportedMajor));
            if (!m_latest)
                error = QString("no HelixSR release (up to %1.x) with a setup zip was found").arg(Helix::kSupportedMajor);
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

void HelixSetup::fetchFile(const QString& url, const QString& dest, const QString& sha256, const QString& title,
                           qint64 sizeHint, std::function<void()> next)
{
    m_zip.setFileName(dest + ".part");
    if (!m_zip.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail("Could not write " + m_zip.fileName());
        return;
    }
    m_hash.reset();
    setPhase(Phase::Downloading, title + "...", 0);
    logInfo(title + " from " + url);

    QNetworkReply* reply = m_net->get(makeRequest(QUrl(url)));
    m_reply = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply]() {
        const QByteArray chunk = reply->readAll();
        m_hash.addData(chunk);
        if (m_zip.write(chunk) != chunk.size())
            reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this, title, sizeHint](qint64 got, qint64 total) {
        const qint64 size = total > 0 ? total : sizeHint;
        m_percent = size > 0 ? static_cast<int>(got * 100 / size) : -1;
        m_status = QString("%1: %2 of %3 MB").arg(title, megabytes(got), megabytes(size));
        emit stateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, dest, sha256, next]() {
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
        if (actual != sha256) {
            fail(QString("The download of %1 does not match its published checksum, so it was discarded.")
                     .arg(QFileInfo(dest).fileName()));
            return;
        }
        logInfo("Download verified (SHA-256 " + actual + ")");
        QFile::remove(dest);
        if (!QFile::rename(dest + ".part", dest)) {
            fail("Could not rename the downloaded file.");
            return;
        }
        next();
    });
}

void HelixSetup::unpack(const QString& archive, const QString& destDir, std::function<void(bool, const QString&)> done)
{
    QDir().mkpath(destDir);
    // tar.exe ships with Windows 10 and later and reads zip archives (a wheel is one too).
    const QString tar = qEnvironmentVariable("SystemRoot") + "/System32/tar.exe";
    m_process = new QProcess(this);
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::finished, this, [this, done](int code, QProcess::ExitStatus status) {
        const QString output = QString::fromLocal8Bit(m_process->readAll());
        m_process->deleteLater();
        m_process = nullptr;
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        done(status == QProcess::NormalExit && code == 0, output);
    });
    m_process->start(tar, {"-xf", archive, "-C", destDir});
}

void HelixSetup::downloadZip()
{
    const Helix::Release r = *m_latest;
    if (!QDir().mkpath(Helix::dataDir())) {
        fail("Could not create " + Helix::dataDir());
        return;
    }
    m_zipPath = Helix::dataDir() + "/" + r.zipName;
    fetchFile(r.zipUrl, m_zipPath, m_expectedSha, QString("Downloading HelixSR %1").arg(r.version.toString()),
              r.zipSize, [this]() { extractZip(); });
}

void HelixSetup::extractZip()
{
    setPhase(Phase::Extracting, "Unpacking HelixSR...");
    const QString temp = Helix::dataDir() + "/extract";
    QDir(temp).removeRecursively();
    unpack(m_zipPath, temp, [this, temp](bool ok, const QString& output) {
        if (!ok) {
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

        if (!QFile::exists(m_buildDir + "/" + Helix::kSetupScript) || !Helix::hasBridge(m_buildDir)) {
            fail("This HelixSR release has no setup/helixsr_setup.py and setup/helixsr_bridge.py, so WindowManager "
                 "cannot set it up. Update WindowManager.");
            return;
        }
        QFile ps1(m_buildDir + "/" + Helix::kSetupPs1);
        m_tools = ps1.open(QIODevice::ReadOnly) ? Helix::setupDownloads(QString::fromUtf8(ps1.readAll()))
                                                : QMap<QString, Helix::ToolDownload>{};
        for (const QString& tool : Helix::kSetupTools) {
            if (!m_tools.contains(tool)) {
                fail(QString("This HelixSR release's %1 does not say where to get %2, so WindowManager cannot set "
                             "it up. Update WindowManager.").arg(Helix::kSetupPs1, tool));
                return;
            }
        }
        prepareTools();
    });
}

// Gets what helixsr-setup.ps1 would get before building, one step per call: Python with numpy, then DXC,
// into %LOCALAPPDATA%\HelixSR. Each download is checked against the release's SHA-256.
void HelixSetup::prepareTools()
{
    const QString data = Helix::setupDataDir();
    if (!QDir().mkpath(data)) {
        fail("Could not create " + data);
        return;
    }
    const QString python = data + "/python";

    if (!QFileInfo::exists(python + "/numpy")) {
        const QString zip = data + "/python.zip";
        fetchFile(m_tools["python"].url, zip, m_tools["python"].sha256, "Downloading Python for the HelixSR setup", 0,
                  [this, data, python, zip]() {
            setPhase(Phase::Extracting, "Unpacking Python...");
            unpack(zip, python, [this, data, python, zip](bool ok, const QString& output) {
                QFile::remove(zip);
                if (!ok || !QFile::exists(Helix::pythonExe(data))) {
                    fail("Unpacking Python failed: " + lastLine(output));
                    return;
                }
                const QString wheel = data + "/numpy.zip";
                fetchFile(m_tools["numpy"].url, wheel, m_tools["numpy"].sha256, "Downloading numpy", 0,
                          [this, python, wheel]() {
                    setPhase(Phase::Extracting, "Unpacking numpy...");
                    unpack(wheel, python, [this, python, wheel](bool ok, const QString& output) {
                        QFile::remove(wheel);
                        if (!ok || !QFileInfo::exists(python + "/numpy")) {
                            fail("Unpacking numpy failed: " + lastLine(output));
                            return;
                        }
                        prepareTools();
                    });
                });
            });
        });
        return;
    }

    if (!QFile::exists(Helix::dxcExe(data))) {
        const QString zip = data + "/dxc.zip";
        fetchFile(m_tools["dxc"].url, zip, m_tools["dxc"].sha256, "Downloading the DirectX shader compiler", 0,
                  [this, data, zip]() {
            setPhase(Phase::Extracting, "Unpacking the DirectX shader compiler...");
            const QString temp = data + "/dxc-unpack";
            QDir(temp).removeRecursively();
            unpack(zip, temp, [this, data, zip, temp](bool ok, const QString& output) {
                QFile::remove(zip);
                // the compiler and its DLLs are in bin/x64
                const QDir bin(temp + "/bin/x64");
                bool copied = ok && bin.exists() && QDir().mkpath(data + "/dxc");
                for (const QFileInfo& f : bin.entryInfoList(QDir::Files)) {
                    const QString to = data + "/dxc/" + f.fileName();
                    QFile::remove(to);
                    copied = QFile::copy(f.absoluteFilePath(), to) && copied;
                }
                QDir(temp).removeRecursively();
                if (!copied || !QFile::exists(Helix::dxcExe(data))) {
                    fail("Unpacking the DirectX shader compiler failed: " + lastLine(output));
                    return;
                }
                prepareTools();
            });
        });
        return;
    }

    buildNetwork();
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
        // 3: the network is built, frame generation is not (it is not asked for)
        if (status != QProcess::NormalExit || (code != 0 && code != 3)) {
            fail(QString("The HelixSR setup failed (exit code %1). See the log for its output.").arg(code));
            return;
        }
        finishBuild();
    });

    // HelixSR's own build script, without helixsr-setup.ps1 around it (that one goes on to install into
    // every game it finds). --yes answers its one question, the download of NVIDIA's DLSS DLL: the user
    // agreed to it in this app.
    m_process->start(Helix::pythonExe(Helix::setupDataDir()),
                     Helix::setupBuildArgs(m_buildDir, Helix::dxcExe(Helix::setupDataDir())));
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
    if (!built || !QFile::exists(Helix::packageDll(m_buildDir))) {
        fail("The setup finished, but HelixSR's DLL is missing.");
        return;
    }
    setPhase(Phase::Building, "Checking the build...");
    runPython(Helix::setupStatusArgs(m_buildDir), [this, built](int, const QString& output) {
        if (m_cancelled) {
            fail("Cancelled.");
            return;
        }
        const auto st = Helix::parseBuildStatus(output);
        if (!st || !st->network) {
            fail("The setup finished, but the network was not built. See the log for its output.");
            return;
        }
        if (!Helix::markBuilt(m_buildDir, *built)) {
            fail("Could not write to " + m_buildDir);
            return;
        }
        cleanUp();
    });
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
    const QString ready = QString("HelixSR %1 is ready.").arg(m_package ? m_package->version.toString() : "?");

    // Before anything is installed into a game, check that the installer still works with this release.
    testDriver([this, ready](bool ok, const QString& problem) {
        emit buildFinished(true, ok ? ready
                                    : ready.chopped(1) + ", but installing into games is switched off: " + problem);
    });
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

QByteArray HelixSetup::driverScript()
{
    QFile resource(":/helix/driver.py");
    return resource.open(QIODevice::ReadOnly) ? resource.readAll() : QByteArray();
}

QString HelixSetup::installerFingerprint() const
{
    if (!m_package)
        return {};
    if (!Helix::hasBridge(m_package->dir))
        return Helix::driverFingerprint(driverScript());
    // HelixSR's own bridge, and the protocol this app speaks to it
    QFile bridge(m_package->dir + "/" + Helix::kBridgeScript);
    const QByteArray script = bridge.open(QIODevice::ReadOnly) ? bridge.readAll() : QByteArray();
    return Helix::driverFingerprint(QString("bridge %1\n").arg(Helix::kBridgeProtocol).toUtf8() + script);
}

QString HelixSetup::describeFailure(int exitCode, const QString& output)
{
    const QString line = lastLine(output);
    const QString incompatible = "This HelixSR version's installer is not what WindowManager expects (%1). "
                                 "Update WindowManager, or install by hand with helixsr-setup in the HelixSR folder.";
    if (line.startsWith('{')) {   // HelixSR's bridge
        const Helix::BridgeReply reply = Helix::parseBridgeReply(output);
        if (!reply.compatible)
            return incompatible.arg("its answer is not bridge protocol " + QString::number(Helix::kBridgeProtocol));
        if (!reply.error.isEmpty())
            return reply.error;
    }
    if (exitCode == kIncompatibleExit) {
        QString detail = line;
        detail.remove("driver-incompatible: ");
        return incompatible.arg(detail);
    }
    return line.isEmpty() ? QString("The HelixSR installer failed (exit code %1).").arg(exitCode) : line;
}

void HelixSetup::runPython(const QStringList& args, std::function<void(int, const QString&)> done)
{
    auto output = std::make_shared<QString>();
    auto* p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    QDir().mkpath(Helix::dataDir());
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
    connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;   // a crash still ends in finished()
        p->deleteLater();
        done(-1, "Could not start Python: " + p->errorString());
    });
    connect(p, &QProcess::finished, this, [p, output, done](int code, QProcess::ExitStatus status) {
        *output += QString::fromUtf8(p->readAll());
        p->deleteLater();
        done(status == QProcess::NormalExit ? code : -1, *output);
    });
    p->start(Helix::pythonExe(Helix::setupDataDir()), args);
    p->closeWriteChannel();
}

void HelixSetup::runInstaller(bool install, const QString& folder, std::function<void(int, const QString&)> done)
{
    if (Helix::hasBridge(m_package->dir)) {
        runPython(Helix::bridgeArgs(m_package->dir, install, folder), done);
        return;
    }

    // HelixSR 1.x: the helper is written fresh next to the data so it always matches this app.
    const QByteArray script = driverScript();
    const QString driver = Helix::dataDir() + "/helixsr_driver.py";
    QDir().mkpath(Helix::dataDir());
    QFile out(driver);
    if (script.isEmpty() || !out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(script) != script.size()) {
        done(-1, "Could not write the HelixSR helper script.");
        return;
    }
    out.close();
    runPython({driver, m_package->dir, install ? "install" : "remove", folder}, done);
}

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
    if (!m_driverOk)
        return reject(m_driverProblem.isEmpty() ? QString("The HelixSR installer has not been checked yet.")
                                                : m_driverProblem);

    const QString verb = install ? "Installing HelixSR into" : "Removing HelixSR from";
    setPhase(install ? Phase::Installing : Phase::Removing,
             QString("%1 %2...").arg(verb, QFileInfo(exePath).dir().dirName()));   // busy from now on

    auto finish = [this, exePath, install](bool ok, const QString& text) {
        setPhase(Phase::Idle, text);
        emit gameJobFinished(exePath, install, ok, text);
    };

    // Looking through the game's folder can take a while: not on the UI thread.
    Helix::scanGameAsync(this, exePath, [this, exePath, install, verb, finish](const Helix::GameScan& scan) {
        if (scan.targets.isEmpty()) {
            const QString text = "No AMD FSR 3.1 DLL (amd_fidelityfx_dx12.dll) was found in the game's folder.";
            logWarn(text);
            finish(false, text);
            return;
        }
        logInfo(QString("%1 %2 (%3)").arg(verb, exePath, scan.root));

        runInstaller(install, scan.root, [this, exePath, install, finish](int code, const QString& output) {
            // The files decide, not the exit code.
            Helix::scanGameAsync(this, exePath, [install, code, output, finish](const Helix::GameScan& after) {
                const bool installed = after.status == Helix::GameScan::Status::Installed;
                const bool ok = install ? installed : !installed;
                QString text;
                if (ok) {
                    text = install ? "HelixSR is installed. Select AMD FSR in the game's graphics settings."
                                   : "HelixSR was removed and the game's own DLL is back.";
                } else {
                    text = code == 0 ? (install ? QString("The install did not complete.")
                                                : QString("HelixSR could not be removed."))
                                     : describeFailure(code, output);
                }
                finish(ok, text);
            });
        });
    });
}

// ---------------------------------------------------------------------------
// Self-test of the install helper
// ---------------------------------------------------------------------------

void HelixSetup::verifyDriver()
{
    if (busy() || !m_package)
        return;
    if (!QFile::exists(Helix::pythonExe(Helix::setupDataDir()))) {
        m_driverOk = false;
        m_driverProblem = "Python of the HelixSR setup is missing (" + Helix::setupDataDir() + "/python).";
        setPhase(Phase::Idle, m_driverProblem);
        return;
    }
    testDriver([this](bool ok, const QString& text) { emit driverChecked(ok, text); });
}

// Installs into and removes from a throw-away "game" with the built package, so a HelixSR release
// whose installer changed is noticed right after the build, not in front of a real game.
void HelixSetup::testDriver(std::function<void(bool, const QString&)> done)
{
    const QString root = Helix::dataDir() + "/selftest";
    const QString bin = root + "/bin";
    const QByteArray ownDll = "MZ self-test: the game's own FSR DLL";
    QDir(root).removeRecursively();
    QDir().mkpath(bin);
    for (const auto& file : {std::pair<QString, QByteArray>{"Game.exe", "MZ self-test"},
                             std::pair<QString, QByteArray>{Helix::kFsrGenericDll, ownDll}}) {
        QFile f(bin + "/" + file.first);
        if (!f.open(QIODevice::WriteOnly) || f.write(file.second) < 0) {
            m_driverOk = false;
            m_driverProblem = "Could not create the self-test folder in " + root;
            QDir(root).removeRecursively();
            setPhase(Phase::Idle, m_driverProblem);
            done(false, m_driverProblem);
            return;
        }
    }

    setPhase(Phase::Testing, "Testing the installer against this HelixSR version...");
    auto finish = [this, root, done](bool ok, const QString& text) {
        QDir(root).removeRecursively();
        m_driverOk = ok;
        m_driverProblem = ok ? QString() : text;
        if (ok && m_package) {
            m_package->driver = installerFingerprint();
            Helix::markBuilt(m_package->dir, m_package->version, m_package->driver);
        }
        logInfo(ok ? "Installer self-test passed" : "Installer self-test FAILED: " + text);
        setPhase(Phase::Idle, ok ? "The installer works with this HelixSR version." : text);
        done(ok, text);
    };

    const QString exe = bin + "/Game.exe";
    runInstaller(true, bin, [this, exe, bin, ownDll, finish](int code, const QString& output) {
        Helix::scanGameAsync(this, exe, [this, bin, ownDll, finish, code, output](const Helix::GameScan& installed) {
            if (installed.status != Helix::GameScan::Status::Installed) {
                finish(false, describeFailure(code, output));
                return;
            }
            runInstaller(false, bin, [bin, ownDll, finish](int code2, const QString& output2) {
                QFile restored(bin + "/" + Helix::kFsrGenericDll);
                const bool back = restored.open(QIODevice::ReadOnly) && restored.readAll() == ownDll;
                restored.close();   // finish() deletes the folder, which an open file blocks on Windows
                if (!back) {
                    finish(false, describeFailure(code2, output2) + " (removing did not restore the game's DLL)");
                    return;
                }
                finish(true, QString());
            });
        });
    });
}
