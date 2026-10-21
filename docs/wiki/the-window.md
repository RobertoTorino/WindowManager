# The main window

![The main window](../assets/screenshots/main-window.png){ .wm-shot loading=lazy }

```
+----------------------------------------------------------------------------+
| File  View  Help                                                           |
+----------------------------------------------------------------------------+
| Game [ saved games dropdown ......... ] [Add game...] [Start][Restart][Kill]|   Game bar
| [Scan][Monitor 1][Monitor 2][Stability][Focus]     [Destroy][Reset All][Shutdown] |   Main actions
| +------------------------------------------------------------------------+ |
| | HWND | Title            | Class | Status | PID | X | Y | W | H          | |   Window table
| |  ...                                                                   | |   (grows with the window)
| +------------------------------------------------------------------------+ |
| [ Window | Position & Size | Presets | Downscale | Display | Game config | GPU & Power | Capture | HelixSR | Magpie | Reports ] | Tabs
| +------------------------------------------------------------------------+ |
| |  controls for the selected tab                                         | |
| +------------------------------------------------------------------------+ |
| status message    Magpie FSR: 1280 × 720 → 1920 × 1080 (150 %)  Focus  exe |   Status bar
+----------------------------------------------------------------------------+
```

## Game bar

| Control | What it does |
|---|---|
| Dropdown | Your saved games. Type to filter. Picking one makes it the target. |
| **Add game...** | Register an `.exe`/`.bat`/`.cmd` and name it. Steam games are added by themselves. |
| **Start** | Launch the selected game. If a game is already running, asks whether to close it first (a game is never started twice). |
| **Restart** | Terminate it, then launch it again. |
| **Kill** | Terminate it. |

More in [Games and launching](games-and-launching.md).

## Main actions

| Button | What it does |
|---|---|
| **Scan** | Read the window list again and pick the best window (the list also follows the game by itself). |
| **Monitor 1 / Monitor 2** | Move the selected window to that monitor (the current one is green). |
| **Stability** | While on, if the game moves or resizes its window away from where you put it, it is put back. |
| **Focus** | Bring the game window to the front. |
| **Destroy** | Ask the selected window to close (`WM_CLOSE`). |
| **Reset All** | Restore normal window styles and forget the saved position for this game. |
| **Shutdown** | Terminate this session's game and its loader processes (asks first). |

## Window table

Lists title, class, visibility, PID and rectangle for each window of the target. The **active window**, the
one every button and tab acts on, is the green, bold row with Status *Active*. See
[Window controls](window-controls.md#window-table).

## Tabs

| Tab | What it is for |
|---|---|
| **Window** | Show, hide, minimise; Windowed, True Borderless FS, Fit Screen, Fit to monitor; extended styles. [Window controls](window-controls.md#window) |
| **Position & Size** | Move, resize, exact size, overscan, saved position. [Window controls](window-controls.md#position-size) |
| **Presets** | One-click sizes from 1920x1080 to 7680x4320. [Window controls](window-controls.md#presets) |
| **Downscale** | Render larger than the monitor and show it scaled down. [Downscaled view](downscaled-view.md) |
| **Display** | The PC's display settings, and the game's own while it runs. [Display settings](display-settings.md) |
| **Game config** | Render scale, resolution and JConfig window in the game's own config files. [Game config](game-config.md) |
| **GPU & Power** | Graphics cards and drivers, the card the game uses, power plan and mode. [GPU and power](gpu-and-power.md) |
| **Capture** | Screenshots of the active window as PNG, one or a burst. [Window controls](window-controls.md#capture) |
| **HelixSR** | Set up HelixSR and put it into FSR 3.1 games. [HelixSR](helixsr.md) |

## Menus

| Menu | Items |
|---|---|
| **File** | Add game..., Find Steam games, Open config file, View logs..., Exit |
| **View** | Always on top, Window opacity, Confirm before terminating a game, Check window fit after launching a game, Fit window to monitor..., Game info overlay (show, corner, in screenshots), Keep this app off the game's monitor, Release the mouse, Switch focus: game / this app, Focus switch hotkey..., Terminate the game (press twice), Terminate hotkey..., Minimize to floating bar, Switch to floating bar, Stop downscaled view, Move this app to the other monitor |
| **Help** | Help topics (F1), Keyboard shortcuts, Logs and troubleshooting, Online wiki, About |

## Status bar

The left side shows the result of your last action. On the right, while a game runs: what scales its picture
(`Magpie FSR: 1280 × 720 → 1920 × 1080 (150 %, upscaling)`, or `No scaler: window 1280 × 720, not scaled
(1:1)`; orange with the reason in the tooltip when something is in the way, see
[Game info overlay](game-stats.md)), who has the keyboard (*Focus: game / app*), and the target program's
file name (the full path in its tooltip). Every result is also written to the log; see
[Logs and troubleshooting](logs-and-troubleshooting.md).
