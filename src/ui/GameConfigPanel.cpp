#include "GameConfigPanel.h"

#include "Theme.h"
#include "WindowManagerController.h"
#include "config/AppConfig.h"
#include "gameconfig/GameConfigFiles.h"
#include "window/MonitorHelper.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QRect>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QUrl>
#include <QVBoxLayout>

namespace {

constexpr int kScales[] = {50, 67, 75, 85, 100, 125, 150, 175, 200, 250, 300};
constexpr QSize kResolutions[] = {{1280, 720}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160}};

QString sizeText(int width, int height)
{
    return QString("%1 x %2").arg(width).arg(height);
}

// Selects the item whose data equals value; returns false if there is none.
bool selectData(QComboBox* combo, const QVariant& value)
{
    const int index = combo->findData(value);
    if (index < 0)
        return false;
    combo->setCurrentIndex(index);
    return true;
}

} // namespace

GameConfigPanel::GameConfigPanel(WindowManagerController* controller, QWidget* parent)
    : ControlPanel(controller, parent)
{
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_files = new QLabel(this);
    m_files->setObjectName("hint");
    m_files->setWordWrap(true);
    m_files->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_enabled = new QCheckBox(this);
    m_enabled->setToolTip("Written when the game is started from this app (Start / Restart).\n"
                          "The files keep the settings afterwards; Restore originals puts the game's own back.");

    m_scale = new QComboBox(this);
    m_scale->addItem("Game's own", 0);
    for (int scale : kScales)
        m_scale->addItem(scale > 100   ? QString("%1 % (supersample)").arg(scale)
                         : scale < 100 ? QString("%1 % (upscale, faster)").arg(scale)
                                       : QString("%1 %").arg(scale),
                         scale);
    m_scale->setToolTip("The resolution the game draws its 3D scene at, in percent of the window (r.ScreenPercentage).\n"
                        "Above 100: drawn larger and scaled down by the engine: sharper edges, needs more GPU.\n"
                        "Below 100: drawn smaller and scaled up: faster, softer.\n"
                        "The window and the menus keep the resolution below.");

    m_resolution = new QComboBox(this);
    m_resolution->addItem("Game's own", QSize());
    for (QSize size : kResolutions)
        m_resolution->addItem(sizeText(size.width(), size.height()), size);
    m_resolution->setToolTip("The game's resolution: the engine's settings, r.setres and JConfig's DX resolution.\n"
                             "In a window the game's window takes this size.");

    m_jconfigWindow = new QCheckBox("JConfig window", this);
    m_jconfigWindow->setToolTip("Also set JConfig's DX Window Mode window (position and size).\n"
                                "Tekken 7 puts its window back to this rectangle when it gets the focus.");
    m_window = new QComboBox(this);
    m_window->setToolTip("Where JConfig places the game's window");

    m_write = Theme::button("Write now", this, "accent", "Write these settings into the game's files now");
    m_restore = Theme::button("Restore originals", this, nullptr,
                              "Put the game's own values back (from the copies kept next to its files)");
    m_openFolder = Theme::button("Open folder", this, nullptr, "Open the game's Config folder");

    auto* grid = new QGridLayout;
    grid->addWidget(m_enabled, 0, 0, 1, 4);
    grid->addWidget(new QLabel("Render scale", this), 1, 0);
    grid->addWidget(m_scale, 1, 1);
    grid->addWidget(new QLabel("Resolution", this), 1, 2);
    grid->addWidget(m_resolution, 1, 3);
    grid->addWidget(m_jconfigWindow, 2, 0);
    grid->addWidget(m_window, 2, 1, 1, 3);
    grid->addWidget(m_files, 3, 0, 1, 4);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(3, 1);

    auto* buttons = new QVBoxLayout;
    buttons->addWidget(m_write);
    buttons->addWidget(m_restore);
    buttons->addWidget(m_openFolder);
    buttons->addStretch(1);
    grid->addLayout(buttons, 0, 4, 4, 1);

    auto* box = new QGroupBox("The game's config files (Unreal Engine 4)", this);
    box->setLayout(grid);

    auto* page = new QVBoxLayout(this);
    page->setContentsMargins(10, 10, 10, 10);
    page->addWidget(m_status);
    page->addWidget(box);

    connect(m_enabled, &QCheckBox::toggled, this, &GameConfigPanel::save);
    connect(m_jconfigWindow, &QCheckBox::toggled, this, &GameConfigPanel::save);
    for (QComboBox* combo : {m_scale, m_resolution, m_window})
        connect(combo, qOverload<int>(&QComboBox::activated), this, &GameConfigPanel::save);
    connect(m_write, &QPushButton::clicked, this, &GameConfigPanel::writeNow);
    connect(m_restore, &QPushButton::clicked, this, &GameConfigPanel::restoreOriginals);
    connect(m_openFolder, &QPushButton::clicked, this, [this]() {
        const GameConfig::Layout l = GameConfig::layoutFor(configExe());
        if (l.valid())
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(l.engineIni).absolutePath()));
    });
}

QString GameConfigPanel::configExe() const
{
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    return GameConfig::exeForConfig(m_gameExe, game ? game->childExePath : QString());
}

void GameConfigPanel::refresh()
{
    const QString exe = m_controller->targetExecutable();
    if (exe != m_gameExe) {
        m_gameExe = exe;
        load();
    }
}

void GameConfigPanel::showEvent(QShowEvent* event)
{
    ControlPanel::showEvent(event);
    if (!event->spontaneous())   // the files may have changed outside the app
        load();
}

void GameConfigPanel::load()
{
    const auto game = m_gameExe.isEmpty() ? std::nullopt : AppConfig::instance().gameByPath(m_gameExe);
    const GameConfig::Settings s = game ? game->gameConfig : GameConfig::Settings{};
    const QString name = !game ? QString()
                       : game->savedName.isEmpty() ? QFileInfo(m_gameExe).completeBaseName()
                                                   : game->savedName;
    m_loading = true;
    m_enabled->setText(game ? QString("Write these settings when %1 starts").arg(name)
                            : QString("Select a game to change its config files"));
    m_enabled->setChecked(s.enabled);
    selectData(m_scale, s.renderScale);

    {
        const QSignalBlocker block(m_resolution);
        while (m_resolution->count() > 1 + int(std::size(kResolutions)))   // a saved size added last time
            m_resolution->removeItem(m_resolution->count() - 1);
        const QSize size(s.width, s.height);
        if (!size.isEmpty() && !selectData(m_resolution, size)) {
            m_resolution->addItem(sizeText(s.width, s.height), size);
            m_resolution->setCurrentIndex(m_resolution->count() - 1);
        } else if (size.isEmpty()) {
            m_resolution->setCurrentIndex(0);
        }
    }
    m_jconfigWindow->setChecked(s.jconfigWindow);
    fillWindowChoices(s.windowX, s.windowY, s.windowWidth, s.windowHeight);
    m_loading = false;
    updateStatus();
}

// The monitors as window rectangles, and the saved rectangle if it is none of them
void GameConfigPanel::fillWindowChoices(int x, int y, int width, int height)
{
    const QSignalBlocker block(m_window);
    m_window->clear();
    for (const MonitorGeometry& m : MonitorHelper::enumerateMonitors())
        m_window->addItem(QString("Monitor %1: %2 at %3,%4").arg(m.index).arg(sizeText(m.w, m.h)).arg(m.x).arg(m.y),
                          QRect(m.x, m.y, m.w, m.h));
    if (width > 0 && height > 0 && !selectData(m_window, QRect(x, y, width, height))) {
        m_window->addItem(QString("Saved: %1 at %2,%3").arg(sizeText(width, height)).arg(x).arg(y),
                          QRect(x, y, width, height));
        m_window->setCurrentIndex(m_window->count() - 1);
    }
}

void GameConfigPanel::save()
{
    if (m_loading || m_gameExe.isEmpty())
        return;
    auto game = AppConfig::instance().gameByPath(m_gameExe);
    if (!game)
        return;
    GameConfig::Settings s;
    s.enabled = m_enabled->isChecked();
    s.renderScale = m_scale->currentData().toInt();
    const QSize size = m_resolution->currentData().toSize();
    if (size.isValid()) {
        s.width = size.width();
        s.height = size.height();
    }
    const QRect window = m_window->currentData().toRect();
    s.jconfigWindow = m_jconfigWindow->isChecked() && window.isValid();
    if (window.isValid()) {
        s.windowX = window.x();
        s.windowY = window.y();
        s.windowWidth = window.width();
        s.windowHeight = window.height();
    }
    if (s == game->gameConfig)
        return;
    game->gameConfig = s;
    AppConfig::instance().saveGame(*game);
    AppConfig::instance().save();
    updateStatus();
}

void GameConfigPanel::updateStatus()
{
    const GameConfig::Status st = GameConfig::status(configExe());
    const bool usable = !m_gameExe.isEmpty() && st.supported()
                        && AppConfig::instance().gameByPath(m_gameExe).has_value();
    for (QWidget* w : std::initializer_list<QWidget*>{m_enabled, m_scale, m_resolution, m_write})
        w->setEnabled(usable);
    m_jconfigWindow->setEnabled(usable && st.jconfig);
    m_window->setEnabled(usable && st.jconfig && m_jconfigWindow->isChecked());
    m_restore->setEnabled(st.changed);
    m_openFolder->setEnabled(st.supported());

    if (m_gameExe.isEmpty()) {
        m_status->setText("Select a game first.");
    } else if (!st.layout.valid() || !st.supported()) {
        m_status->setText("This game has no loose Unreal Engine config files: the app looks for "
                          "<Project>/Config/DefaultEngine.ini, with the exe in <Project>/Binaries/Win64. "
                          "Most store games keep theirs packed or in the user folder.");
    } else {
        QStringList found{"DefaultEngine.ini"};
        if (st.userSettingsIni)
            found << "DefaultGameUserSettings.ini";
        if (st.platformIni)
            found << "WindowsEngine.ini";
        if (st.jconfig)
            found << "JConfig (monitor_misc.bin)";
        m_status->setText(QString("Found: %1. The game reads them when it starts: restart it after a change.")
                              .arg(found.join(", ")));
    }
    // the same number of lines either way: the tab's height is set when it is shown
    m_files->setText(st.summary.isEmpty() ? QString()
                     : st.changed ? "Now in the files (changed by this app, originals kept as *.wm-original):\n" + st.summary
                                  : "Now in the files (the game's own):\n" + st.summary);
}

void GameConfigPanel::writeNow()
{
    const auto game = AppConfig::instance().gameByPath(m_gameExe);
    if (!game)
        return;
    QString text;
    if (GameConfig::apply(configExe(), game->gameConfig, &text) && m_controller->isTargetRunning())
        text += " The game is running: restart it to see the change.";
    emit message(text);
    updateStatus();
}

void GameConfigPanel::restoreOriginals()
{
    if (QMessageBox::question(this, "Restore originals",
                              "Put the game's own values back for the keys this app changed?\n"
                              "Your other edits in those files stay.")
        != QMessageBox::Yes)
        return;
    QString text;
    GameConfig::restore(configExe(), &text);
    emit message(text);
    updateStatus();
}
