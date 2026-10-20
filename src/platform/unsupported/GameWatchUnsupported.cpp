#include "../../window/GameWatch.h"

// The game watch uses Windows' WinEvent hooks. On X11 the same could come from PropertyNotify on the
// game's windows and _NET_ACTIVE_WINDOW on the root; until then it does nothing.

struct GameWatch::Impl
{
};

GameWatch::GameWatch(QObject *parent)
    : QObject(parent)
{
}

GameWatch::~GameWatch() = default;

bool GameWatch::supported()
{
    return false;
}

void GameWatch::watch(qint64)
{
}
