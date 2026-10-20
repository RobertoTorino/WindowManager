#include "../../window/GameWatch.h"

#include "../../core/Logger.h"
#include "../../window/GameWatchModel.h"

#include <QFileInfo>
#include <QString>
#include <QTimer>

#include <Windows.h>

#include <set>
#include <vector>

// WinEvent hooks, out of context, so the callbacks run on this (the GUI) thread from its message loop:
// window create / destroy / show / hide / location for the game's process, and the foreground for all.
// Location changes come many times a second while a window is dragged, so windows are read 300 ms after
// the last report. The mouse clip has no event: it is read once a second.

namespace {

constexpr int kSettleMs = 300;
constexpr int kClipPollMs = 1000;

GameWatch::Impl *g_active = nullptr;   // the one watch with hooks installed (WinEvent procs are plain functions)

Watch::Rect toRect(const RECT &r)
{
    return {int(r.left), int(r.top), int(r.right - r.left), int(r.bottom - r.top)};
}

QString windowText(HWND hwnd)
{
    wchar_t text[256] = {};
    GetWindowTextW(hwnd, text, 256);
    return QString::fromWCharArray(text);
}

QString className(HWND hwnd)
{
    wchar_t text[128] = {};
    GetClassNameW(hwnd, text, 128);
    return QString::fromWCharArray(text);
}

QString exeName(DWORD pid)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        return {};
    wchar_t path[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    const bool ok = QueryFullProcessImageNameW(process, 0, path, &size);
    CloseHandle(process);
    return ok ? QFileInfo(QString::fromWCharArray(path, int(size))).fileName() : QString();
}

Watch::WindowState readState(HWND hwnd)
{
    Watch::WindowState s;
    RECT r{};
    GetWindowRect(hwnd, &r);
    s.rect = toRect(r);
    RECT c{};
    GetClientRect(hwnd, &c);
    POINT origin{0, 0};
    ClientToScreen(hwnd, &origin);
    s.client = {int(origin.x), int(origin.y), int(c.right - c.left), int(c.bottom - c.top)};
    s.visible = IsWindowVisible(hwnd) != FALSE;
    s.framed = (GetWindowLongPtrW(hwnd, GWL_STYLE) & (WS_CAPTION | WS_THICKFRAME)) != 0;
    s.title = windowText(hwnd);
    s.className = className(hwnd);
    return s;
}

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD);

} // namespace

struct GameWatch::Impl
{
    explicit Impl(GameWatch *owner)
        : owner(owner)
        , settle(owner)
        , clipPoll(owner)
    {
        settle.setSingleShot(true);
        settle.setInterval(kSettleMs);
        QObject::connect(&settle, &QTimer::timeout, owner, [this]() { flush(); });
        clipPoll.setInterval(kClipPollMs);
        QObject::connect(&clipPoll, &QTimer::timeout, owner, [this]() { checkClip(); });
    }

    ~Impl() { stop(); }

    void start(qint64 newPid)
    {
        pid = DWORD(newPid);
        g_active = this;
        const DWORD flags = WINEVENT_OUTOFCONTEXT;
        hooks.push_back(SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, nullptr, onWinEvent, pid, 0, flags));
        hooks.push_back(SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                        onWinEvent, pid, 0, flags));
        hooks.push_back(SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, onWinEvent,
                                        0, 0, flags));
        Logger::instance().info(QString("Watching the game (pid %1): its windows, the foreground and the mouse")
                                    .arg(pid).toStdString(), "Watch");

        // The windows it already has
        EnumWindows([](HWND hwnd, LPARAM self) -> BOOL {
            DWORD owner = 0;
            GetWindowThreadProcessId(hwnd, &owner);
            auto *impl = reinterpret_cast<Impl *>(self);
            if (owner == impl->pid)
                impl->pending.insert(hwnd);
            return TRUE;
        }, reinterpret_cast<LPARAM>(this));
        flush();
        foreground(GetForegroundWindow());
        checkClip();
        clipPoll.start();
    }

    void stop()
    {
        for (HWINEVENTHOOK hook : hooks) {
            if (hook)
                UnhookWinEvent(hook);
        }
        if (!hooks.empty())
            Logger::instance().info("Stopped watching the game", "Watch");
        hooks.clear();
        if (g_active == this)
            g_active = nullptr;
        settle.stop();
        clipPoll.stop();
        pending.clear();
        tracker.clear();
        lastForeground = nullptr;
        lastClip.reset();
        clipKnown = false;
        pid = 0;
    }

    void event(DWORD type, HWND hwnd)
    {
        if (type == EVENT_SYSTEM_FOREGROUND) {
            foreground(hwnd);
            return;
        }
        if (GetAncestor(hwnd, GA_ROOT) != hwnd)
            return;   // child windows and controls
        if (type == EVENT_OBJECT_DESTROY) {
            pending.erase(hwnd);
            if (const auto line = tracker.destroyed(quintptr(hwnd))) {
                Logger::instance().info(line->toStdString(), "Watch");
                emit owner->windowsChanged();
            }
            return;
        }
        pending.insert(hwnd);
        settle.start();   // read it once it has stopped changing
    }

    void flush()
    {
        bool changed = false;
        for (HWND hwnd : pending) {
            if (!IsWindow(hwnd))
                continue;
            if (const auto line = tracker.update(quintptr(hwnd), readState(hwnd))) {
                Logger::instance().info(line->toStdString(), "Watch");
                changed = true;
            }
        }
        pending.clear();
        if (changed)
            emit owner->windowsChanged();
    }

    void foreground(HWND hwnd)
    {
        if (!hwnd || hwnd == lastForeground)
            return;
        lastForeground = hwnd;
        DWORD owner = 0;
        GetWindowThreadProcessId(hwnd, &owner);
        const auto who = owner == pid                    ? Watch::Owner::Game
                         : owner == GetCurrentProcessId() ? Watch::Owner::ThisApp
                                                          : Watch::Owner::Other;
        const QString title = who == Watch::Owner::Other ? QString() : windowText(hwnd);
        Logger::instance().info(Watch::foregroundLine(who, exeName(owner), title).toStdString(), "Watch");
    }

    void checkClip()
    {
        RECT clip{};
        std::optional<Watch::Rect> now;
        if (GetClipCursor(&clip))
            now = toRect(clip);
        if (clipKnown && now == lastClip)
            return;
        clipKnown = true;
        lastClip = now;
        const Watch::Rect desktop{GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
                                  GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
        Logger::instance().info(Watch::clipLine(now, desktop).toStdString(), "Watch");
    }

    GameWatch *owner;
    DWORD pid{0};
    std::vector<HWINEVENTHOOK> hooks;
    std::set<HWND> pending;
    Watch::WindowTracker tracker;
    HWND lastForeground{nullptr};
    std::optional<Watch::Rect> lastClip;
    bool clipKnown{false};
    QTimer settle;
    QTimer clipPoll;
};

namespace {

void CALLBACK onWinEvent(HWINEVENTHOOK, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD, DWORD)
{
    if (!g_active || !hwnd || idObject != OBJID_WINDOW || idChild != CHILDID_SELF)
        return;
    g_active->event(event, hwnd);
}

} // namespace

GameWatch::GameWatch(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Impl>(this))
{
}

GameWatch::~GameWatch() = default;

bool GameWatch::supported()
{
    return true;
}

void GameWatch::watch(qint64 pid)
{
    if (DWORD(pid) == d->pid)
        return;
    d->stop();
    if (pid)
        d->start(pid);
}
