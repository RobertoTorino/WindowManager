#include "HelixPanel.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "core/Logger.h"
#include "helix/HelixSetup.h"
#include "helix/SystemProbe.h"
#include "ui/GameRegistry.h"
#include "ui/Theme.h"

#include <algorithm>

#include <QDir>
#include <QComboBox>
#include <QDesktopServices>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QUrl>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kPathRole = Qt::UserRole;
constexpr int kNameRole = Qt::UserRole + 1;

QString clockText(qint64 ms)
{
    const int s = static_cast<int>(ms / 1000);
    return QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}

QString gameName(const QString& path)
{
    const auto game = AppConfig::instance().gameByPath(path);
    return game && !game->savedName.isEmpty() ? game->savedName : QFileInfo(path).dir().dirName();
}

} // namespace

HelixPanel::HelixPanel(WindowManagerController* c, QWidget* parent)
    : ControlPanel(c, parent)
    , m_setup(new HelixSetup(this))
    , m_clock(new QTimer(this))
{
    // --- HelixSR itself ---
    m_version = new QLabel(this);
    m_system = new QLabel("Checking this PC...", this);
    m_system->setWordWrap(true);
    m_checkPc = Theme::button("Check this PC", this, nullptr,
                              "Test the graphics card, driver and disk space against what HelixSR needs");
    m_state = new QLabel(this);
    m_state->setObjectName("hint");
    m_state->setWordWrap(true);
    m_driverWarning = new QLabel(this);
    m_driverWarning->setWordWrap(true);
    m_driverWarning->setVisible(false);
    m_openFolder = Theme::button("Open folder", this, nullptr, "Show the HelixSR folder (for installing by hand with helixsr-setup)");
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(true);
    m_elapsed = new QLabel(this);
    m_setupButton = Theme::button("Set up HelixSR", this, "accent",
                                  "Download HelixSR, build its network on this PC and remove what is no longer needed");
    m_check = Theme::button("Check for updates", this, nullptr, "Look for a newer HelixSR release on GitHub");
    m_cancel = Theme::button("Cancel", this, "danger", "Stop the download or build and remove its leftovers");

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_setupButton);
    buttons->addWidget(m_check);
    buttons->addWidget(m_checkPc);
    buttons->addWidget(m_openFolder);
    buttons->addWidget(m_cancel);
    buttons->addStretch(1);

    auto* progressRow = new QHBoxLayout;
    progressRow->addWidget(m_progress, 1);
    progressRow->addWidget(m_elapsed);

    auto* top = new QVBoxLayout;
    top->addWidget(m_version);
    top->addWidget(m_system);
    top->addLayout(buttons);
    top->addLayout(progressRow);
    top->addWidget(m_state);
    top->addWidget(m_driverWarning);

    // --- games ---
    m_games = new QListWidget(this);
    m_games->setMinimumHeight(80);
    m_games->setMaximumHeight(130);
    m_install = Theme::button("Install", this, nullptr,
                              "Replace the game's FSR 3.1 DLL with HelixSR (the game's own file is kept)");
    m_remove = Theme::button("Remove", this, nullptr, "Put the game's own FSR DLL back");
    m_run = Theme::button("Run with HelixSR", this, "accent",
                          "Install HelixSR if needed, then start the game. In the game, select AMD FSR.");
    m_gameHint = new QLabel(this);
    m_gameHint->setObjectName("hint");
    m_gameHint->setWordWrap(true);
    m_gameHint->setText("HelixSR replaces a game's AMD FSR 3.1 DLL. Games without one are greyed out. "
                        "In the game, choose AMD FSR as the upscaler.");

    // helixsr.ini of the selected game (only when HelixSR is installed in it)
    m_sharpener = new QComboBox(this);
    for (const QString& s : Helix::kSharpeners)
        m_sharpener->addItem(s);
    m_sharpener->setToolTip("off, nvidia or rcas: an optional sharpening pass after upscaling");
    m_sharpness = new QDoubleSpinBox(this);
    m_sharpness->setRange(0.0, 1.0);
    m_sharpness->setSingleStep(0.05);
    m_sharpness->setDecimals(2);
    m_sharpness->setToolTip("0.0 to 1.0, used when a sharpener is on");
    m_waveSize = new QComboBox(this);
    for (const QString& s : Helix::kWaveSizes)
        m_waveSize->addItem(s);
    m_waveSize->setToolTip("Leave on auto unless the picture is broken (32 or 64 force the SIMD width)");
    m_applyIni = Theme::button("Save settings", this, nullptr,
                               "Write helixsr.ini next to HelixSR's DLL; restart the game for it to apply");

    auto* iniRow = new QHBoxLayout;
    iniRow->addWidget(new QLabel("Sharpening", this));
    iniRow->addWidget(m_sharpener);
    iniRow->addWidget(new QLabel("Strength", this));
    iniRow->addWidget(m_sharpness);
    iniRow->addWidget(new QLabel("Wave size", this));
    iniRow->addWidget(m_waveSize);
    iniRow->addWidget(m_applyIni);
    iniRow->addStretch(1);

    auto* gameButtons = new QHBoxLayout;
    gameButtons->addStretch(1);
    gameButtons->addWidget(m_remove);
    gameButtons->addWidget(m_install);
    gameButtons->addWidget(m_run);

    auto* games = new QVBoxLayout;
    games->addWidget(m_games, 1);
    games->addLayout(gameButtons);
    games->addLayout(iniRow);
    games->addWidget(m_gameHint);

    auto* topBox = new QGroupBox("HelixSR (FSR 3.1 upscaler for AMD GPUs)", this);
    topBox->setLayout(top);
    auto* gamesBox = new QGroupBox("Games", this);
    gamesBox->setLayout(games);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setSpacing(10);
    grid->addWidget(topBox, 0, 0);
    grid->addWidget(gamesBox, 1, 0);
    grid->setColumnStretch(0, 1);

    // --- wiring ---
    connect(m_setupButton, &QPushButton::clicked, this, &HelixPanel::setUpOrUpdate);
    connect(m_check, &QPushButton::clicked, m_setup, &HelixSetup::checkForUpdate);
    connect(m_openFolder, &QPushButton::clicked, this, [this]() {
        if (!m_setup->packageDir().isEmpty())
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_setup->packageDir()));
    });
    connect(m_applyIni, &QPushButton::clicked, this, &HelixPanel::saveIni);
    connect(m_sharpener, &QComboBox::currentTextChanged, this, [this](const QString& value) {
        m_sharpness->setEnabled(m_applyIni->isEnabled() && value != "off");
    });
    connect(m_checkPc, &QPushButton::clicked, this, &HelixPanel::showSystemCheck);
    connect(m_cancel, &QPushButton::clicked, m_setup, &HelixSetup::cancel);
    connect(m_install, &QPushButton::clicked, this, &HelixPanel::installSelected);
    connect(m_remove, &QPushButton::clicked, this, &HelixPanel::removeSelected);
    connect(m_run, &QPushButton::clicked, this, &HelixPanel::runSelected);
    connect(m_games, &QListWidget::itemSelectionChanged, this, [this]() {
        loadIni();
        updateButtons();
    });
    connect(m_games, &QListWidget::itemDoubleClicked, this, &HelixPanel::runSelected);

    connect(m_clock, &QTimer::timeout, this, &HelixPanel::updateSetupLabels);
    connect(m_setup, &HelixSetup::stateChanged, this, [this]() {
        if (m_setup->busy() && !m_clock->isActive()) {
            m_elapsedTimer.start();
            m_clock->start(1000);
        } else if (!m_setup->busy()) {
            m_clock->stop();
        }
        updateSetupLabels();
        updateButtons();
    });
    connect(m_setup, &HelixSetup::updateChecked, this, [this](bool ok) {
        if (!ok)
            emit message("HelixSR update check failed: " + m_setup->lastError());
        else if (m_setup->updateAvailable())
            emit message(QString("HelixSR %1 is available (see the HelixSR tab).")
                             .arg(m_setup->latest()->version.toString()));
        rebuildGames();
    });
    connect(m_setup, &HelixSetup::buildFinished, this, [this](bool ok, const QString& text) {
        emit message(text);
        if (!ok || !m_setup->driverOk()) {
            rebuildGames();
            QMessageBox::warning(this, "HelixSR", text);
            return;
        }
        rebuildGames([this]() { updateOutdatedGames(); });
    });
    connect(m_setup, &HelixSetup::driverChecked, this, [this](bool ok, const QString& text) {
        emit message(ok ? QString("HelixSR's installer checked: it works with this version.") : text);
        rebuildGames();
    });
    connect(m_setup, &HelixSetup::gameJobFinished, this,
            [this](const QString& path, bool install, bool ok, const QString& text) {
        emit message(gameName(path) + ": " + text);
        rebuildGames();
        const bool run = install && m_runAfterInstall == path;
        if (m_runAfterInstall == path)
            m_runAfterInstall.clear();
        if (run && ok)
            emit runRequested(path);
        else if (!ok)
            QMessageBox::warning(this, "HelixSR - " + gameName(path), text);
        if (!m_queue.isEmpty())
            nextQueued();
    });

    rebuildGames();
    updateSetupLabels();
    updateButtons();
    QTimer::singleShot(0, this, &HelixPanel::runSystemCheck);   // creates GPU devices: after the window shows

    // Only a PC that has HelixSR checks by itself; the check is one small request to GitHub.
    QTimer::singleShot(3000, this, [this]() {
        if (m_setup->installedVersion() && !m_setup->busy())
            m_setup->checkForUpdate();
    });
}

bool HelixPanel::updateAvailable() const
{
    return m_setup->updateAvailable();
}

void HelixPanel::refresh()
{
    updateButtons();
}

// ---------------------------------------------------------------------------
// Labels and buttons
// ---------------------------------------------------------------------------

void HelixPanel::updateSetupLabels()
{
    const auto installed = m_setup->installedVersion();
    const auto latest = m_setup->latest();
    const bool busy = m_setup->busy();

    QString version = installed ? QString("Installed: HelixSR %1").arg(installed->toString())
                                : QString("HelixSR is not set up on this PC yet.");
    if (latest)
        version += QString("    Latest: %1").arg(latest->version.toString());
    if (const auto newer = m_setup->unsupportedNewer())
        version += QString("    (HelixSR %1 is out; this app does not support it yet)").arg(newer->toString());
    m_version->setText(version);

    m_state->setText(busy || !m_setup->status().isEmpty()
                         ? m_setup->status()
                         : QString("The first set-up downloads about 140 MB and builds the network on this PC "
                                   "(5 to 6 minutes)."));

    const QString problem = m_setup->driverProblem();
    m_driverWarning->setVisible(installed && !busy && !problem.isEmpty());
    m_driverWarning->setText("Installing into games is switched off: " + problem);
    m_openFolder->setEnabled(!m_setup->packageDir().isEmpty());

    m_progress->setVisible(busy);
    m_elapsed->setVisible(busy);
    if (busy) {
        if (m_setup->percent() >= 0) {
            m_progress->setRange(0, 100);
            m_progress->setValue(m_setup->percent());
        } else {
            m_progress->setRange(0, 0);
        }
        m_elapsed->setText("Elapsed " + clockText(m_elapsedTimer.elapsed()));
    }

    if (!installed)
        m_setupButton->setText("Set up HelixSR");
    else if (m_setup->updateAvailable())
        m_setupButton->setText(QString("Update to %1").arg(latest->version.toString()));
    else
        m_setupButton->setText("Up to date");

    const bool flag = m_setup->updateAvailable();
    if (flag != m_lastUpdateFlag) {
        m_lastUpdateFlag = flag;
        emit updateAvailableChanged(flag);
    }
}

void HelixPanel::updateButtons()
{
    using Status = Helix::GameScan::Status;
    const bool busy = m_setup->busy();
    const bool installed = m_setup->installedVersion().has_value();
    const auto phase = m_setup->phase();

    m_setupButton->setEnabled(!busy && (!installed || m_setup->updateAvailable()));
    m_check->setEnabled(!busy);
    m_checkPc->setEnabled(!busy);
    m_cancel->setVisible(busy && phase != HelixSetup::Phase::Installing && phase != HelixSetup::Phase::Removing
                         && phase != HelixSetup::Phase::Cleaning);

    const QString path = selectedPath();
    const auto scan = scanOf(path);
    const bool ready = m_setup->canInstall();

    m_install->setText(scan && scan->status == Status::Installed ? "Reinstall" : "Install");
    m_install->setEnabled(!busy && ready && scan && scan->status != Status::Unsupported);
    m_remove->setEnabled(!busy && ready && scan
                         && (scan->status == Status::Installed || scan->status == Status::Replaced));
    m_run->setEnabled(!busy && !m_checking && ready && scan && scan->status != Status::Unsupported);
    m_run->setToolTip(ready ? QString("Install HelixSR if needed, then start the game. In the game, select AMD FSR.")
                      : !m_setup->installedVersion() ? QString("Set up HelixSR first.")
                                                     : QString("Installing into games is switched off (see the warning above)."));

    const bool iniUsable = !busy && scan && scan->status == Status::Installed;
    for (QWidget* w : {static_cast<QWidget*>(m_sharpener), static_cast<QWidget*>(m_waveSize), static_cast<QWidget*>(m_applyIni)})
        w->setEnabled(iniUsable);
    m_sharpness->setEnabled(iniUsable && m_sharpener->currentText() != "off");
}

// ---------------------------------------------------------------------------
// Games
// ---------------------------------------------------------------------------

QString HelixPanel::selectedPath() const
{
    const auto* item = m_games->currentItem();
    return item ? item->data(kPathRole).toString() : QString();
}

std::optional<Helix::GameScan> HelixPanel::scanOf(const QString& exePath) const
{
    const auto it = m_scans.constFind(exePath);
    if (it == m_scans.constEnd())
        return std::nullopt;
    return *it;
}

bool HelixPanel::needsUpdate(const Helix::GameScan& scan) const
{
    const auto current = m_setup->installedVersion();
    return scan.status == Helix::GameScan::Status::Installed && scan.installedVersion && current
           && *scan.installedVersion < *current;
}

QString HelixPanel::statusText(const Helix::GameScan& scan) const
{
    using Status = Helix::GameScan::Status;
    switch (scan.status) {
    case Status::Unsupported:
        return "no AMD FSR 3.1 DLL: HelixSR cannot be used";
    case Status::Ready:
        return "not installed";
    case Status::Replaced:
        return "the game put its own DLL back: install again";
    case Status::Installed:
        break;
    }
    QString text = scan.installedVersion ? QString("HelixSR %1 installed").arg(scan.installedVersion->toString())
                                         : QString("HelixSR installed");
    if (needsUpdate(scan))
        text += QString(" - %1 is available").arg(m_setup->installedVersion()->toString());
    return text;
}

void HelixPanel::gamesChanged()
{
    if (!m_setup->busy())   // a running job rebuilds the list when it ends
        rebuildGames();
}

void HelixPanel::rebuildGames(std::function<void()> then)
{
    const QString keep = selectedPath();
    m_games->clear();
    m_scans.clear();
    ++m_scanGeneration;
    m_afterScans = std::move(then);

    // Every game; those that had an FSR 3.1 DLL last time first. The scans below have the final word.
    auto games = AppConfig::instance().games();
    std::sort(games.begin(), games.end(), [](const auto& a, const auto& b) {
        if (a.helixConfirmed != b.helixConfirmed)
            return a.helixConfirmed;
        return a.savedName.toUpper() < b.savedName.toUpper();
    });

    // The list shows at once; each game's folder is looked through in the background.
    m_pendingScans = static_cast<int>(games.size());
    for (const auto& g : games) {
        auto* item = new QListWidgetItem(QString("%1  -  checking...").arg(g.savedName), m_games);
        item->setData(kPathRole, g.applicationPath);
        item->setData(kNameRole, g.savedName);
        item->setToolTip(g.applicationPath);
        if (g.applicationPath == keep)
            m_games->setCurrentItem(item);

        Helix::scanGameAsync(this, g.applicationPath,
                             [this, generation = m_scanGeneration, path = g.applicationPath](const Helix::GameScan& scan) {
            if (generation != m_scanGeneration)
                return;   // the list was rebuilt meanwhile
            m_scans.insert(path, scan);
            const bool usable = scan.status != Helix::GameScan::Status::Unsupported;
            for (int i = 0; i < m_games->count(); ++i) {
                auto* row = m_games->item(i);
                if (row->data(kPathRole).toString() != path)
                    continue;
                row->setText(QString("%1  -  %2").arg(row->data(kNameRole).toString(), statusText(scan)));
                if (!usable) {
                    row->setFlags(row->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
                    if (row == m_games->currentItem())
                        m_games->setCurrentItem(nullptr);
                }
            }
            if (auto entry = AppConfig::instance().gameByPath(path); entry && entry->helixConfirmed != usable) {
                entry->helixConfirmed = usable;   // remembered for the order of the list next time
                AppConfig::instance().saveGame(*entry);
            }
            if (path == selectedPath())
                loadIni();
            updateButtons();
            if (--m_pendingScans == 0 && m_afterScans) {
                auto after = std::move(m_afterScans);
                m_afterScans = nullptr;
                after();
            }
        });
    }
    if (!m_games->currentItem() && m_games->count() > 0)
        m_games->setCurrentRow(0);
    if (games.isEmpty() && m_afterScans) {
        auto after = std::move(m_afterScans);
        m_afterScans = nullptr;
        after();
    }
    loadIni();
    updateButtons();
    updateSetupLabels();
}

void HelixPanel::installSelected()
{
    const QString path = selectedPath();
    if (!path.isEmpty())
        m_setup->installIntoGame(path);
}

void HelixPanel::removeSelected()
{
    const QString path = selectedPath();
    if (!path.isEmpty())
        m_setup->removeFromGame(path);
}

void HelixPanel::runSelected()
{
    using Status = Helix::GameScan::Status;
    const QString path = selectedPath();
    if (path.isEmpty() || m_checking || m_setup->busy() || !m_setup->canInstall())
        return;

    // the game may have been updated since the list was built: look again, in the background
    m_checking = true;
    updateButtons();
    Helix::scanGameAsync(this, path, [this, path](const Helix::GameScan& scan) {
        m_checking = false;
        updateButtons();
        if (m_setup->busy())
            return;
        if (scan.status == Status::Unsupported) {
            rebuildGames();
            QMessageBox::warning(this, "HelixSR - " + gameName(path),
                                 "The game no longer has an AMD FSR 3.1 DLL, so HelixSR cannot be used.");
            return;
        }
        if (scan.status == Status::Installed) {
            emit runRequested(path);
            return;
        }
        m_runAfterInstall = path;
        m_setup->installIntoGame(path);   // starts the game when it succeeds
    });
}

// ---------------------------------------------------------------------------
// helixsr.ini of the selected game
// ---------------------------------------------------------------------------

// The helixsr.ini files next to HelixSR's DLLs in the game.
QStringList HelixPanel::iniPaths(const Helix::GameScan& scan) const
{
    QStringList out;
    for (const QString& dll : scan.targets) {
        if (Helix::isHelixDll(dll))
            out << QFileInfo(dll).absolutePath() + "/helixsr.ini";
    }
    return out;
}

void HelixPanel::loadIni()
{
    Helix::IniSettings settings;
    const auto scan = scanOf(selectedPath());
    if (scan && scan->status == Helix::GameScan::Status::Installed) {
        const QStringList paths = iniPaths(*scan);
        QFile f(paths.isEmpty() ? QString() : paths.first());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            settings = Helix::parseIni(QString::fromUtf8(f.readAll()));
    }
    const QSignalBlocker a(m_sharpener), b(m_sharpness), c(m_waveSize);
    m_sharpener->setCurrentText(settings.sharpener);
    m_sharpness->setValue(settings.sharpness);
    m_waveSize->setCurrentText(settings.waveSize);
}

void HelixPanel::saveIni()
{
    const auto scan = scanOf(selectedPath());
    if (!scan || scan->status != Helix::GameScan::Status::Installed)
        return;

    Helix::IniSettings settings;
    settings.sharpener = m_sharpener->currentText();
    settings.sharpness = m_sharpness->value();
    settings.waveSize = m_waveSize->currentText();

    int saved = 0;
    for (const QString& path : iniPaths(*scan)) {
        // keep what the file already says (comments, other keys); a missing file is created
        QString text;
        QFile in(path);
        if (in.open(QIODevice::ReadOnly | QIODevice::Text)) {
            text = QString::fromUtf8(in.readAll());
            in.close();
        }
        QFile out(path);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || out.write(Helix::applyIni(text, settings).toUtf8()) < 0) {
            QMessageBox::warning(this, "HelixSR - " + gameName(selectedPath()),
                                 "Could not write " + path + ".\nIs the game still running?");
            return;
        }
        ++saved;
    }
    emit message(QString("%1: HelixSR settings saved (%2 file%3). Restart the game for them to apply.")
                     .arg(gameName(selectedPath())).arg(saved).arg(saved == 1 ? "" : "s"));
}

// ---------------------------------------------------------------------------
// Set-up and updates
// ---------------------------------------------------------------------------

void HelixPanel::runSystemCheck()
{
    m_verdict = Helix::evaluate(Helix::probeSystem(Helix::dataDir()));
    Logger::instance().info(("System check: " + Helix::levelName(m_verdict.level) + " - " + m_verdict.summary).toStdString(),
                            "HelixSR");
    Logger::instance().info(("System check details: " + m_verdict.details().replace('\n', " | ")).toStdString(), "HelixSR");
    m_system->setText("This PC: " + m_verdict.summary);
    m_system->setToolTip(m_verdict.details());
}

void HelixPanel::showSystemCheck()
{
    runSystemCheck();
    const auto icon = m_verdict.level == Helix::Level::Supported      ? QMessageBox::Information
                      : m_verdict.level == Helix::Level::Experimental ? QMessageBox::Warning
                                                                      : QMessageBox::Critical;
    QMessageBox box(icon, "HelixSR - this PC", m_verdict.summary, QMessageBox::Ok, this);
    box.setInformativeText(m_verdict.details());
    box.exec();
}

void HelixPanel::setUpOrUpdate()
{
    if (m_setup->busy())
        return;

    // Check the PC first: building takes minutes and is pointless on a card that cannot run it.
    runSystemCheck();
    if (m_verdict.level == Helix::Level::Unsupported) {
        QMessageBox box(QMessageBox::Critical, "HelixSR cannot run on this PC", m_verdict.summary,
                        QMessageBox::Ok, this);
        box.setInformativeText(m_verdict.details());
        box.exec();
        return;
    }

    const auto latest = m_setup->latest();
    const QString version = latest ? latest->version.toString() : QString("the latest version");
    const QString size = latest && latest->zipSize > 0 ? QString("about %1 MB").arg(latest->zipSize / 1048576)
                                                      : QString("about 140 MB");

    QMessageBox box(QMessageBox::Question, "Set up HelixSR",
                    QString("Download and build HelixSR %1?").arg(version),
                    QMessageBox::NoButton, this);
    const QString warning = m_verdict.level == Helix::Level::Experimental
                                ? QString("This PC: %1\n\n").arg(m_verdict.summary)
                                : QString();
    box.setInformativeText(warning +
        QString("- HelixSR (%1) is downloaded from github.com/lonewolf0622/HelixSR and checked against its "
                "published SHA-256.\n"
                "- HelixSR's own setup then downloads Python, a shader compiler and NVIDIA's DLSS 310.7.0 DLL "
                "(NVIDIA's licence: github.com/NVIDIA/DLSS/blob/v310.7.0/LICENSE.txt) and builds the network on "
                "this PC. This takes about 5 to 6 minutes.\n"
                "- The built DLL contains NVIDIA's network. Keep it on this PC and do not share it.\n"
                "- The downloads, the DLSS DLL and the build tools are deleted afterwards.").arg(size));
    auto* go = box.addButton("Download and build", QMessageBox::AcceptRole);
    auto* cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.exec();
    if (box.clickedButton() != go)
        return;
    m_setup->installOrUpdate();
}

void HelixPanel::updateOutdatedGames()
{
    QStringList outdated;
    for (auto it = m_scans.constBegin(); it != m_scans.constEnd(); ++it) {
        if (needsUpdate(it.value()))
            outdated << it.key();
    }
    if (outdated.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, "HelixSR",
        QString("%1 game(s) still have the older HelixSR. Update them to %2 now?")
            .arg(outdated.size()).arg(m_setup->installedVersion()->toString()));
    if (answer != QMessageBox::Yes)
        return;
    m_queue = outdated;
    nextQueued();
}

void HelixPanel::nextQueued()
{
    if (m_queue.isEmpty() || m_setup->busy())
        return;
    m_setup->installIntoGame(m_queue.takeFirst());
}
