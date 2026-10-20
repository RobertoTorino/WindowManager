#pragma once

#include "DisplayModel.h"

#include <QObject>
#include <QString>

class QTimer;

/**
 * A game's own display settings (Display tab, "While the game runs").
 *
 *   gameStarting(exe)  before the launch: applies the game's settings, remembers the desktop's
 *   tick (2 s)         game seen running, then gone for ~6 s -> restore()
 *                      never seen within 3 minutes           -> restore()
 *   restore()          also on a failed launch, "Restore now", and when the app closes
 *
 * The undo record is written to windowmanager.ini while a game runs; recoverFromCrash() at
 * start-up restores it if the app did not get to.
 */
class DisplaySession : public QObject
{
    Q_OBJECT

public:
    explicit DisplaySession(QObject* parent = nullptr);

    void gameStarting(const QString& exePath);
    void restore(const QString& reason);
    void recoverFromCrash();

    bool    isActive() const { return m_active; }
    QString description() const;   // for the Display tab

signals:
    void message(const QString& text);
    void changed();

private:
    void tick();
    bool apply(const DisplayModel::Monitor& monitor, const DisplayModel::Changes& changes, QStringList& failures);
    void persist();

    QTimer*          m_timer;
    QString          m_exePath;
    QString          m_gameName;
    DisplayModel::Restore m_restore;
    bool             m_active{false};
    bool             m_seenRunning{false};
    int              m_waitTicks{0};
    int              m_goneTicks{0};
};
