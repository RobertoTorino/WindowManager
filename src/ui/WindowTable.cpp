#include "WindowTable.h"

#include <QHeaderView>
#include <QPaintEvent>
#include <QPainter>
#include <QScrollBar>

namespace {

enum Column { ColHwnd, ColTitle, ColClass, ColStatus, ColPid, ColX, ColY, ColW, ColH, ColCount };

QTableWidgetItem* makeItem(const QString& text, bool centered = false, bool dim = false)
{
    auto* it = new QTableWidgetItem(text);
    if (centered)
        it->setTextAlignment(Qt::AlignCenter);
    if (dim)
        it->setForeground(QColor(0x8b, 0x8f, 0x95));
    return it;
}

} // namespace

WindowTable::WindowTable(QWidget* parent)
    : QTableWidget(parent)
{
    setObjectName("windowTable");   // the active row's colour: Theme.cpp
    setColumnCount(ColCount);
    setHorizontalHeaderLabels({"HWND", "Title", "Class", "Status", "PID", "X", "Y", "W", "H"});
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAlternatingRowColors(true);
    setShowGrid(false);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(26);
    setMinimumHeight(150);

    auto* header = horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    header->setStretchLastSection(false);
    setColumnWidth(ColHwnd, 90);
    setColumnWidth(ColClass, 170);
    setColumnWidth(ColStatus, 70);
    setColumnWidth(ColPid, 70);
    for (int c : {ColX, ColY, ColW, ColH})
        setColumnWidth(c, 60);

    connect(selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
        if (m_populating)
            return;
        const int row = current.row();
        if (row >= 0 && row < static_cast<int>(m_hwnds.size()))
            emit windowChosen(m_hwnds[static_cast<std::size_t>(row)]);
    });
}

void WindowTable::setWindows(const std::vector<WindowManager::WindowInfo>& windows, quintptr activeHwnd)
{
    m_populating = true;
    const int scroll = verticalScrollBar()->value();

    clearSelection();
    setRowCount(static_cast<int>(windows.size()));
    m_hwnds.clear();
    m_hwnds.reserve(windows.size());

    int activeRow = -1;
    for (int i = 0; i < static_cast<int>(windows.size()); ++i) {
        const auto& w = windows[static_cast<std::size_t>(i)];
        m_hwnds.push_back(w.hwnd);
        const bool dim = !w.visible;

        const bool active = w.hwnd == activeHwnd;
        const QString status = active ? QString("Active") : w.visible ? QString("Visible") : QString("Hidden");

        setItem(i, ColHwnd,   makeItem(QString::number(w.hwnd), false, dim));
        setItem(i, ColTitle,  makeItem(w.title, false, dim));
        setItem(i, ColClass,  makeItem(w.className, false, dim));
        setItem(i, ColStatus, makeItem(status, true, dim));
        setItem(i, ColPid,    makeItem(QString::number(w.pid), true, dim));
        setItem(i, ColX,      makeItem(QString::number(w.x), true, dim));
        setItem(i, ColY,      makeItem(QString::number(w.y), true, dim));
        setItem(i, ColW,      makeItem(QString::number(w.w), true, dim));
        setItem(i, ColH,      makeItem(QString::number(w.h), true, dim));

        if (active) {
            activeRow = i;
            for (int c = 0; c < ColCount; ++c) {
                QFont bold = item(i, c)->font();
                bold.setBold(true);
                item(i, c)->setFont(bold);
                item(i, c)->setToolTip("The active window: the buttons and tabs act on this one");
            }
        }
    }

    if (activeRow >= 0)
        selectRow(activeRow);
    verticalScrollBar()->setValue(scroll);
    m_populating = false;
}

void WindowTable::setEmptyMessage(const QString& text)
{
    if (m_emptyMessage == text)
        return;
    m_emptyMessage = text;
    viewport()->update();
}

bool WindowTable::viewportEvent(QEvent* event)
{
    const bool handled = QTableWidget::viewportEvent(event);
    if (event->type() == QEvent::Paint && rowCount() == 0 && !m_emptyMessage.isEmpty()) {
        QPainter p(viewport());
        p.setPen(QColor(0x8b, 0x8f, 0x95));
        p.drawText(viewport()->rect().adjusted(24, 0, -24, 0),
                   Qt::AlignCenter | Qt::TextWordWrap, m_emptyMessage);
    }
    return handled;
}
