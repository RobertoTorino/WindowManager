#include "GameOverlay.h"

#include <QLabel>
#include <QPainter>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kMargin = 16;    // from the monitor's edge, physical pixels
constexpr int kWidth = 520;    // the panel's text width; long warnings wrap

} // namespace

GameOverlay::GameOverlay()
    : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput)
{
    setObjectName("gameOverlay");
    setWindowTitle("WindowManager game info");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);   // the game keeps the keyboard
    setAttribute(Qt::WA_TransparentForMouseEvents);

    m_text = new QLabel(this);
    m_text->setTextFormat(Qt::RichText);
    m_text->setWordWrap(true);
    m_text->setFixedWidth(kWidth);
    m_text->setStyleSheet("color: #f2f2f2; background: transparent; font-size: 10pt;");
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(12, 10, 12, 10);
    v->addWidget(m_text);
}

void GameOverlay::setCorner(Corner corner)
{
    m_corner = corner;
    place();
}

void GameOverlay::setCapturable(bool capturable)
{
    m_capturable = capturable;
    applyAffinity();
}

void GameOverlay::display(const QString &title, const std::vector<Picture::Line> &lines, const QRect &monitor)
{
    QString html = QString("<div style='font-weight:600; font-size:11pt; margin-bottom:4px;'>%1</div>"
                           "<table cellspacing='0' cellpadding='1'>")
                       .arg(title.toHtmlEscaped());
    for (const Picture::Line &line : lines) {
        const QString color = line.warn ? "#ffb347" : "#f2f2f2";
        if (line.label.isEmpty()) {   // a warning: the whole width
            html += QString("<tr><td colspan='2' style='color:%1; padding-top:3px;'>%2 %3</td></tr>")
                        .arg(color, QString::fromUtf8("⚠"), line.value.toHtmlEscaped());
        } else {
            html += QString("<tr><td style='color:#a8b0bc; padding-right:10px;'>%1</td>"
                            "<td style='color:%2;'>%3</td></tr>")
                        .arg(line.label.toHtmlEscaped(), color, line.value.toHtmlEscaped());
        }
    }
    html += "</table>";
    if (html != m_text->text())
        m_text->setText(html);
    m_monitor = monitor;
    adjustSize();
    if (!isVisible())
        show();
    place();
    raiseAboveAll();
}

void GameOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QColor(255, 255, 255, 40));
    p.setBrush(QColor(12, 14, 20, 228));
    p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 8, 8);
}

void GameOverlay::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    applyAffinity();
}

void GameOverlay::applyAffinity()
{
#ifdef Q_OS_WIN
    // By default not in screenshots, snapshots (a copy of the screen) or recordings: the panel is for the player.
    if (testAttribute(Qt::WA_WState_Created))
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), m_capturable ? WDA_NONE : WDA_EXCLUDEFROMCAPTURE);
#endif
}

void GameOverlay::place()
{
    if (m_monitor.isEmpty() || !isVisible())
        return;
    const bool left = m_corner == Corner::TopLeft || m_corner == Corner::BottomLeft;
    const bool top = m_corner == Corner::TopLeft || m_corner == Corner::TopRight;
#ifdef Q_OS_WIN
    // In physical pixels: Qt converts a position with the scale of the monitor the window is on now, which is
    // wrong when the game's monitor has another scale. Moving it there first lets it take that monitor's scale
    // (and size); the corner is then worked out from the size it has there.
    const HWND h = reinterpret_cast<HWND>(winId());
    const HMONITOR target = MonitorFromPoint(POINT{m_monitor.center().x(), m_monitor.center().y()},
                                             MONITOR_DEFAULTTONEAREST);
    const UINT flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;
    if (MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST) != target)
        SetWindowPos(h, nullptr, m_monitor.left() + kMargin, m_monitor.top() + kMargin, 0, 0, flags);
    RECT r{};
    GetWindowRect(h, &r);
    const int w = r.right - r.left;
    const int hgt = r.bottom - r.top;
    const int x = left ? m_monitor.left() + kMargin : m_monitor.left() + m_monitor.width() - w - kMargin;
    const int y = top ? m_monitor.top() + kMargin : m_monitor.top() + m_monitor.height() - hgt - kMargin;
    if (r.left != x || r.top != y)
        SetWindowPos(h, nullptr, x, y, 0, 0, flags);
#else
    const int x = left ? m_monitor.left() + kMargin : m_monitor.right() - width() - kMargin;
    const int y = top ? m_monitor.top() + kMargin : m_monitor.bottom() - height() - kMargin;
    if (pos() != QPoint(x, y))
        move(x, y);
#endif
}

void GameOverlay::raiseAboveAll()
{
#ifdef Q_OS_WIN
    // Magpie puts its full-screen window above every topmost window when the focus changes.
    SetWindowPos(reinterpret_cast<HWND>(winId()), HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
#else
    raise();
#endif
}
