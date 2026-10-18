#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QStringList>

#include "helix/HelixGame.h"
#include "helix/HelixSystem.h"
#include "ui/ControlPanels.h"

class HelixSetup;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QTimer;

/**
 * HelixSR tab: set up / update HelixSR (with a running timer, the build takes minutes) and the
 * list of games confirmed to have an FSR 3.1 DLL, from which they are installed, removed and run.
 */
class HelixPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit HelixPanel(WindowManagerController* controller, QWidget* parent = nullptr);

    bool updateAvailable() const;

public slots:
    void refresh() override;

signals:
    void runRequested(const QString& exePath);   // HelixSR is in the game: start it
    void message(const QString& text);
    void updateAvailableChanged(bool available);

private:
    void rebuildGames();
    void updateButtons();
    void updateSetupLabels();
    void runSystemCheck();
    void showSystemCheck();

    void setUpOrUpdate();
    void confirmCurrentGame();
    void forgetSelectedGame();
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
    QProgressBar* m_progress;
    QLabel*       m_elapsed;
    QPushButton*  m_setupButton;
    QPushButton*  m_check;
    QPushButton*  m_cancel;

    QListWidget*  m_games;
    QPushButton*  m_confirm;
    QPushButton*  m_install;
    QPushButton*  m_remove;
    QPushButton*  m_run;
    QPushButton*  m_forget;
    QLabel*       m_gameHint;

    Helix::Verdict m_verdict;                  // this PC against HelixSR's requirements
    QTimer*        m_clock;
    QElapsedTimer  m_elapsedTimer;
    QHash<QString, Helix::GameScan> m_scans;   // by game path; rescanned when the list is rebuilt
    QString        m_runAfterInstall;
    QStringList    m_queue;                    // games waiting for an update after a new HelixSR build
    bool           m_lastUpdateFlag{false};
};
