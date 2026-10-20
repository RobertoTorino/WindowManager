#include "FocusSwitch.h"

#include <QStringList>

namespace FocusSwitch {

namespace {

struct NamedKey
{
    const char* name;
    int         virtualKey;
};

// Windows virtual-key codes (WinUser.h) for the keys that make sense as a hotkey besides F1-F24, A-Z, 0-9
constexpr NamedKey kNamedKeys[] = {
    {"Pause", 0x13},  {"ScrollLock", 0x91}, {"Insert", 0x2D}, {"Delete", 0x2E}, {"Home", 0x24},
    {"End", 0x23},    {"PageUp", 0x21},     {"PageDown", 0x22}, {"Space", 0x20}, {"Tab", 0x09},
};
constexpr int kVkF1 = 0x70;

} // namespace

Owner ownerOf(qint64 foregroundPid, qint64 appPid, qint64 gamePid, bool foregroundRunsGameExe)
{
    if (foregroundPid != 0 && (foregroundPid == gamePid || foregroundRunsGameExe))
        return Owner::Game;
    if (gamePid == 0)
        return Owner::None;
    if (foregroundPid != 0 && foregroundPid == appPid)
        return Owner::App;
    return Owner::Other;
}

Action toggle(Owner owner)
{
    switch (owner) {
    case Owner::Game:  return Action::ShowApp;
    case Owner::App:
    case Owner::Other: return Action::FocusGame;
    case Owner::None:  return Action::None;
    }
    return Action::None;
}

QString label(Owner owner)
{
    switch (owner) {
    case Owner::Game:  return "Focus: game";
    case Owner::App:   return "Focus: app";
    case Owner::Other: return "Focus: other";
    case Owner::None:  return {};
    }
    return {};
}

bool DoublePress::press(qint64 nowMs)
{
    if (waiting(nowMs)) {
        m_first = -1;
        return true;
    }
    m_first = nowMs;
    return false;
}

bool DoublePress::waiting(qint64 nowMs) const
{
    return m_first >= 0 && nowMs >= m_first && nowMs - m_first <= m_windowMs;
}

std::optional<Hotkey> parseHotkey(const QString& text)
{
    const QStringList parts = text.split('+', Qt::SkipEmptyParts);
    if (parts.isEmpty())
        return std::nullopt;
    Hotkey key;
    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts[i].trimmed();
        const bool last = i == parts.size() - 1;
        if (!last) {
            if (part.compare("Ctrl", Qt::CaseInsensitive) == 0 || part.compare("Control", Qt::CaseInsensitive) == 0)
                key.ctrl = true;
            else if (part.compare("Alt", Qt::CaseInsensitive) == 0)
                key.alt = true;
            else if (part.compare("Shift", Qt::CaseInsensitive) == 0)
                key.shift = true;
            else if (part.compare("Win", Qt::CaseInsensitive) == 0 || part.compare("Meta", Qt::CaseInsensitive) == 0)
                key.win = true;
            else
                return std::nullopt;
            continue;
        }
        if (part.size() == 1 && part[0].isLetterOrNumber() && part[0].unicode() < 128) {
            key.virtualKey = part[0].toUpper().unicode();   // 'A'-'Z' and '0'-'9' are their own codes
        } else if (part.size() >= 2 && (part[0] == 'F' || part[0] == 'f')) {
            bool ok = false;
            const int n = part.mid(1).toInt(&ok);
            if (!ok || n < 1 || n > 24)
                return std::nullopt;
            key.virtualKey = kVkF1 + n - 1;
        } else {
            for (const NamedKey& named : kNamedKeys)
                if (part.compare(named.name, Qt::CaseInsensitive) == 0)
                    key.virtualKey = named.virtualKey;
        }
    }
    if (key.virtualKey == 0)
        return std::nullopt;
    return key;
}

QString hotkeyText(const Hotkey& key)
{
    QStringList parts;
    if (key.ctrl)  parts << "Ctrl";
    if (key.alt)   parts << "Alt";
    if (key.shift) parts << "Shift";
    if (key.win)   parts << "Win";
    QString name;
    if (key.virtualKey >= kVkF1 && key.virtualKey < kVkF1 + 24)
        name = QString("F%1").arg(key.virtualKey - kVkF1 + 1);
    else if ((key.virtualKey >= 'A' && key.virtualKey <= 'Z') || (key.virtualKey >= '0' && key.virtualKey <= '9'))
        name = QChar(key.virtualKey);
    else
        for (const NamedKey& named : kNamedKeys)
            if (named.virtualKey == key.virtualKey)
                name = named.name;
    parts << name;
    return parts.join('+');
}

} // namespace FocusSwitch
