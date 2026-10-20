#pragma once

#include <QString>
#include <QtGlobal>

#include <map>
#include <optional>

/**
 * Pure logic behind GameWatch's log lines (tst_gamewatch): what changed about the game's windows,
 * who has the foreground, where the mouse is held. No OS calls; GameWatch feeds it what the
 * WinEvent hooks report.
 */
namespace Watch {

struct Rect
{
    int x{0}, y{0}, w{0}, h{0};
    bool operator==(const Rect&) const = default;
    bool empty() const { return w <= 0 || h <= 0; }
};

QString describe(const Rect& r);   // "1920x1080 at 0,0"

struct WindowState
{
    Rect    rect;          // the window, screen pixels
    Rect    client;        // the drawn area, screen pixels
    bool    visible{false};
    bool    framed{false}; // has a title bar / border
    QString title;
    QString className;
    bool operator==(const WindowState&) const = default;
};

/**
 * Remembers what was last logged per window and says what to log when a window is reported again.
 * Only changes are logged; a window gets a full line the first time it is seen visible (windows that
 * never show, like IME and tooltip helpers, are left out).
 */
class WindowTracker
{
public:
    // A line when something worth logging changed; nullopt when it is the same as last time.
    std::optional<QString> update(quintptr hwnd, const WindowState& state);
    std::optional<QString> destroyed(quintptr hwnd);
    void clear() { m_windows.clear(); }
    size_t count() const { return m_windows.size(); }

private:
    std::map<quintptr, WindowState> m_windows;
};

enum class Owner { Game, ThisApp, Other };

// "Foreground: TekkenGame-Win64-Shipping.exe 'TEKKEN 7' (the game)". The title is only written for the
// game and this app: other programs' titles may be private (documents, web pages).
QString foregroundLine(Owner owner, const QString& exeName, const QString& title);

// Where the mouse is held (ClipCursor). `clip` is nullopt or covers the whole desktop: free.
QString clipLine(const std::optional<Rect>& clip, const Rect& desktop);

} // namespace Watch
