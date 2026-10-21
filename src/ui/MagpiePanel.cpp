#include "MagpiePanel.h"

#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "magpie/MagpieConfig.h"
#include "magpie/MagpieSetup.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const QString kOff = QStringLiteral("Off");

// Magpie's scaling modes, from the app's copy (its defaults until it has written its config).
QStringList modeNames()
{
    QFile f(Magpie::configPath(Magpie::installDir()));
    const QStringList modes = f.open(QIODevice::ReadOnly) ? Magpie::scalingModes(f.readAll()) : QStringList{};
    return modes.isEmpty() ? QStringList{"Lanczos", "FSR", "FSRCNNX", "CuNNy", "Anime4K", "CRT-Geom", "Integer Scale 2x"}
                           : modes;
}

} // namespace

MagpiePanel::MagpiePanel(WindowManagerController* c, QWidget* parent)
    : ControlPanel(c, parent)
    , m_setup(new MagpieSetup(this))
    , m_clock(new QTimer(this))
{
    m_version = new QLabel(this);
    m_state = new QLabel(this);
    m_state->setObjectName("hint");
    m_state->setWordWrap(true);
    m_progress = new QProgressBar(this);
    m_progress->setTextVisible(true);
    m_setupButton = Theme::button("Set up Magpie", this, "accent",
                                  "Download Magpie from GitHub, check it and keep this app's own copy of it");
    m_check = Theme::button("Check for updates", this, nullptr, "Look for a newer Magpie release on GitHub");
    m_openFolder = Theme::button("Open folder", this, nullptr, "Show this app's copy of Magpie");
    m_cancel = Theme::button("Cancel", this, "danger", "Stop the download and remove what it left");
    m_hint = new QLabel(this);
    m_hint->setObjectName("hint");
    m_hint->setWordWrap(true);
    m_hint->setText(MagpieSetup::supported()
        ? QString("Magpie scales a game's window up to the full monitor (FSR, Lanczos, CRT and more), for any game: "
                  "DirectX 11 or 12, Vulkan, emulators. Nothing is put into the game. The game runs windowed at a "
                  "lower size and Magpie fills the monitor with it. This app keeps its own copy, apart from a "
                  "Magpie you installed yourself.")
        : QString("Magpie runs on Windows only."));

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_setupButton);
    buttons->addWidget(m_check);
    buttons->addWidget(m_openFolder);
    buttons->addWidget(m_cancel);
    buttons->addStretch(1);

    auto* top = new QVBoxLayout;
    top->addWidget(m_version);
    top->addLayout(buttons);
    top->addWidget(m_progress);
    top->addWidget(m_state);
    top->addWidget(m_hint);

    auto* topBox = new QGroupBox("Magpie (window upscaler)", this);
    topBox->setLayout(top);

    // --- the selected game ---
    m_gameLabel = new QLabel(this);
    m_mode = new QComboBox(this);
    m_mode->setToolTip("The effect Magpie scales this game with when it is in front; Off: not scaled");
    m_game3D = new QCheckBox("3D game mode", this);
    m_game3D->setToolTip("For games that turn the camera with the mouse. Magpie then keeps the mouse inside the "
                         "game all the time, also when another window is in front, until Magpie closes "
                         "(it closes when the game ends).");
    m_gameHint = new QLabel(this);
    m_gameHint->setObjectName("hint");
    m_gameHint->setWordWrap(true);
    m_gameHint->setText("On Start, the app gives Magpie this game's window and starts Magpie; it closes again "
                        "when the game ends. Run the game windowed at the size it should render (not "
                        "maximized, not borderless full screen): Magpie fills the monitor with it.");

    auto* gameRow = new QHBoxLayout;
    gameRow->addWidget(new QLabel("Scale with", this));
    gameRow->addWidget(m_mode);
    gameRow->addWidget(m_game3D);
    gameRow->addStretch(1);

    auto* game = new QVBoxLayout;
    game->addWidget(m_gameLabel);
    game->addLayout(gameRow);
    game->addWidget(m_gameHint);
    auto* gameBox = new QGroupBox("Selected game", this);
    gameBox->setLayout(game);

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(10, 10, 10, 10);
    grid->setSpacing(10);
    grid->addWidget(topBox, 0, 0);
    grid->addWidget(gameBox, 1, 0);
    grid->setRowStretch(2, 1);
    grid->setColumnStretch(0, 1);

    connect(m_setupButton, &QPushButton::clicked, this, &MagpiePanel::setUpOrUpdate);
    connect(m_check, &QPushButton::clicked, m_setup, &MagpieSetup::checkForUpdate);
    connect(m_cancel, &QPushButton::clicked, m_setup, &MagpieSetup::cancel);
    connect(m_openFolder, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_setup->dir()));
    });
    connect(m_mode, &QComboBox::currentTextChanged, this, &MagpiePanel::saveGame);
    connect(m_game3D, &QCheckBox::toggled, this, &MagpiePanel::saveGame);
    connect(m_clock, &QTimer::timeout, this, &MagpiePanel::updateLabels);
    connect(m_setup, &MagpieSetup::stateChanged, this, [this]() {
        if (m_setup->busy() && !m_clock->isActive()) {
            m_elapsedTimer.start();
            m_clock->start(1000);
        } else if (!m_setup->busy()) {
            m_clock->stop();
        }
        updateLabels();
        updateButtons();
    });
    connect(m_setup, &MagpieSetup::updateChecked, this, [this](bool ok) {
        if (!ok)
            emit message("Magpie update check failed: " + m_setup->lastError());
        else if (m_setup->updateAvailable())
            emit message(QString("Magpie %1 is available (see the Magpie tab).").arg(m_setup->latest()->version.toString()));
        updateButtons();
    });
    connect(m_setup, &MagpieSetup::installFinished, this, [this](bool ok, const QString& text) {
        emit message(text);
        loadGame();   // Magpie's own list of scaling modes is there now
        if (!ok && text != "Cancelled.")
            QMessageBox::warning(this, "Magpie", text);
    });

    updateLabels();
    loadGame();

    // Only a PC that has Magpie checks by itself; the check is one small request to GitHub.
    QTimer::singleShot(4000, this, [this]() {
        if (m_setup->installedVersion() && !m_setup->busy())
            m_setup->checkForUpdate();
    });
}

bool MagpiePanel::updateAvailable() const
{
    return m_setup->updateAvailable();
}

void MagpiePanel::refresh()
{
    if (m_controller->targetExecutable() != m_gameExe)
        loadGame();
    else
        updateButtons();
}

void MagpiePanel::loadGame()
{
    m_gameExe = m_controller->targetExecutable();
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    const Magpie::GameSettings s = game ? game->magpie : Magpie::GameSettings{};

    m_loading = true;
    m_mode->clear();
    m_mode->addItem(kOff);
    m_mode->addItems(modeNames());
    if (s.enabled() && m_mode->findText(s.mode) < 0)
        m_mode->addItem(s.mode);   // a mode renamed or removed in Magpie: still shown (and reported on Start)
    m_mode->setCurrentText(s.enabled() ? s.mode : kOff);
    m_game3D->setChecked(s.game3D);
    m_gameLabel->setText(!game ? QString("No game selected.")
                               : game->savedName.isEmpty() ? QFileInfo(m_gameExe).completeBaseName()
                                                           : game->savedName);
    m_loading = false;
    updateButtons();
}

void MagpiePanel::saveGame()
{
    updateButtons();
    if (m_loading || m_gameExe.isEmpty())
        return;
    auto game = AppConfig::instance().gameByPath(m_gameExe);
    if (!game)
        return;
    Magpie::GameSettings s;
    s.mode = m_mode->currentText() == kOff ? QString() : m_mode->currentText();
    s.game3D = s.enabled() && m_game3D->isChecked();
    if (s == game->magpie)
        return;
    game->magpie = s;
    AppConfig::instance().saveGame(*game);
    AppConfig::instance().save();
    const QString name = game->savedName.isEmpty() ? QFileInfo(m_gameExe).completeBaseName() : game->savedName;
    emit message(s.enabled() ? QString("%1: Magpie scales it with %2 from the next Start%3.")
                                   .arg(name, s.mode, s.game3D ? " (3D game mode)" : "")
                             : QString("%1: Magpie is not used.").arg(name));
}

void MagpiePanel::setUpOrUpdate()
{
    if (m_setup->busy())
        return;
    const auto latest = m_setup->latest();
    const QString version = latest ? latest->version.toString() : QString("the latest version");
    const QString size = latest && latest->zipSize > 0 ? QString("about %1 MB").arg(latest->zipSize / 1048576)
                                                      : QString("about 10 MB");
    QMessageBox box(QMessageBox::Question, "Set up Magpie", QString("Download Magpie %1?").arg(version),
                    QMessageBox::NoButton, this);
    box.setInformativeText(
        QString("- Magpie (%1) is downloaded from github.com/Blinue/Magpie (GPL-3.0) and checked against its "
                "published SHA-256.\n"
                "- It is kept in the magpie folder next to this program, with its own settings; a Magpie you "
                "installed yourself is not changed.\n"
                "- Magpie is started once in the tray so it writes its default settings, then closed.\n"
                "- Only one Magpie can run at a time: close your own Magpie first if it is running.").arg(size));
    auto* go = box.addButton("Download", QMessageBox::AcceptRole);
    auto* cancel = box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(cancel);
    box.exec();
    if (box.clickedButton() == go)
        m_setup->installOrUpdate();
}

void MagpiePanel::updateLabels()
{
    const auto installed = m_setup->installedVersion();
    const auto latest = m_setup->latest();
    QString version = installed ? QString("Installed: Magpie %1").arg(installed->toString())
                                : QString("Magpie is not set up yet.");
    if (latest)
        version += QString("    Latest: %1").arg(latest->version.toString());
    m_version->setText(version);
    m_state->setText(m_setup->status());
    m_state->setVisible(!m_setup->status().isEmpty());

    const bool busy = m_setup->busy();
    m_progress->setVisible(busy);
    if (busy) {
        if (m_setup->percent() >= 0) {
            m_progress->setRange(0, 100);
            m_progress->setValue(m_setup->percent());
        } else {
            m_progress->setRange(0, 0);
        }
    }

    if (!installed)
        m_setupButton->setText("Set up Magpie");
    else if (m_setup->updateAvailable())
        m_setupButton->setText(QString("Update to %1").arg(latest->version.toString()));
    else
        m_setupButton->setText("Up to date");
}

void MagpiePanel::updateButtons()
{
    const bool supported = MagpieSetup::supported();
    const bool busy = m_setup->busy();
    const bool installed = m_setup->installedVersion().has_value();
    m_setupButton->setEnabled(supported && !busy && (!installed || m_setup->updateAvailable()));
    m_check->setEnabled(supported && !busy);
    m_openFolder->setEnabled(installed);
    m_cancel->setVisible(busy && m_setup->phase() != MagpieSetup::Phase::Installing
                         && m_setup->phase() != MagpieSetup::Phase::FirstStart);
    const bool hasGame = !m_gameExe.isEmpty() && AppConfig::instance().gameByPath(m_gameExe).has_value();
    m_mode->setEnabled(supported && installed && hasGame);
    m_game3D->setEnabled(supported && installed && hasGame && m_mode->currentText() != kOff);
    updateLabels();

    const bool flag = m_setup->updateAvailable();
    if (flag != m_lastUpdateFlag) {
        m_lastUpdateFlag = flag;
        emit updateAvailableChanged(flag);
    }
}
