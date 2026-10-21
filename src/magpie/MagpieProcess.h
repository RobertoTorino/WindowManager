#pragma once

#include <QList>
#include <QString>

/**
 * Starting and stopping Magpie (Windows). Magpie runs once per user: a second start only shows the
 * first one, so the app checks whose Magpie is running before it starts its own.
 */
namespace Magpie {

struct RunningMagpie
{
    qint64  pid{0};
    QString path;   // its Magpie.exe (empty if Windows would not say)
};

// Every running Magpie.exe.
QList<RunningMagpie> runningMagpies();

// True when `m` is the Magpie in `dir`.
bool isFrom(const RunningMagpie& m, const QString& dir);

// Asks the running Magpie to quit (its WM_MAGPIE_QUIT message, broadcast as Magpie itself does it).
// Magpie saves its settings and exits.
bool postQuit();

// Starts the Magpie in `dir` in the tray (-t), detached: it keeps running after this app exits.
bool startInTray(const QString& dir);

// What a Magpie profile needs of a game window, read from the window itself.
struct WindowFacts
{
    qint64  pid{0};
    QString className;
    QString exePath;          // native, exactly as Windows reports it (Magpie compares this string)
    bool    visible{false};
    bool    maximized{false};
    bool    fullScreen{false};   // no frame and covering its whole monitor (borderless full screen)
    int     clientWidth{0};      // the drawn area, in pixels
    int     clientHeight{0};
    int     monitorWidth{0};     // the monitor it is on (mostly), in pixels
    int     monitorHeight{0};
    int     monitorHz{0};        // its refresh rate; 0: unknown
    // Magpie scales up: a bigger window is shrunk to fit its monitor, squeezed if the shape differs.
    bool largerThanMonitor() const
    {
        return monitorWidth > 0 && (clientWidth > monitorWidth || clientHeight > monitorHeight);
    }
};
WindowFacts windowFacts(quintptr hwnd);   // empty fields when the window is gone

bool processAlive(qint64 pid);

// What Magpie is scaling now, read from its scaling window (class Window_Magpie_967EB565-...): the
// properties Magpie sets for other programs. Physical pixels.
struct Scaling
{
    bool     active{false};   // a scaling window with a source exists
    quintptr source{0};       // the window it scales (Magpie.SrcHWND)
    int      srcLeft{0};      // the part of it Magpie captures (Magpie.Src*), screen coordinates
    int      srcTop{0};
    int      srcWidth{0};
    int      srcHeight{0};
    int      destLeft{0};     // where it draws that (Magpie.Dest*), screen coordinates
    int      destTop{0};
    int      destWidth{0};
    int      destHeight{0};
    bool     windowed{false}; // Magpie's windowed mode (else full screen on the source's monitor)
};
Scaling scaling();

} // namespace Magpie
