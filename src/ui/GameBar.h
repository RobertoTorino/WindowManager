#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QComboBox;
class QPushButton;

/** Top row: pick/register a game and Start / Restart / Kill it. */
class GameBar : public QWidget
{
    Q_OBJECT

public:
    explicit GameBar(QWidget* parent = nullptr);

    // (display name, executable path) pairs; keeps the current selection if possible.
    void setGames(const QList<QPair<QString, QString>>& games, const QString& currentPath);
    QString selectedPath() const;

signals:
    void selectRequested();
    void startRequested();
    void restartRequested();
    void killRequested();
    void gameChosen(const QString& path);

private:
    QComboBox*   m_combo;
    QPushButton* m_start;
    QPushButton* m_restart;
    QPushButton* m_kill;
    QList<QPair<QString, QString>> m_games;
};
