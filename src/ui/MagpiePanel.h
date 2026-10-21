#pragma once

#include <QElapsedTimer>

#include "ui/ControlPanels.h"

class MagpieSetup;
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTimer;

/**
 * Magpie tab: sets up and updates the app's own copy of Magpie, a window upscaler that scales a
 * game's window to the full monitor, and the selected game's choice (an effect, or off), which
 * MagpieSession applies when the game is started. Windows only.
 */
class MagpiePanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit MagpiePanel(WindowManagerController* controller, QWidget* parent = nullptr);

    bool updateAvailable() const;

public slots:
    void refresh() override;

signals:
    void message(const QString& text);
    void updateAvailableChanged(bool available);

private:
    void setUpOrUpdate();
    void updateLabels();
    void updateButtons();
    void loadGame();     // the selected game's choice into the controls
    void saveGame();     // the controls into the selected game

    MagpieSetup* m_setup;

    QLabel*       m_version;
    QLabel*       m_state;
    QProgressBar* m_progress;
    QPushButton*  m_setupButton;
    QPushButton*  m_check;
    QPushButton*  m_openFolder;
    QPushButton*  m_cancel;
    QLabel*       m_hint;

    QLabel*       m_gameLabel;   // the selected game
    QComboBox*    m_mode;        // Off, or one of Magpie's scaling modes
    QCheckBox*    m_game3D;
    QLabel*       m_gameHint;
    QString       m_gameExe;
    bool          m_loading{false};

    QTimer*       m_clock;
    QElapsedTimer m_elapsedTimer;
    bool          m_lastUpdateFlag{false};
};
