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
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kPathRole = Qt::UserRole;

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

    // --- games ---
    m_games = new QListWidget(this);
    m_games->setMinimumHeight(80);
    m_games->setMaximumHeight(130);
    m_confirm = Theme::button("Check selected game", this, nullptr,
                              "Look for an AMD FSR 3.1 DLL in the game selected in the game bar and add it to this list");
    m_install = Theme::button("Install", this, nullptr,
                              "Replace the game's FSR 3.1 DLL with HelixSR (the game's own file is kept)");
    m_remove = Theme::button("Remove", this, nullptr, "Put the game's own FSR DLL back");
    m_forget = Theme::button("Forget", this, nullptr, "Take the game off this list (HelixSR stays in it if installed)");
    m_run = Theme::button("Run with HelixSR", this, "accent",
                          "Install HelixSR if needed, then start the game. In the game, select AMD FSR.");
    m_gameHint = new QLabel(this);
    m_gameHint->setObjectName("hint");
    m_gameHint->setWordWrap(true);
    m_gameHint->setText("Select a game in the game bar, then press \"Check selected game\". Games with an AMD FSR 3.1 "
                        "DLL are listed here. In the game, choose AMD FSR as the upscaler.");

    auto* gameButtons = new QHBoxLayout;
    gameButtons->addWidget(m_confirm);
    gameButtons->addStretch(1);
    gameButtons->addWidget(m_forget);
    gameButtons->addWidget(m_remove);
    gameButtons->addWidget(m_install);
    gameButtons->addWidget(m_run);

    auto* games = new QVBoxLayout;
    games->addWidget(m_games, 1);
    games->addLayout(gameButtons);
    games->addWidget(m_gameHint);

    auto* topBox = new QGroupBox("HelixSR (FSR 3.1 upscaler for AMD GPUs)", this);
    topBox->setLayout(top);
    auto* gamesBox = new QGroupBox("Games with HelixSR", this);
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
    connect(m_checkPc, &QPushButton::clicked, this, &HelixPanel::showSystemCheck);
    connect(m_cancel, &QPushButton::clicked, m_setup, &HelixSetup::cancel);
    connect(m_confirm, &QPushButton::clicked, this, &HelixPanel::confirmCurrentGame);
    connect(m_forget, &QPushButton::clicked, this, &HelixPanel::forgetSelectedGame);
    connect(m_install, &QPushButton::clicked, this, &HelixPanel::installSelected);
    connect(m_remove, &QPushButton::clicked, this, &HelixPanel::removeSelected);
    connect(m_run, &QPushButton::clicked, this, &HelixPanel::runSelected);
    connect(m_games, &QListWidget::itemSelectionChanged, this, &HelixPanel::updateButtons);
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
        rebuildGames();
        if (!ok) {
            QMessageBox::warning(this, "HelixSR", text);
            return;
        }
        updateOutdatedGames();
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
    m_version->setText(version);

    m_state->setText(busy || !m_setup->status().isEmpty()
                         ? m_setup->status()
                         : QString("The first set-up downloads about 140 MB and builds the network on this PC "
                                   "(5 to 6 minutes)."));

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
    const bool ready = m_setup->ready();

    m_confirm->setEnabled(!busy && !m_controller->targetExecutable().isEmpty());
    m_forget->setEnabled(!busy && scan.has_value());
    m_install->setText(scan && scan->status == Status::Installed ? "Reinstall" : "Install");
    m_install->setEnabled(!busy && ready && scan && scan->status != Status::Unsupported);
    m_remove->setEnabled(!busy && ready && scan
                         && (scan->status == Status::Installed || scan->status == Status::Replaced));
    m_run->setEnabled(!busy && ready && scan && scan->status != Status::Unsupported);
    if (!ready)
        m_run->setToolTip("Set up HelixSR first.");
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
        return "no AMD FSR 3.1 DLL found (game moved or updated?)";
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

void HelixPanel::rebuildGames()
{
    const QString keep = selectedPath();
    m_games->clear();
    m_scans.clear();

    auto games = AppConfig::instance().games();
    games.erase(std::remove_if(games.begin(), games.end(), [](const auto& g) { return !g.helixConfirmed; }),
                games.end());
    std::sort(games.begin(), games.end(), [](const auto& a, const auto& b) {
        return a.savedName.toUpper() < b.savedName.toUpper();
    });

    for (const auto& g : games) {
        const auto scan = Helix::scanGame(g.applicationPath);
        m_scans.insert(g.applicationPath, scan);
        auto* item = new QListWidgetItem(QString("%1  -  %2").arg(g.savedName, statusText(scan)), m_games);
        item->setData(kPathRole, g.applicationPath);
        item->setToolTip(g.applicationPath);
        if (g.applicationPath == keep)
            m_games->setCurrentItem(item);
    }
    if (!m_games->currentItem() && m_games->count() > 0)
        m_games->setCurrentRow(0);
    updateButtons();
    updateSetupLabels();
}

void HelixPanel::confirmCurrentGame()
{
    const QString exe = m_controller->targetExecutable();
    if (exe.isEmpty())
        return;
    const QString path = GameRegistry::normalizePath(exe);
    const auto scan = Helix::scanGame(path);
    if (scan.targets.isEmpty()) {
        QMessageBox::information(
            this, "HelixSR - " + gameName(path),
            "No AMD FSR 3.1 DLL (amd_fidelityfx_dx12.dll or amd_fidelityfx_upscaler_dx12.dll) was found in "
            "this game's folder, so HelixSR cannot replace its upscaler.\n\nGames that only offer DLSS or XeSS "
            "need HelixSR Scaler, which this app does not install.\n\nSearched: " + scan.root);
        return;
    }

    auto entry = AppConfig::instance().gameByPath(path)
                     .value_or(GameRegistry::buildEntry(path, GameRegistry::defaultFriendlyName(path)));
    entry.helixConfirmed = true;
    AppConfig::instance().saveGame(entry);
    emit message(QString("%1 has an AMD FSR 3.1 DLL and is now on the HelixSR list.").arg(entry.savedName));
    rebuildGames();
    for (int i = 0; i < m_games->count(); ++i) {
        if (m_games->item(i)->data(kPathRole).toString() == path)
            m_games->setCurrentRow(i);
    }
}

void HelixPanel::forgetSelectedGame()
{
    const QString path = selectedPath();
    auto entry = AppConfig::instance().gameByPath(path);
    if (!entry)
        return;
    entry->helixConfirmed = false;
    AppConfig::instance().saveGame(*entry);
    rebuildGames();
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
    if (path.isEmpty() || m_setup->busy() || !m_setup->ready())
        return;

    // the game may have been updated since the list was built
    const auto scan = Helix::scanGame(path);
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
