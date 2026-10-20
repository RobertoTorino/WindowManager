#include "GameWatchModel.h"

#include <QStringList>

namespace Watch {

namespace {

QString hex(quintptr hwnd)
{
    return "0x" + QString::number(hwnd, 16);
}

QString full(quintptr hwnd, const WindowState& s)
{
    QString line = QString("%1 '%2' (%3): window %4, drawn area %5, %6")
                       .arg(hex(hwnd), s.title, s.className, describe(s.rect), describe(s.client),
                            s.framed ? "framed" : "borderless");
    if (!s.visible)
        line += ", hidden";
    return line;
}

} // namespace

QString describe(const Rect& r)
{
    return QString("%1x%2 at %3,%4").arg(r.w).arg(r.h).arg(r.x).arg(r.y);
}

std::optional<QString> WindowTracker::update(quintptr hwnd, const WindowState& state)
{
    const auto it = m_windows.find(hwnd);
    if (it == m_windows.end()) {
        if (!state.visible)
            return std::nullopt;   // helper windows (IME, tooltips, lists) that never show: noise
        m_windows[hwnd] = state;
        return "Game window " + full(hwnd, state);
    }
    const WindowState old = it->second;
    if (old == state)
        return std::nullopt;
    it->second = state;

    QStringList changes;
    if (old.visible != state.visible)
        changes << (state.visible ? "shown" : "hidden");
    if (old.rect != state.rect)
        changes << QString("window %1 -> %2").arg(describe(old.rect), describe(state.rect));
    if (old.client != state.client)
        changes << QString("drawn area %1 -> %2").arg(describe(old.client), describe(state.client));
    if (old.framed != state.framed)
        changes << (state.framed ? "frame added" : "frame removed");
    if (old.title != state.title)
        changes << QString("title '%1'").arg(state.title);
    if (changes.isEmpty())   // only the class changed: not worth a line
        return std::nullopt;
    return QString("Game window %1: %2").arg(hex(hwnd), changes.join(", "));
}

std::optional<QString> WindowTracker::destroyed(quintptr hwnd)
{
    if (m_windows.erase(hwnd) == 0)
        return std::nullopt;   // never logged: not one of the game's windows
    return QString("Game window %1 closed").arg(hex(hwnd));
}

QString foregroundLine(Owner owner, const QString& exeName, const QString& title)
{
    const QString exe = exeName.isEmpty() ? QString("unknown program") : exeName;
    switch (owner) {
    case Owner::Game:    return QString("Foreground: %1 '%2' (the game)").arg(exe, title);
    case Owner::ThisApp: return QString("Foreground: WindowManager '%1' (this app)").arg(title);
    case Owner::Other:   break;
    }
    return QString("Foreground: %1 (another program)").arg(exe);
}

QString clipLine(const std::optional<Rect>& clip, const Rect& desktop)
{
    if (!clip || *clip == desktop || clip->empty())
        return "Mouse: free";
    return "Mouse: held inside " + describe(*clip);
}

} // namespace Watch
