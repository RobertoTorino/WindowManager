#include "../PlatformBackend.h"

#include <xcb/xcb.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <climits>
#include <cstring>
#include <cmath>
#include <map>
#include <memory>
#include <string>

/*
 * Window access for X11 (and XWayland, where Proton games live) through plain xcb and the
 * EWMH / ICCCM conventions, so it works with any window manager that follows them.
 *
 * Monitors come from Qt (QScreen), so run the app with QT_QPA_PLATFORM=xcb (main() sets it).
 * Processes come from /proc. A Wine/Proton process is reported under its Windows image name
 * (from its command line) because that is what the saved game paths refer to.
 */

namespace {

struct Free { void operator()(void* p) const { free(p); } };
template <class T> using Reply = std::unique_ptr<T, Free>;

constexpr uint32_t kMotifDecorAll = 1;
constexpr uint32_t kMotifHintsDecorations = 2;
constexpr uint32_t kStaticGravity = 10;
constexpr uint32_t kSourceApplication = 1;
constexpr uint32_t kSourcePager = 2;   // a tool acting for the user: KWin's focus-stealing prevention allows it

enum StateAction : uint32_t { Remove = 0, Add = 1, Toggle = 2 };

class Connection
{
public:
    Connection()
    {
        m_conn = xcb_connect(nullptr, nullptr);
        if (xcb_connection_has_error(m_conn))
            return;
        m_root = xcb_setup_roots_iterator(xcb_get_setup(m_conn)).data->root;
    }
    ~Connection() { if (m_conn) xcb_disconnect(m_conn); }

    bool ok() const { return m_conn && !xcb_connection_has_error(m_conn); }
    xcb_connection_t* get() const { return m_conn; }
    xcb_window_t root() const { return m_root; }

    xcb_atom_t atom(const char* name)
    {
        const auto it = m_atoms.find(name);
        if (it != m_atoms.end())
            return it->second;
        Reply<xcb_intern_atom_reply_t> r(xcb_intern_atom_reply(
            m_conn, xcb_intern_atom(m_conn, 0, static_cast<uint16_t>(strlen(name)), name), nullptr));
        const xcb_atom_t a = r ? r->atom : XCB_ATOM_NONE;
        m_atoms[name] = a;
        return a;
    }

    // Round trip: returns once the server has handled everything sent before.
    void sync()
    {
        free(xcb_get_input_focus_reply(m_conn, xcb_get_input_focus(m_conn), nullptr));
    }

    Reply<xcb_get_property_reply_t> property(xcb_window_t w, xcb_atom_t prop, xcb_atom_t type,
                                             uint32_t longLength = 1024)
    {
        return Reply<xcb_get_property_reply_t>(xcb_get_property_reply(
            m_conn, xcb_get_property(m_conn, 0, w, prop, type, 0, longLength), nullptr));
    }

    std::vector<uint32_t> cardinals(xcb_window_t w, const char* prop, xcb_atom_t type = XCB_ATOM_CARDINAL)
    {
        std::vector<uint32_t> out;
        const auto r = property(w, atom(prop), type);
        if (r && r->format == 32) {
            const auto* data = static_cast<const uint32_t*>(xcb_get_property_value(r.get()));
            out.assign(data, data + xcb_get_property_value_length(r.get()) / 4);
        }
        return out;
    }

    QString text(xcb_window_t w, xcb_atom_t prop, xcb_atom_t type)
    {
        const auto r = property(w, prop, type);
        if (!r || r->format != 8)
            return {};
        return QString::fromUtf8(static_cast<const char*>(xcb_get_property_value(r.get())),
                                 xcb_get_property_value_length(r.get()));
    }

    // Client message to the root window, which is how a window manager is asked to do things.
    void request(xcb_window_t w, const char* type, uint32_t d0 = 0, uint32_t d1 = 0,
                 uint32_t d2 = 0, uint32_t d3 = 0, uint32_t d4 = 0)
    {
        xcb_client_message_event_t ev{};
        ev.response_type = XCB_CLIENT_MESSAGE;
        ev.format = 32;
        ev.window = w;
        ev.type = atom(type);
        ev.data.data32[0] = d0;
        ev.data.data32[1] = d1;
        ev.data.data32[2] = d2;
        ev.data.data32[3] = d3;
        ev.data.data32[4] = d4;
        xcb_send_event(m_conn, 0, m_root,
                       XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT | XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY,
                       reinterpret_cast<const char*>(&ev));
        xcb_flush(m_conn);
    }

    void changeState(xcb_window_t w, StateAction action, const char* a1, const char* a2 = nullptr)
    {
        request(w, "_NET_WM_STATE", action, atom(a1), a2 ? atom(a2) : 0, kSourceApplication);
    }

private:
    xcb_connection_t* m_conn{nullptr};
    xcb_window_t m_root{0};
    std::map<std::string, xcb_atom_t> m_atoms;
};

Connection& x()
{
    static Connection conn;
    return conn;
}

xcb_window_t toWin(quintptr w) { return static_cast<xcb_window_t>(w); }

// ----- Processes (/proc) --------------------------------------------------

bool readProc(uint32_t pid, const char* file, QByteArray& out)
{
    QFile f(QString("/proc/%1/%2").arg(pid).arg(file));
    if (!f.open(QIODevice::ReadOnly))
        return false;
    out = f.readAll();   // /proc files report size 0, so read until the end
    return true;
}

bool isWineLoader(const QString& exePath)
{
    const QString name = QFileInfo(exePath).fileName();
    return name.startsWith("wine") || name.endsWith("-preloader");
}

// The Linux folder behind a drive letter of a Wine process: the link in its prefix's dosdevices
// folder (Proton maps the Steam library to S: and / to Z:). Empty when it cannot be found.
QString wineDriveRoot(uint32_t pid, QChar drive)
{
    QByteArray env;
    if (readProc(pid, "environ", env)) {
        for (const QByteArray& entry : env.split('\0')) {
            if (!entry.startsWith("WINEPREFIX="))
                continue;
            const QString link = QString::fromUtf8(entry.mid(11)) + "/dosdevices/" + drive.toLower() + ':';
            const QString target = QFileInfo(link).symLinkTarget();
            if (!target.isEmpty())
                return target;
        }
    }
    return drive.toUpper() == 'Z' ? QString("/") : QString();
}

// "S:\steamapps\common\game.exe" of process `pid` as a Linux path; unknown drives stay Windows paths.
QString fromWinePath(uint32_t pid, const QString& path)
{
    static const QRegularExpression drive("^[A-Za-z]:\\\\");
    if (!drive.match(path).hasMatch())
        return path;
    QString rest = path.mid(3);
    rest.replace('\\', '/');
    if (const QString root = wineDriveRoot(pid, path[0]); !root.isEmpty())
        return QDir::cleanPath(root + '/' + rest);

    // The prefix cannot be read (a game in Steam's sandbox, seen from a container). Proton maps
    // the Steam folder to S:, so look for the file there.
    const QString home = QDir::homePath();
    for (const QString& steam : {home + "/.local/share/Steam", home + "/.steam/steam",
                                 home + "/.var/app/com.valvesoftware.Steam/.local/share/Steam"}) {
        const QString candidate = QDir::cleanPath(steam + '/' + rest);
        if (QFileInfo::exists(candidate))
            return candidate;
    }
    return path;
}

// Fedora Atomic (Bazzite, Silverblue) keeps homes in /var/home with /home linked to it, and in a
// container both can be separate mounts of the same folder. A path in the other form is written in
// the form of $HOME, as the app saves paths, so the two compare equal.
QString inHomeForm(const QString& path)
{
    static const QString home = QDir::homePath();
    static const QString alias = []() {
        const QString other = home.startsWith("/var/home/") ? home.mid(4)
                            : home.startsWith("/home/")     ? "/var" + home
                                                            : QString();
        struct stat a{}, h{};
        if (other.isEmpty() || ::stat(other.toLocal8Bit().constData(), &a) != 0
            || ::stat(home.toLocal8Bit().constData(), &h) != 0 || a.st_dev != h.st_dev || a.st_ino != h.st_ino)
            return QString();
        return other;
    }();
    if (!alias.isEmpty() && (path == alias || path.startsWith(alias + '/')))
        return home + path.mid(alias.size());
    return path;
}

struct StatInfo
{
    bool     ok{false};
    QString  comm;
    uint32_t ppid{0};
    uint64_t startTicks{0};
};

StatInfo readStat(uint32_t pid)
{
    StatInfo info;
    QByteArray raw;
    if (!readProc(pid, "stat", raw))
        return info;
    // "pid (comm) S ppid ..." where comm may contain spaces and parentheses
    const int open = raw.indexOf('(');
    const int close = raw.lastIndexOf(')');
    if (open < 0 || close < open)
        return info;
    info.comm = QString::fromUtf8(raw.mid(open + 1, close - open - 1));
    const QList<QByteArray> rest = raw.mid(close + 2).split(' ');
    if (rest.size() < 20)
        return info;
    info.ppid = rest[1].toUInt();
    info.startTicks = rest[19].toULongLong();
    info.ok = true;
    return info;
}

class X11Backend final : public PlatformBackend
{
public:
    // ----- Windows ---------------------------------------------------------
    std::vector<TopLevelWindow> topLevelWindows() const override
    {
        std::vector<TopLevelWindow> out;
        if (!x().ok())
            return out;
        for (const uint32_t id : clientWindows()) {
            Fit::Rect rect;
            const qint64 pid = windowPid(id);
            if (pid <= 0 || !windowRect(id, rect))
                continue;
            out.push_back({id, pid, rect});
        }
        return out;
    }

    bool isValid(quintptr window) const override
    {
        if (!window || !x().ok())
            return false;
        xcb_generic_error_t* err = nullptr;
        Reply<xcb_get_window_attributes_reply_t> r(xcb_get_window_attributes_reply(
            x().get(), xcb_get_window_attributes(x().get(), toWin(window)), &err));
        free(err);
        return r != nullptr;
    }

    WindowState windowState(quintptr window) const override
    {
        WindowState s;
        if (!isValid(window))
            return s;
        const xcb_window_t w = toWin(window);
        s.valid = true;

        const auto atoms = x().cardinals(w, "_NET_WM_STATE", XCB_ATOM_ATOM);
        const auto has = [&](const char* name) {
            return std::find(atoms.begin(), atoms.end(), x().atom(name)) != atoms.end();
        };
        s.minimized = has("_NET_WM_STATE_HIDDEN");
        s.maximized = has("_NET_WM_STATE_MAXIMIZED_VERT") && has("_NET_WM_STATE_MAXIMIZED_HORZ");
        s.topmost = has("_NET_WM_STATE_ABOVE");
        s.toolWindow = has("_NET_WM_STATE_SKIP_TASKBAR");

        // A minimized window is unmapped but still "visible" in the Windows sense.
        Reply<xcb_get_window_attributes_reply_t> attrs(xcb_get_window_attributes_reply(
            x().get(), xcb_get_window_attributes(x().get(), w), nullptr));
        s.visible = s.minimized || (attrs && attrs->map_state == XCB_MAP_STATE_VIEWABLE);

        // Decorations: absent unless a Motif hint asks for none.
        s.hasCaption = s.hasFrame = true;
        const auto motif = x().cardinals(w, "_MOTIF_WM_HINTS", x().atom("_MOTIF_WM_HINTS"));
        if (motif.size() >= 3 && (motif[0] & kMotifHintsDecorations) && motif[2] == 0)
            s.hasCaption = s.hasFrame = false;
        return s;
    }

    QString windowTitle(quintptr window) const override
    {
        const xcb_window_t w = toWin(window);
        QString title = x().text(w, x().atom("_NET_WM_NAME"), x().atom("UTF8_STRING"));
        if (title.isEmpty())
            title = x().text(w, XCB_ATOM_WM_NAME, XCB_ATOM_STRING);
        return title;
    }

    QString windowClass(quintptr window) const override
    {
        // WM_CLASS is "instance\0class\0"
        const auto r = x().property(toWin(window), XCB_ATOM_WM_CLASS, XCB_ATOM_STRING);
        if (!r || r->format != 8)
            return {};
        const QList<QByteArray> parts = QByteArray(static_cast<const char*>(xcb_get_property_value(r.get())),
                                                   xcb_get_property_value_length(r.get())).split('\0');
        return QString::fromLatin1(parts.size() > 1 ? parts[1] : parts.value(0));
    }

    qint64 windowPid(quintptr window) const override
    {
        const auto pid = x().cardinals(toWin(window), "_NET_WM_PID");
        return pid.empty() ? 0 : static_cast<qint64>(pid.front());
    }

    // The outer edge: the client area plus the window manager's frame.
    bool windowRect(quintptr window, Fit::Rect& out) const override
    {
        Fit::Rect client;
        if (!clientRect(window, client))
            return false;
        const auto e = frameExtents(toWin(window));
        out = {client.left - e.left, client.top - e.top, client.right + e.right, client.bottom + e.bottom};
        return true;
    }

    bool clientRect(quintptr window, Fit::Rect& out) const override
    {
        if (!x().ok())
            return false;
        const xcb_window_t w = toWin(window);
        Reply<xcb_get_geometry_reply_t> g(xcb_get_geometry_reply(
            x().get(), xcb_get_geometry(x().get(), w), nullptr));
        Reply<xcb_translate_coordinates_reply_t> t(xcb_translate_coordinates_reply(
            x().get(), xcb_translate_coordinates(x().get(), w, x().root(), 0, 0), nullptr));
        if (!g || !t)
            return false;
        out = Fit::Rect::fromXYWH(t->dst_x, t->dst_y, g->width, g->height);
        return true;
    }

    std::optional<Fit::Rect> visibleFrame(quintptr) const override
    {
        return std::nullopt;   // no invisible resize borders to discount
    }

    bool setBounds(quintptr window, const Fit::Rect& outer) override
    {
        if (!isValid(window))
            return false;
        const xcb_window_t w = toWin(window);

        // A whole monitor is asked of the window manager as full screen: that drops the frame and
        // ignores size limits, which Wine sets on windows that may not be resized (min = max size).
        // Any other size first leaves full screen, or the manager keeps the window where it is.
        const auto screens = monitors();
        const bool wholeMonitor = std::any_of(screens.begin(), screens.end(),
                                              [&](const Monitor& m) { return m.full == outer; });
        if (!wholeMonitor && hasState(w, "_NET_WM_STATE_FULLSCREEN")) {
            x().changeState(w, Remove, "_NET_WM_STATE_FULLSCREEN");
            waitFor([&]() { return !hasState(w, "_NET_WM_STATE_FULLSCREEN"); });
        }

        // Window managers differ in what a requested position refers to (the client or its frame),
        // so ask once, look at where the window ended up and correct the request by the difference.
        // For a monitor this also puts the window on it, which is where full screen will use.
        const auto e = frameExtents(w);
        int cx = outer.left + e.left;
        int cy = outer.top + e.top;
        int cw = std::max(1, outer.width() - e.left - e.right);
        int ch = std::max(1, outer.height() - e.top - e.bottom);

        constexpr int kAttempts = 3;
        for (int attempt = 0; attempt < kAttempts; ++attempt) {
            sendGeometry(w, cx, cy, cw, ch);

            Fit::Rect now;
            if (waitFor([&]() { return windowRect(window, now) && now == outer; }))
                return true;
            if (!windowRect(window, now))
                return false;
            cx += outer.left - now.left;
            cy += outer.top - now.top;
            cw += outer.width() - now.width();
            ch += outer.height() - now.height();
            cw = std::max(1, cw);
            ch = std::max(1, ch);
        }

        if (wholeMonitor) {
            x().changeState(w, Add, "_NET_WM_STATE_FULLSCREEN");
            Fit::Rect now;
            return waitFor([&]() { return windowRect(window, now) && now == outer; }, 15);
        }
        return true;   // the request was made; the manager may have its own limits
    }

    bool hasState(xcb_window_t w, const char* name) const
    {
        const auto atoms = x().cardinals(w, "_NET_WM_STATE", XCB_ATOM_ATOM);
        return std::find(atoms.begin(), atoms.end(), x().atom(name)) != atoms.end();
    }

    // Window manager requests are asynchronous: check `done` every 20 ms, `tries` times.
    template <typename Done>
    static bool waitFor(Done done, int tries = 5)
    {
        for (int i = 0; i < tries; ++i) {
            usleep(20000);
            if (done())
                return true;
        }
        return false;
    }

    void stripFrame(quintptr window) override
    {
        // _MOTIF_WM_HINTS: flags, functions, decorations, input mode, status
        const uint32_t hints[5] = {kMotifHintsDecorations, 0, 0, 0, 0};
        const xcb_atom_t motif = x().atom("_MOTIF_WM_HINTS");
        xcb_change_property(x().get(), XCB_PROP_MODE_REPLACE, toWin(window), motif, motif, 32, 5, hints);
        xcb_flush(x().get());
    }

    void restoreFrame(quintptr window) override
    {
        const uint32_t hints[5] = {kMotifHintsDecorations, 0, kMotifDecorAll, 0, 0};
        const xcb_atom_t motif = x().atom("_MOTIF_WM_HINTS");
        xcb_change_property(x().get(), XCB_PROP_MODE_REPLACE, toWin(window), motif, motif, 32, 5, hints);
        xcb_flush(x().get());
        // borderless full screen is the manager's full-screen state (see setBounds)
        if (hasState(toWin(window), "_NET_WM_STATE_FULLSCREEN"))
            x().changeState(toWin(window), Remove, "_NET_WM_STATE_FULLSCREEN");
    }

    void resetWindow(quintptr window) override
    {
        const xcb_window_t w = toWin(window);
        xcb_delete_property(x().get(), w, x().atom("_MOTIF_WM_HINTS"));
        x().changeState(w, Remove, "_NET_WM_STATE_ABOVE");
        x().changeState(w, Remove, "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_SKIP_PAGER");
        x().changeState(w, Remove, "_NET_WM_STATE_FULLSCREEN");
        show(window, ShowState::Restore);
    }

    void show(quintptr window, ShowState state) override
    {
        const xcb_window_t w = toWin(window);
        switch (state) {
        case ShowState::Hide:
            xcb_unmap_window(x().get(), w);
            xcb_flush(x().get());
            break;
        case ShowState::Show:
            xcb_map_window(x().get(), w);
            xcb_flush(x().get());
            break;
        case ShowState::Minimize:
            x().request(w, "WM_CHANGE_STATE", 3);   // IconicState
            break;
        case ShowState::Maximize:
            x().changeState(w, Add, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
            break;
        case ShowState::Restore:
            x().changeState(w, Remove, "_NET_WM_STATE_MAXIMIZED_VERT", "_NET_WM_STATE_MAXIMIZED_HORZ");
            if (windowState(window).minimized)
                activate(w);
            break;
        }
    }

    void bringToTop(quintptr window) override
    {
        x().request(toWin(window), "_NET_RESTACK_WINDOW", kSourceApplication, 0, 0 /* Above */);
    }

    void focus(quintptr window) override
    {
        activate(toWin(window));
    }

    quintptr foregroundWindow() const override
    {
        const auto active = x().cardinals(x().root(), "_NET_ACTIVE_WINDOW", XCB_ATOM_WINDOW);
        return active.empty() ? 0 : quintptr(active.front());
    }

    void requestClose(quintptr window) override
    {
        x().request(toWin(window), "_NET_CLOSE_WINDOW", 0, kSourceApplication);
    }

    bool supportsFlag(WindowFlag flag) const override
    {
        return flag == WindowFlag::Topmost || flag == WindowFlag::ToolWindow;
    }

    bool toggleFlag(quintptr window, WindowFlag flag) override
    {
        const xcb_window_t w = toWin(window);
        const WindowState before = windowState(window);
        if (flag == WindowFlag::Topmost) {
            x().changeState(w, Toggle, "_NET_WM_STATE_ABOVE");
            return !before.topmost;
        }
        if (flag == WindowFlag::ToolWindow) {
            x().changeState(w, Toggle, "_NET_WM_STATE_SKIP_TASKBAR", "_NET_WM_STATE_SKIP_PAGER");
            return !before.toolWindow;
        }
        return false;
    }

    // ----- Monitors --------------------------------------------------------
    std::vector<Monitor> monitors() const override
    {
        std::vector<Monitor> out;
        for (const QScreen* screen : QGuiApplication::screens()) {
            out.push_back({physical(screen, screen->geometry()), physical(screen, screen->availableGeometry())});
        }
        return out;
    }

    std::optional<Monitor> nearestMonitor(quintptr window) const override
    {
        Fit::Rect r;
        const auto all = monitors();
        if (all.empty() || !windowRect(window, r))
            return std::nullopt;

        const int cx = r.left + r.width() / 2;
        const int cy = r.top + r.height() / 2;
        const Monitor* best = &all.front();
        double bestDistance = -1;
        for (const auto& m : all) {
            // distance from the window centre to the monitor rectangle (0 when inside)
            const int dx = std::max({m.full.left - cx, 0, cx - (m.full.right - 1)});
            const int dy = std::max({m.full.top - cy, 0, cy - (m.full.bottom - 1)});
            const double d = std::hypot(dx, dy);
            if (bestDistance < 0 || d < bestDistance) {
                bestDistance = d;
                best = &m;
            }
        }
        return *best;
    }

    // ----- Processes -------------------------------------------------------
    std::vector<ProcessEntry> processes() const override
    {
        std::vector<ProcessEntry> out;
        for (const QString& entry : QDir("/proc").entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            bool numeric = false;
            const uint32_t pid = entry.toUInt(&numeric);
            if (!numeric)
                continue;
            const StatInfo stat = readStat(pid);
            if (!stat.ok)
                continue;
            const QString image = processImagePath(pid);
            out.push_back({pid, stat.ppid, image.isEmpty() ? stat.comm : QFileInfo(image).fileName()});
        }
        return out;
    }

    QString processImagePath(qint64 pid) const override
    {
        // exe is empty when it may not be read (a sandboxed game seen from a container): try cmdline
        const QString exe = inHomeForm(QFileInfo(QString("/proc/%1/exe").arg(pid)).symLinkTarget());
        if (!exe.isEmpty() && !isWineLoader(exe))
            return exe;

        // A Wine/Proton process: its command line starts with the Windows image path.
        QByteArray cmdline;
        if (!readProc(static_cast<uint32_t>(pid), "cmdline", cmdline))
            return exe;
        const QString first = QString::fromUtf8(cmdline.left(cmdline.indexOf('\0')));
        static const QRegularExpression windowsPath("^[A-Za-z]:\\\\");
        return windowsPath.match(first).hasMatch() ? inHomeForm(fromWinePath(static_cast<uint32_t>(pid), first)) : exe;
    }

    uint64_t processCreationTime(uint32_t pid) const override
    {
        const StatInfo stat = readStat(pid);
        return stat.ok ? stat.startTicks : 0;
    }

    // Clock ticks since boot, the unit of a process's start time.
    uint64_t now() const override
    {
        QByteArray raw;
        QFile f("/proc/uptime");
        if (!f.open(QIODevice::ReadOnly))
            return 0;
        raw = f.readAll();
        const double seconds = raw.split(' ').value(0).toDouble();
        return static_cast<uint64_t>(seconds * static_cast<double>(sysconf(_SC_CLK_TCK)));
    }

    uint32_t currentPid() const override
    {
        return static_cast<uint32_t>(getpid());
    }

    bool terminateProcess(uint32_t pid) override
    {
        return ::kill(static_cast<pid_t>(pid), SIGKILL) == 0;
    }

    bool startProcess(const QString& path, const QString& workingDir, qint64* pid) override
    {
        // Native programs and scripts run as they are; a Windows exe needs Wine.
        if (QFileInfo(path).suffix().compare("exe", Qt::CaseInsensitive) == 0)
            return QProcess::startDetached("wine", {path}, workingDir, pid);
        return QProcess::startDetached(path, {}, workingDir, pid);
    }

    bool openUrl(const QString& url) override
    {
        // In a container (distrobox, toolbox) the handler for steam:// lives on the host.
        const QString hostExec = QStandardPaths::findExecutable("distrobox-host-exec");
        if (QFile::exists("/run/.containerenv") && !hostExec.isEmpty())
            return QProcess::startDetached(hostExec, {"xdg-open", url});
        return PlatformBackend::openUrl(url);
    }

    // ----- Capture ---------------------------------------------------------
    QImage captureScreen(const Fit::Rect& rect) const override
    {
        // Fails under a Wayland compositor: XWayland's root window has no pixels (see captureWindow).
        return grab(x().root(), rect, {0, 0, 0, 0});
    }

    // The window's own contents (its client area, the part on the screen). This also works under
    // XWayland, where the screen itself cannot be read.
    QImage captureWindow(quintptr window) const override
    {
        Fit::Rect client;
        if (!clientRect(window, client))
            return {};
        const QImage image = grab(toWin(window), client, client);
        return image.isNull() ? PlatformBackend::captureWindow(window) : image;
    }
private:
    // The part of `rect` (screen coordinates) that is on the screen, read from `drawable`, whose
    // top-left corner is at `origin` on the screen. Null on failure.
    QImage grab(xcb_drawable_t drawable, const Fit::Rect& rect, const Fit::Rect& origin) const
    {
        if (!x().ok())
            return {};
        Reply<xcb_get_geometry_reply_t> g(xcb_get_geometry_reply(
            x().get(), xcb_get_geometry(x().get(), x().root()), nullptr));
        if (!g)
            return {};

        const int left = std::max(rect.left, 0);
        const int top = std::max(rect.top, 0);
        const int right = std::min(rect.right, static_cast<int>(g->width));
        const int bottom = std::min(rect.bottom, static_cast<int>(g->height));
        if (right <= left || bottom <= top)
            return {};

        const int w = right - left;
        const int h = bottom - top;
        Reply<xcb_get_image_reply_t> image(xcb_get_image_reply(
            x().get(),
            xcb_get_image(x().get(), XCB_IMAGE_FORMAT_Z_PIXMAP, drawable,
                          static_cast<int16_t>(left - origin.left), static_cast<int16_t>(top - origin.top),
                          static_cast<uint16_t>(w), static_cast<uint16_t>(h), ~0u),
            nullptr));
        if (!image || xcb_get_image_data_length(image.get()) < w * h * 4)
            return {};

        // 32 bits per pixel, blue first: QImage's RGB32 on a little-endian machine
        return QImage(xcb_get_image_data(image.get()), w, h, w * 4, QImage::Format_RGB32).copy();
    }

    void sendGeometry(xcb_window_t window, int x_, int y_, int w, int h) const
    {
        if (windowManagerSupports("_NET_MOVERESIZE_WINDOW")) {
            constexpr uint32_t kHasX = 1u << 8, kHasY = 1u << 9, kHasW = 1u << 10, kHasH = 1u << 11;
            x().request(window, "_NET_MOVERESIZE_WINDOW",
                        kStaticGravity | kHasX | kHasY | kHasW | kHasH | (kSourceApplication << 12),
                        static_cast<uint32_t>(x_), static_cast<uint32_t>(y_),
                        static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        } else {
            // A plain ConfigureWindow is turned into a request to the window manager (root coordinates).
            const uint32_t values[4] = {static_cast<uint32_t>(x_), static_cast<uint32_t>(y_),
                                        static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
            xcb_configure_window(x().get(), window,
                                 XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y
                                     | XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT, values);
            xcb_flush(x().get());
        }
        x().sync();
    }

    bool windowManagerSupports(const char* atomName) const
    {
        const xcb_atom_t wanted = x().atom(atomName);
        const auto supported = x().cardinals(x().root(), "_NET_SUPPORTED", XCB_ATOM_ATOM);
        return std::find(supported.begin(), supported.end(), wanted) != supported.end();
    }

    // The managed top-level windows: the window manager's list, or, for a manager that does not
    // keep one, whatever under the root looks like an application window.
    std::vector<uint32_t> clientWindows() const
    {
        auto ids = x().cardinals(x().root(), "_NET_CLIENT_LIST", XCB_ATOM_WINDOW);
        if (!ids.empty())
            return ids;

        const auto children = [](xcb_window_t parent) {
            std::vector<uint32_t> out;
            Reply<xcb_query_tree_reply_t> tree(xcb_query_tree_reply(
                x().get(), xcb_query_tree(x().get(), parent), nullptr));
            if (tree) {
                const xcb_window_t* list = xcb_query_tree_children(tree.get());
                out.assign(list, list + xcb_query_tree_children_length(tree.get()));
            }
            return out;
        };
        const auto looksLikeApp = [](xcb_window_t w) {
            if (!x().cardinals(w, "WM_STATE", x().atom("WM_STATE")).empty())
                return true;
            Reply<xcb_get_window_attributes_reply_t> a(xcb_get_window_attributes_reply(
                x().get(), xcb_get_window_attributes(x().get(), w), nullptr));
            return a && !a->override_redirect && a->map_state == XCB_MAP_STATE_VIEWABLE
                   && !x().cardinals(w, "_NET_WM_PID").empty();
        };

        for (const uint32_t top : children(x().root())) {
            if (looksLikeApp(top)) {
                ids.push_back(top);
                continue;
            }
            for (const uint32_t inner : children(top))   // reparented: frame -> client
                if (looksLikeApp(inner))
                    ids.push_back(inner);
        }
        return ids;
    }

    struct Extents { int left{0}, right{0}, top{0}, bottom{0}; };

    static Extents frameExtents(xcb_window_t w)
    {
        Extents e;
        const auto v = x().cardinals(w, "_NET_FRAME_EXTENTS");
        if (v.size() >= 4) {
            e.left = static_cast<int>(v[0]);
            e.right = static_cast<int>(v[1]);
            e.top = static_cast<int>(v[2]);
            e.bottom = static_cast<int>(v[3]);
        }
        return e;
    }

    static void activate(xcb_window_t w)
    {
        x().request(w, "_NET_ACTIVE_WINDOW", kSourcePager, XCB_CURRENT_TIME, 0);
    }

    // QScreen reports an origin in native pixels and a size in device-independent ones.
    static Fit::Rect physical(const QScreen* screen, const QRect& r)
    {
        const qreal dpr = screen->devicePixelRatio();
        return Fit::Rect::fromXYWH(r.x(), r.y(),
                                   static_cast<int>(std::lround(r.width() * dpr)),
                                   static_cast<int>(std::lround(r.height() * dpr)));
    }
};

} // namespace

PlatformBackend& nativePlatformBackend()
{
    static X11Backend backend;
    return backend;
}
