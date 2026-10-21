#pragma once

#include "ControlPanels.h"
#include "stats/SessionReport.h"

#include <vector>

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class SessionChart;
class SetupChart;
class ImageView;

/**
 * Reports tab: the recorded sessions of one game (SessionRecorder's files under reports/), chosen in the
 * dropdown. Left, the sessions; right, three views: *Session* (the picked session over time: FPS and GPU,
 * coloured by setup), *Setups* (the game's setups compared over all its sessions) and *Pictures* (the picture
 * comparisons, PictureCompare). "Larger view" opens the same in a window of its own.
 */
class ReportsPanel : public ControlPanel
{
    Q_OBJECT

public:
    explicit ReportsPanel(WindowManagerController *controller, QWidget *parent = nullptr, bool compact = true);

    void showGame(const QString &game);   // select it (after a session or a comparison was saved)

public slots:
    void refresh() override;

signals:
    void compareRequested();
    void message(const QString &text);

private:
    void loadGames();
    void loadGame();
    void showSession(int row);
    void showView(int index);
    void showComparison(int row);
    void openLarger();

    bool m_compact;
    QComboBox    *m_game{nullptr};
    QPushButton  *m_compare{nullptr};
    QTableWidget *m_sessionsTable{nullptr};
    QPushButton  *m_viewButtons[3]{};
    QStackedWidget *m_views{nullptr};
    SessionChart *m_sessionChart{nullptr};
    SetupChart   *m_setupChart{nullptr};
    QListWidget  *m_pictureList{nullptr};
    ImageView    *m_picture{nullptr};
    QLabel       *m_pictureText{nullptr};   // the verdict above the picture
    QLabel       *m_hint{nullptr};

    std::vector<Report::Session> m_sessions;        // the chosen game's, newest first
    std::vector<Report::Comparison> m_comparisons;
    QStringList m_setupOrder;
    QString m_dir;                                   // the chosen game's folder
    bool    m_loading{false};                        // a session picked by loadGame, not by the user
};
