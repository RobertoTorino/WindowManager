#include "../../ui/ScaledView.h"

#include <QTimer>

// The downscaled view mirrors the game through the Windows compositor (DWM). Until there is an
// equivalent for the platform, the view reports that it cannot start.

struct ScaledView::State {};

ScaledView::ScaledView(QObject *parent)
    : QObject(parent)
    , m_state(new State)
    , m_timer(new QTimer(this))
{
}

ScaledView::~ScaledView()
{
    delete m_state;
}

bool ScaledView::start(const Options &, QString *error)
{
    if (error)
        *error = "The downscaled view is not available on this system yet.";
    return false;
}

void ScaledView::stop(const QString &)
{
}

void ScaledView::restoreGameWindow()
{
}

void ScaledView::updateThumbnail()
{
}

void ScaledView::tick()
{
}

bool ScaledView::handleMouse(unsigned int, quintptr, qintptr)
{
    return false;
}
