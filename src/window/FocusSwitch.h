#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

/**
 * Pure logic for the focus switch: who has the keyboard (the game, this app or another program), what the
 * hotkey does about it, and the hotkey itself. Games like Tekken 7 keep the focus, so the app's own
 * shortcuts never arrive; a global hotkey (RegisterHotKey, registered while the game runs) does.
 */
namespace FocusSwitch {

enum class Owner
{
    None,    // no game running (or not found yet)
    Game,
    App,
    Other,   // another program, or nothing
};

// `foregroundRunsGameExe`: the foreground window's process runs the game's exe (a second game process).
Owner ownerOf(qint64 foregroundPid, qint64 appPid, qint64 gamePid, bool foregroundRunsGameExe);

enum class Action { None, ShowApp, FocusGame };
Action toggle(Owner owner);   // the game has it -> this app; anything else -> the game

QString label(Owner owner);   // "Focus: game"; empty for None

// A key with modifiers, as Windows' RegisterHotKey wants it: virtualKey is a Windows virtual-key code.
struct Hotkey
{
    bool ctrl{false};
    bool alt{false};
    bool shift{false};
    bool win{false};
    int  virtualKey{0};

    bool operator==(const Hotkey&) const = default;
};

// A key that has to be pressed twice within `windowMs` (the terminate-game hotkey: a single slip of the
// finger must not end the game). press() is true for the second press; the next one starts over.
class DoublePress
{
public:
    explicit DoublePress(qint64 windowMs = 2000) : m_windowMs(windowMs) {}
    bool press(qint64 nowMs);
    bool waiting(qint64 nowMs) const;   // one press made, the second still in time

private:
    qint64 m_windowMs;
    qint64 m_first{-1};
};

// "F1", "Ctrl+Alt+G", "Shift+F12", "Pause" (case-insensitive); nullopt for an empty or unknown key.
std::optional<Hotkey> parseHotkey(const QString& text);
QString hotkeyText(const Hotkey& key);   // "Ctrl+Alt+G"

} // namespace FocusSwitch
