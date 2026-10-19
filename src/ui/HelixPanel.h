#pragma once

#include <functional>

#include <QElapsedTimer>
#include <QHash>
#include <QStringList>

#include "helix/HelixGame.h"
#include "helix/HelixIni.h"
#include "helix/HelixSystem.h"
#include "ui/ControlPanels.h"

class HelixSetup;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;

/**
 * HelixSR tab: set up / update HelixSR (with a running timer, the build takes minutes) and the
 * list of games, from which it is installed, removed and run. A game without an FSR 3.1 DLL is
 * greyed out: HelixSR cannot be used in it.
 */
class HelixPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit HelixPanel(WindowManagerController* controller, QWidget* parent = nullptr);

    bool updateAvailable() const;

public slots:
    void refresh() override;
    void gamesChanged();   // the saved games changed: rebuild the list

signals:
    void runRequested(const QString& exePath);   // HelixSR is in the game: start it
    void message(const QString& text);
    void updateAvailableChanged(bool available);

private:
    void rebuildGames(std::function<void()> then = {});   // scans run in the background; then() after the last
    void loadIni();
    void saveIni();
    QStringList iniPaths(const Helix::GameScan& scan) const;
    void updateButtons();
    void updateSetupLabels();
    void runSystemCheck();
    void showSystemCheck();

    void setUpOrUpdate();
    void installSelected();
    void removeSelected();
    void runSelected();

    QString selectedPath() const;
    std::optional<Helix::GameScan> scanOf(const QString& exePath) const;
    QString statusText(const Helix::GameScan& scan) const;
    bool needsUpdate(const Helix::GameScan& scan) const;
    void updateOutdatedGames();
    void nextQueued();

    HelixSetup* m_setup;

    QLabel*       m_version;
    QLabel*       m_system;
    QPushButton*  m_checkPc;
    QLabel*       m_state;
    QLabel*       m_driverWarning;
    QPushButton*  m_openFolder;
    QProgressBar* m_progress;
    QLabel*       m_elapsed;
    QPushButton*  m_setupButton;
    QPushButton*  m_check;
    QPushButton*  m_cancel;

    QListWidget*  m_games;
    QPushButton*  m_install;
    QPushButton*  m_remove;
    QPushButton*  m_run;
    QLabel*       m_gameHint;

    QComboBox*      m_sharpener;     // helixsr.ini of the selected game
    QDoubleSpinBox* m_sharpness;
    QComboBox*      m_waveSize;
    QPushButton*    m_applyIni;

    Helix::Verdict m_verdict;                  // this PC against HelixSR's requirements
    QTimer*        m_clock;
    QElapsedTimer  m_elapsedTimer;
    QHash<QString, Helix::GameScan> m_scans;   // by game path; rescanned when the list is rebuilt
    QString        m_runAfterInstall;
    QStringList    m_queue;                    // games waiting for an update after a new HelixSR build
    bool           m_lastUpdateFlag{false};
    bool           m_checking{false};          // a game is being looked at (Check / Run) in the background
    int            m_scanGeneration{0};        // results of an older list are dropped
    int            m_pendingScans{0};
    std::function<void()> m_afterScans;
};
