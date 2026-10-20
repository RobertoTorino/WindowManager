#pragma once

#include "ControlPanels.h"
#include "stats/DriverCheck.h"
#include "stats/StatsModel.h"

#include <QList>

class QComboBox;
class QGridLayout;
class QLabel;
class QPushButton;

/**
 * GPU & Power tab:
 *  - the PC's graphics cards with their driver version and date, and whether a newer driver is out
 *    (NVIDIA: asked from NVIDIA's lookup; other vendors: their driver page);
 *  - Windows' graphics setting for the selected game's exe (Let Windows decide / Power saving /
 *    High performance), the same setting as Settings > System > Display > Graphics, and the card
 *    the running game renders on now (from StatsMonitor). A change applies when the game starts
 *    next, so a running game offers Restart;
 *  - the power plan (all plans, also the ones Control Panel hides) and the Windows power mode.
 */
class GpuPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit GpuPanel(WindowManagerController* controller, QWidget* parent = nullptr);
    void refresh() override;

    // From StatsMonitor::statsUpdated.
    void showStats(const Stats::GameStats& stats);

signals:
    void message(const QString& text);
    void restartRequested();

protected:
    void showEvent(QShowEvent* event) override;

private:
    struct DriverRow
    {
        Stats::Adapter adapter;
        QLabel*      status{nullptr};
        QPushButton* page{nullptr};
        QString      url;
    };

    void buildAdapters(QGridLayout* grid);
    void checkDrivers();
    void driverChecked(const DriverCheck::Result& result);

    QString gameExe() const;   // the real exe (a script's game exe), native separators; "" if unknown
    void loadPreference();
    void choosePreference();
    void updateStatus();

    void loadPower();
    void choosePlan();
    void chooseMode();

    DriverCheck* m_driverCheck;
    QList<DriverRow> m_drivers;
    bool m_driversChecked{false};

    QString m_target;
    QString m_exe;
    bool    m_changedWhileRunning{false};
    Stats::GameStats m_stats;

    QLabel*      m_unsupported;
    QWidget*     m_content;
    QPushButton* m_checkDrivers;
    QLabel*      m_gameTitle;
    QComboBox*   m_preference;
    QLabel*      m_status;
    QPushButton* m_restart;
    QComboBox*   m_plan;
    QComboBox*   m_mode;
    QLabel*      m_source;
};
