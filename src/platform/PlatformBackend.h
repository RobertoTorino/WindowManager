#pragma once

#include "../window/FitCalculator.h"

#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <optional>
#include <vector>

class QImage;

/**
 * Everything WindowManager needs from the operating system, as plain operations.
 *
 * WindowManager, MonitorHelper and the controller hold the policy (which window is
 * the game, how to fit it, what to kill) and call this interface for the mechanism
 * (list windows, move one, read a process). There is one implementation per platform
 * under src/platform/<name>/; platform() returns it.
 *
 * Window handles are opaque quintptr values (an HWND on Windows). All rectangles are
 * in screen coordinates, outer edge to outer edge.
 */
class PlatformBackend
{
public:
    virtual ~PlatformBackend() = default;

    struct TopLevelWindow
    {
        quintptr  handle{0};
        qint64    pid{0};
        Fit::Rect rect;
    };

    struct Monitor
    {
        Fit::Rect full;   // the whole monitor
        Fit::Rect work;   // without the task bar / panels
    };

    struct ProcessEntry
    {
        uint32_t pid{0};
        uint32_t ppid{0};
        QString  exeName;   // file name only
    };

    // Snapshot of the flags the UI shows as toggles.
    struct WindowState
    {
        bool valid{false};          // the handle still names a window
        bool visible{false};
        bool minimized{false};
        bool maximized{false};
        bool hasCaption{false};     // title bar present ("windowed")
        bool hasFrame{false};       // title bar or sizing border present
        bool topmost{false};
        bool toolWindow{false};
        bool layered{false};
        bool noActivate{false};
    };

    enum class ShowState { Hide, Show, Minimize, Maximize, Restore };
    enum class WindowFlag { Topmost, ToolWindow, Layered, NoActivate };

    // ----- Windows ---------------------------------------------------------
    // Top-level windows that have a rectangle and an owning process.
    virtual std::vector<TopLevelWindow> topLevelWindows() const = 0;
    virtual bool    isValid(quintptr window) const = 0;
    virtual WindowState windowState(quintptr window) const = 0;
    virtual QString windowTitle(quintptr window) const = 0;
    virtual QString windowClass(quintptr window) const = 0;
    virtual qint64  windowPid(quintptr window) const = 0;

    virtual bool windowRect(quintptr window, Fit::Rect& out) const = 0;
    // The picture area (no title bar or borders).
    virtual bool clientRect(quintptr window, Fit::Rect& out) const = 0;
    // What the user sees as the window edge; differs from windowRect where the OS
    // adds invisible resize borders. Empty if the platform has no such notion.
    virtual std::optional<Fit::Rect> visibleFrame(quintptr window) const = 0;

    // Move/resize without activating; also applies pending frame changes.
    virtual bool setBounds(quintptr window, const Fit::Rect& rect) = 0;
    virtual void stripFrame(quintptr window) = 0;     // remove title bar and borders
    virtual void restoreFrame(quintptr window) = 0;   // put them back
    // Undo every override this app can make (frame, tool/layered/no-activate, topmost) and restore.
    virtual void resetWindow(quintptr window) = 0;

    virtual void show(quintptr window, ShowState state) = 0;
    virtual void bringToTop(quintptr window) = 0;
    virtual void focus(quintptr window) = 0;          // restore + steal the foreground
    virtual void requestClose(quintptr window) = 0;   // polite close, the game may refuse
    // Not every system has every flag (e.g. no "layered" windows on X11).
    virtual bool supportsFlag(WindowFlag flag) const = 0;
    // Flips the flag and returns whether it is now set.
    virtual bool toggleFlag(quintptr window, WindowFlag flag) = 0;

    // ----- Monitors --------------------------------------------------------
    virtual std::vector<Monitor> monitors() const = 0;                     // in index order
    virtual std::optional<Monitor> nearestMonitor(quintptr window) const = 0;

    // ----- Processes -------------------------------------------------------
    virtual std::vector<ProcessEntry> processes() const = 0;
    virtual QString  processImagePath(qint64 pid) const = 0;
    // Creation time in the same units as now(), 0 if unknown.
    virtual uint64_t processCreationTime(uint32_t pid) const = 0;
    virtual uint64_t now() const = 0;
    virtual uint32_t currentPid() const = 0;
    virtual bool     terminateProcess(uint32_t pid) = 0;
    // Starts the game (a script is run through its interpreter); pid is the new process.
    virtual bool     startProcess(const QString& path, const QString& workingDir, qint64* pid) = 0;

    // ----- Capture ---------------------------------------------------------
    // Pixels currently on screen inside rect (null image on failure).
    virtual QImage captureScreen(const Fit::Rect& rect) const = 0;
};

// The backend for this platform (or the one a test installed).
PlatformBackend& platform();

// Tests only: install a fake, or pass nullptr to go back to the real one.
void setPlatformBackendForTesting(PlatformBackend* backend);

// Defined once per platform under src/platform/<name>/.
PlatformBackend& nativePlatformBackend();
