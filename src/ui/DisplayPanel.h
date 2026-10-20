#pragma once

#include "ControlPanels.h"
#include "display/DisplayModel.h"

#include <vector>

class DisplaySession;
class QSlider;
class QTimer;

/**
 * Display tab: the PC's display settings (resolution, refresh rate, orientation, scale, HDR,
 * brightness, colour profile, multiple displays, night light) and the selected game's own
 * settings, which DisplaySession applies while the game runs.
 *
 * The monitors are read from the system when the tab is shown, after each change and on
 * "Re-read"; refresh() only follows the selected game (no OS calls).
 */
class DisplayPanel : public ControlPanel
{
    Q_OBJECT

public:
    DisplayPanel(WindowManagerController* controller, DisplaySession* session, QWidget* parent = nullptr);
    void refresh() override;

signals:
    void message(const QString& text);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void reload();
    void showMonitor();
    void fillResolutions();
    void fillRefreshRates();
    const DisplayModel::Monitor* selected() const;

    void applyMode();
    void toggleHdr();
    void applyScale();
    void applyBrightness();
    void applyColorProfile();
    void applyTopology();
    bool askToKeep();
    void report(const QString& text, bool failed = false);

    void loadGame();
    void fillGameChoices();
    void fillGameRefreshRates();
    void saveGame();
    void updateGameStatus();

    DisplaySession*               m_session;
    std::vector<DisplayModel::Monitor> m_monitors;
    QString                       m_gameExe;
    bool                          m_loading{false};

    QWidget*     m_content;
    QLabel*      m_unsupported;
    QComboBox*   m_monitor;
    QComboBox*   m_resolution;
    QComboBox*   m_refresh;
    QComboBox*   m_orientation;
    QComboBox*   m_scale;
    QComboBox*   m_profile;
    QComboBox*   m_topology;
    QPushButton* m_applyMode;
    QPushButton* m_hdr;
    QPushButton* m_reload;
    QPushButton* m_nightLight;
    QSlider*     m_brightness;
    QLabel*      m_brightnessValue;
    QTimer*      m_brightnessTimer;

    QCheckBox*   m_gameEnabled;
    QComboBox*   m_gameMonitor;
    QComboBox*   m_gameResolution;
    QComboBox*   m_gameRefresh;
    QComboBox*   m_gameHdr;
    QComboBox*   m_gameScale;
    QComboBox*   m_gameBrightness;
    QLabel*      m_gameStatus;
    QPushButton* m_gameRestore;
};
