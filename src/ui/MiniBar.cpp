#include "MiniBar.h"

#include "ui/Theme.h"

#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPushButton>
#include <QWindow>

MiniBar::MiniBar()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus)
{
    setObjectName("miniBar");
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);   // the game keeps the keyboard
    setWindowTitle("WindowManager");
    setToolTip("Drag to move. Double-click to open WindowManager.");

    auto *icon = new QLabel(this);
    icon->setPixmap(QIcon(":/window-manager-icon.png").pixmap(16, 16));
    m_game = new QLabel(this);
    m_game->setObjectName("miniGame");
    auto *open = Theme::button("Open", this, nullptr, "Open WindowManager");
    open->setFocusPolicy(Qt::NoFocus);

    auto *h = new QHBoxLayout(this);
    h->setContentsMargins(8, 4, 4, 4);
    h->setSpacing(10);
    h->addWidget(icon);
    h->addWidget(m_game);
    h->addWidget(open);

    connect(open, &QPushButton::clicked, this, &MiniBar::restoreRequested);
    setGame({});
}

void MiniBar::setGame(const QString &name)
{
    m_game->setText(name.isEmpty() ? QString("No game selected") : name);
    adjustSize();
}

void MiniBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && windowHandle())
        windowHandle()->startSystemMove();   // drag from anywhere on the bar
    QWidget::mousePressEvent(event);
}

void MiniBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit restoreRequested();
}

void MiniBar::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.addAction("&Open WindowManager", this, &MiniBar::restoreRequested);
    menu.addAction("&Focus the game", this, &MiniBar::focusGameRequested);
    menu.addSeparator();
    menu.addAction("E&xit", this, &MiniBar::exitRequested);
    menu.exec(event->globalPos());
}
