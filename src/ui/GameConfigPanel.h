#pragma once

#include "ControlPanels.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

/**
 * Game config tab: settings written into the selected game's own config files before it starts
 * (GameConfig): the render scale (supersampling or upscaling inside the engine), the resolution, and for
 * Tekken 7's arcade builds JConfig's window. Unreal Engine 4 games with loose config files only, for now.
 */
class GameConfigPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit GameConfigPanel(WindowManagerController* controller, QWidget* parent = nullptr);
    void refresh() override;

signals:
    void message(const QString& text);

protected:
    void showEvent(QShowEvent* event) override;

private:
    QString configExe() const;   // the exe whose folders hold the config (.bat games: the one it started)
    void load();
    void save();
    void fillWindowChoices(int x, int y, int width, int height);
    void updateStatus();
    void writeNow();
    void restoreOriginals();

    QString m_gameExe;
    bool    m_loading{false};

    QLabel*      m_status;
    QLabel*      m_files;
    QCheckBox*   m_enabled;
    QComboBox*   m_scale;
    QComboBox*   m_resolution;
    QCheckBox*   m_jconfigWindow;
    QComboBox*   m_window;
    QPushButton* m_write;
    QPushButton* m_restore;
    QPushButton* m_openFolder;
};
