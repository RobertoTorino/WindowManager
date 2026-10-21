#pragma once

#include "stats/StatsModel.h"

#include <QString>

#include <vector>

/**
 * Pure logic behind the game info overlay and the status bar's scaler line: no OS calls, no widgets
 * (tst_picture). From what is known about the game's window, its monitor, Magpie and the downscaled
 * view, it works out which scaler shows the picture, from what size to what size, the scale and the
 * direction, and what is in the way (Magpie not scaling, a window bigger than the monitor).
 */
namespace Picture {

struct Size
{
    int w{0};
    int h{0};
    bool valid() const { return w > 0 && h > 0; }
    bool operator==(const Size&) const = default;
};

enum class Scaler { None, Magpie, DownscaledView };

// Read once a second by MainWindow.
struct Facts
{
    bool    gameRunning{false};
    bool    windowFound{false};
    Size    window;               // the game's drawn area (client), physical pixels
    Size    monitor;              // the monitor it is on
    int     monitorIndex{0};      // 1-based; 0: unknown
    int     monitorHz{0};
    bool    maximized{false};
    bool    fullScreen{false};    // borderless and covering its monitor
    bool    gameInFront{false};
    QString foreground;           // the program in front when it is not the game ("JConfigTK7R2.exe")

    QString magpieMode;           // the game's Magpie effect; empty: Magpie is off for this game
    bool    magpieRunning{false}; // the app's Magpie runs
    bool    magpieStarting{false};// the app is starting Magpie for this game, or started it a moment ago
    bool    magpieScaling{false}; // Magpie's scaling window shows this game
    Size    magpieSource;         // what Magpie captures
    Size    magpieDest;           // what it draws

    bool    viewRunning{false};   // the downscaled view
    Size    viewRender;
    Size    viewShown;

    int     renderScale{0};       // r.ScreenPercentage in the game's files; 0: not set / not known
    Size    fileResolution;       // the resolution in the game's files; invalid: not known
    bool    filesChanged{false};  // this app changed the game's files (Game config tab)
};

struct Result
{
    Scaler  scaler{Scaler::None};
    QString scalerName;          // "Magpie FSR", "Downscaled view", "None"
    Size    from;                // what is scaled ...
    Size    to;                  // ... to what (both invalid: nothing scales)
    int     percent{100};
    bool    paused{false};       // Magpie waits or pauses: another window is in front
    bool    starting{false};     // Magpie is starting
    QString picture;             // "1280 × 720 → 1920 × 1080 · 150 % · upscaling"
    QString status;              // for the status bar: "Magpie FSR: 1280 × 720 → 1920 × 1080 (150 %, upscaling)"
    std::vector<QString> problems;   // what is in the way, with what to do
};

// to / from as a percentage: the smaller of the two ratios (a letterboxed picture), rounded.
int scalePercent(Size from, Size to);
QString direction(int percent);   // "upscaling", "downscaling", "1:1"
QString sizeText(Size s);         // "1280 × 720"

Result evaluate(const Facts& f);

// One row of the overlay.
struct Line
{
    QString label;   // "Scaler"; empty for a warning
    QString value;
    bool    warn{false};
    bool operator==(const Line&) const = default;
};

// Everything the overlay shows: the picture (scaler, sizes, render scale, window, monitor), the game's
// FPS / CPU / GPU / memory / power, then the problems and the stats warnings.
std::vector<Line> overlayLines(const Facts& f, const Result& r, const Stats::GameStats& stats);

} // namespace Picture
