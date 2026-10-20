#include "ScaledView.h"

#include "core/Logger.h"
#include "window/MonitorHelper.h"
#include "window/Supersample.h"

#include <QTimer>
#include <QWidget>

#include <Windows.h>
#include <windowsx.h>
#include <dwmapi.h>

struct ScaledView::State
{
    HWND      game{nullptr};
    HTHUMBNAIL thumb{nullptr};

    // what we changed on the game window, so stop() can undo it
    RECT      origRect{};
    LONG_PTR  origStyle{0};
    LONG_PTR  origExStyle{0};

    // where we put it
    int  parkX{0};
    int  parkY{0};
    int  renderW{0};
    int  renderH{0};
    int  reasserts{0};           // corrections in total (for the log)
    Supersample::ReassertPolicy policy;
    bool warnedResize{false};

    // diagnostics: the monitor asked for, and the last layout written to the log
    RECT    monitor{};
    QString lastLayout;
};

namespace {

QString rectText(const RECT &r)
{
    return QString("%1x%2 at %3,%4").arg(r.right - r.left).arg(r.bottom - r.top).arg(r.left).arg(r.top);
}

// Everything that decides what the view shows, in one line: the monitor, the overlay (and its DPI: a
// monitor with another scale makes Windows resize it), the game window, its drawn area, the picture.
QString layoutText(const RECT &monitor, HWND overlay, HWND game, const Fit::Rect &dest)
{
    RECT o{}, g{}, c{};
    GetWindowRect(overlay, &o);
    GetWindowRect(game, &g);
    GetClientRect(game, &c);
    POINT origin{0, 0};
    ClientToScreen(game, &origin);
    const RECT drawn{origin.x, origin.y, origin.x + c.right, origin.y + c.bottom};
    QString line = QString("monitor %1, overlay %2 (scale %3 %), game window %4 (scale %5 %), drawn area %6, "
                           "picture %7x%8 at %9,%10 inside the overlay")
                       .arg(rectText(monitor), rectText(o))
                       .arg(GetDpiForWindow(overlay) * 100 / 96)
                       .arg(rectText(g))
                       .arg(GetDpiForWindow(game) * 100 / 96)
                       .arg(rectText(drawn))
                       .arg(dest.width()).arg(dest.height()).arg(dest.left).arg(dest.top);
    if (o.left != monitor.left || o.top != monitor.top || o.right != monitor.right || o.bottom != monitor.bottom)
        line += " - THE OVERLAY DOES NOT COVER THE MONITOR";
    return line;
}

} // namespace

// The overlay forwards raw mouse messages (exact physical pixels) to the owner. Once pinned it keeps
// exactly the monitor's rectangle: when it lands on a monitor with another display scale, Windows and
// Qt resize it (it was seen at Qt's default 640x480, drawing the picture far too small).
class ScaledView::Overlay : public QWidget
{
public:
    explicit Overlay(ScaledView *owner)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                               | Qt::WindowDoesNotAcceptFocus)
        , m_owner(owner)
    {}

    void pin(const RECT &r)
    {
        m_pin = r;
        m_pinned = true;
    }

protected:
    bool nativeEvent(const QByteArray &type, void *message, qintptr *result) override
    {
        const MSG *m = static_cast<const MSG *>(message);
        if (m_pinned && m->message == WM_WINDOWPOSCHANGING) {
            auto *wp = reinterpret_cast<WINDOWPOS *>(m->lParam);
            wp->x  = m_pin.left;
            wp->y  = m_pin.top;
            wp->cx = m_pin.right - m_pin.left;
            wp->cy = m_pin.bottom - m_pin.top;
            wp->flags &= ~UINT(SWP_NOMOVE | SWP_NOSIZE);
            return false;   // Windows and Qt go on with the corrected position
        }
        if (m_owner->handleMouse(m->message, m->wParam, static_cast<qintptr>(m->lParam))) {
            *result = 0;
            return true;
        }
        return QWidget::nativeEvent(type, message, result);
    }

private:
    ScaledView *m_owner;
    RECT        m_pin{};
    bool        m_pinned{false};
};

ScaledView::ScaledView(QObject *parent)
    : QObject(parent)
    , m_state(new State)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(400);
    connect(m_timer, &QTimer::timeout, this, &ScaledView::tick);
}

ScaledView::~ScaledView()
{
    if (m_running)
        stop();
    delete m_state;
}

bool ScaledView::start(const Options &o, QString *error)
{
    auto fail = [&](const QString &text) {
        Logger::instance().warn("ScaledView: " + text.toStdString(), "ScaledView");
        if (error)
            *error = text;
        return false;
    };

    if (m_running)
        stop();

    HWND game = reinterpret_cast<HWND>(o.gameHwnd);
    if (!game || !IsWindow(game))
        return fail("The game window is not available. Select it first (Scan).");
    if (o.renderW < 200 || o.renderH < 200 || o.renderW > 16384 || o.renderH > 16384)
        return fail(QString("Render size %1 x %2 is out of range.").arg(o.renderW).arg(o.renderH));

    State &s = *m_state;
    s = State{};
    s.game = game;
    s.renderW = o.renderW;
    s.renderH = o.renderH;
    GetWindowRect(game, &s.origRect);
    s.origStyle = GetWindowLongPtr(game, GWL_STYLE);
    s.origExStyle = GetWindowLongPtr(game, GWL_EXSTYLE);
    s.monitor = RECT{o.monitor.left, o.monitor.top, o.monitor.right, o.monitor.bottom};
    Logger::instance().info(QString("ScaledView: game window before: %1, render size asked %2x%3")
                                .arg(rectText(s.origRect)).arg(o.renderW).arg(o.renderH).toStdString(),
                            "ScaledView");

    // Overlay first: black, borderless, topmost, never takes focus.
    m_forwardMouse = o.forwardMouse;
    m_mouseCaptured = false;
    m_overlay = new Overlay(this);
    m_overlay->setAttribute(Qt::WA_ShowWithoutActivating);
    m_overlay->setStyleSheet("background: #000;");
    m_overlay->show();
    const HWND overlay = reinterpret_cast<HWND>(m_overlay->winId());
    SetWindowPos(overlay, HWND_TOPMOST, o.monitor.left, o.monitor.top,
                 o.monitor.width(), o.monitor.height(), SWP_NOACTIVATE | SWP_SHOWWINDOW);
    static_cast<Overlay *>(m_overlay)->pin(s.monitor);
    SetWindowPos(overlay, nullptr, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOZORDER);   // the pin applies it
    m_monitorIndex = o.monitorIndex;

    // Game window: normal state, no frame (so client == window), big, parked.
    if (IsIconic(game) || IsZoomed(game))
        ShowWindow(game, SW_RESTORE);

    // A popup-style window is not limited to the desktop size the way a normal
    // (overlapped) window is, so it can be larger than the monitor.
    LONG_PTR style = (s.origStyle & ~static_cast<LONG_PTR>(WS_OVERLAPPEDWINDOW)) | WS_POPUP;
    SetWindowLongPtr(game, GWL_STYLE, style);
    LONG_PTR ex = s.origExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE);
    SetWindowLongPtr(game, GWL_EXSTYLE, ex);

    std::vector<Fit::Rect> monitors;
    for (const auto &m : MonitorHelper::enumerateMonitors())
        monitors.push_back(Fit::Rect::fromXYWH(m.x, m.y, m.w, m.h));
    const auto park = Supersample::parkingOrigin(monitors, o.monitor, o.renderW, o.renderH);
    s.parkX = park.x;
    s.parkY = park.y;
    SetWindowPos(game, nullptr, s.parkX, s.parkY, s.renderW, s.renderH,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);

    // Live scaled copy of the game window on the overlay.
    const HRESULT hr = DwmRegisterThumbnail(overlay, game, &s.thumb);
    if (FAILED(hr) || !s.thumb) {
        restoreGameWindow();
        delete m_overlay;
        m_overlay = nullptr;
        return fail(QString("Windows refused to mirror this window (error 0x%1). "
                            "If the game runs as administrator, run this app as administrator too.")
                        .arg(static_cast<quint32>(hr), 8, 16, QChar('0')));
    }

    m_running = true;
    updateThumbnail();

    const Fit::Rect shown = Supersample::letterbox(o.renderW, o.renderH, o.monitor.width(), o.monitor.height());
    m_description = QString("Rendering %1 x %2, shown as %3 x %4 on monitor %5")
                        .arg(o.renderW).arg(o.renderH).arg(shown.width()).arg(shown.height())
                        .arg(o.monitorIndex);
    Logger::instance().info("ScaledView started: " + m_description.toStdString(), "ScaledView");

    // Keep keyboard / gamepad focus on the game, not on this app.
    Logger::instance().info("ScaledView: bringing the game to the front", "ScaledView");
    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(game);

    m_timer->start();
    emit runningChanged(true);
    emit message(m_description + ". Stop with Ctrl+Alt+D.");
    return true;
}

void ScaledView::stop(const QString &reason)
{
    if (!m_running && !m_overlay)
        return;

    m_timer->stop();
    if (m_mouseCaptured) {
        ReleaseCapture();
        m_mouseCaptured = false;
    }
    State &s = *m_state;
    if (s.thumb) {
        DwmUnregisterThumbnail(s.thumb);
        s.thumb = nullptr;
    }
    delete m_overlay;
    m_overlay = nullptr;

    restoreGameWindow();

    const bool was = m_running;
    m_running = false;
    Logger::instance().info("ScaledView stopped" + (reason.isEmpty() ? std::string()
                            : ": " + reason.toStdString()), "ScaledView");
    if (was) {
        emit runningChanged(false);
        emit message(reason.isEmpty() ? QString("Downscaled view stopped.") : reason);
    }
}

void ScaledView::restoreGameWindow()
{
    State &s = *m_state;
    if (!s.game || !IsWindow(s.game))
        return;

    SetWindowLongPtr(s.game, GWL_STYLE, s.origStyle);
    SetWindowLongPtr(s.game, GWL_EXSTYLE, s.origExStyle);
    SetWindowPos(s.game, nullptr, s.origRect.left, s.origRect.top,
                 s.origRect.right - s.origRect.left, s.origRect.bottom - s.origRect.top,
                 SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
}

void ScaledView::updateThumbnail()
{
    State &s = *m_state;
    if (!s.thumb || !m_overlay)
        return;

    RECT area{};
    GetClientRect(reinterpret_cast<HWND>(m_overlay->winId()), &area);
    RECT client{};
    GetClientRect(s.game, &client);
    const int cw = client.right - client.left;
    const int ch = client.bottom - client.top;
    if (cw <= 0 || ch <= 0)
        return;

    const Fit::Rect dest = Supersample::letterbox(cw, ch, area.right - area.left, area.bottom - area.top);
    m_dest = dest;
    m_gameW = cw;
    m_gameH = ch;

    DWM_THUMBNAIL_PROPERTIES p{};
    p.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
    p.rcDestination = RECT{dest.left, dest.top, dest.right, dest.bottom};
    p.fVisible = TRUE;
    p.opacity = 255;
    p.fSourceClientAreaOnly = TRUE;
    DwmUpdateThumbnailProperties(s.thumb, &p);

    const QString layout = layoutText(s.monitor, reinterpret_cast<HWND>(m_overlay->winId()), s.game, dest);
    if (layout != s.lastLayout) {
        s.lastLayout = layout;
        Logger::instance().info("ScaledView layout: " + layout.toStdString(), "ScaledView");
    }
}

void ScaledView::tick()
{
    State &s = *m_state;
    if (!s.game || !IsWindow(s.game)) {
        stop("The game window was closed, so the downscaled view stopped.");
        return;
    }

    // A minimized window has no picture to mirror.
    if (IsIconic(s.game))
        ShowWindow(s.game, SW_SHOWNOACTIVATE);

    // Some games resize their window again: after start-up, or every time they get the focus.
    RECT r{};
    GetWindowRect(s.game, &r);
    const bool moved = r.left != s.parkX || r.top != s.parkY
                    || (r.right - r.left) != s.renderW || (r.bottom - r.top) != s.renderH;
    if (moved) {
        if (s.policy.allow(qint64(GetTickCount64()))) {
            ++s.reasserts;
            s.warnedResize = false;
            Logger::instance().info(QString("ScaledView: the game changed its window to %1; putting it back to "
                                            "%2x%3 at %4,%5 (correction %6)")
                                        .arg(rectText(r)).arg(s.renderW).arg(s.renderH).arg(s.parkX).arg(s.parkY)
                                        .arg(s.reasserts).toStdString(),
                                    "ScaledView");
            SetWindowPos(s.game, nullptr, s.parkX, s.parkY, s.renderW, s.renderH,
                         SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        } else if (!s.warnedResize) {
            s.warnedResize = true;
            Logger::instance().warn(QString("ScaledView: the game resized its window %1 times within 10 s; "
                                            "following its size (%2) until it is quiet")
                                        .arg(Supersample::ReassertPolicy::kMax).arg(rectText(r)).toStdString(),
                                    "ScaledView");
            emit message("The game keeps changing its own window size; the view follows its shape "
                         "but the render size may differ from what you asked for.");
        }
    }

    updateThumbnail();
}

bool ScaledView::handleMouse(unsigned int msg, quintptr wParam, qintptr lParam)
{
    switch (msg) {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN: case WM_XBUTTONUP: case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
        break;
    default:
        return false;
    }

    State &s = *m_state;
    if (!m_running || !m_forwardMouse || !m_overlay || !s.game || !IsWindow(s.game))
        return false;

    const HWND overlay = reinterpret_cast<HWND>(m_overlay->winId());
    POINT pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    const bool wheel = msg == WM_MOUSEWHEEL || msg == WM_MOUSEHWHEEL;
    if (wheel)                       // wheel messages carry screen coordinates
        ScreenToClient(overlay, &pt);

    // While a button is held the drag keeps tracking even outside the picture.
    Supersample::Point g;
    if (!Supersample::mapPoint(m_dest, m_gameW, m_gameH, pt.x, pt.y, m_mouseCaptured, g))
        return true;                 // over the black bars: swallow

    switch (msg) {
    case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: case WM_XBUTTONDOWN:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDBLCLK: case WM_MBUTTONDBLCLK: case WM_XBUTTONDBLCLK:
        if (!m_mouseCaptured) {
            SetCapture(overlay);
            m_mouseCaptured = true;
        }
        break;
    default:
        break;
    }

    LPARAM out = MAKELPARAM(g.x, g.y);
    if (wheel) {
        POINT origin{0, 0};
        ClientToScreen(s.game, &origin);
        out = MAKELPARAM(origin.x + g.x, origin.y + g.y);
    }
    PostMessage(s.game, msg, static_cast<WPARAM>(wParam), out);

    switch (msg) {
    case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: case WM_XBUTTONUP: {
        const WPARAM held = static_cast<WPARAM>(wParam)
                            & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2);
        if (!held && m_mouseCaptured) {
            ReleaseCapture();
            m_mouseCaptured = false;
        }
        break;
    }
    default:
        break;
    }
    return true;
}
