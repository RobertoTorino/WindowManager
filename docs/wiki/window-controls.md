# Window controls

All of these act on the **selected window** in the table.

## Window table

Shows title, class, visibility, process id and rectangle of each window of the target.
Hidden windows are dimmed. If it is empty it says why: no game selected, or no windows
found yet (start the game, then press **Scan**).

The **active window**, the one every button and tab acts on, is the green, bold row with Status
*Active*. Click another row to make that one active.

The table follows the game by itself: when the game opens, closes, moves or resizes a window (at
once on Windows, otherwise within 3 seconds). A window you picked stays active as long as it exists;
when it closes, the best remaining window (visible, largest) becomes active. **Scan** reads the list
again on demand and picks the best window.

## Action row

| Button | What it does |
|---|---|
| **Scan** | Read the window list again and pick the best window (the list also follows the game by itself). |
| **Monitor 1 / Monitor 2** | Move the window to that monitor. The current one is green. Ctrl+S toggles. |
| **Stability** | While on, a window that the game moves or resizes is put back where you set it. |
| **Focus** | Bring the game window to the front. |
| **Destroy** | Ask the window to close (`WM_CLOSE`). The game may refuse or ask to save. |
| **Reset All** | Restore normal window styles and forget the saved position for this game. |
| **Shutdown** | Terminate this session's game and loader processes (asks first). |

## Tabs

### Window
- **Visibility:** Show, Hide, Minimize, Maximize, Restore.
- **Window mode:** *Windowed* (with borders), *True Borderless FS* (no borders, covers the
  whole monitor), *Fit Screen* (fills the work area, taskbar stays visible) and
  *Fit to monitor...* (see [Fit to monitor](fit-to-monitor.md)).
  On Linux, *True Borderless FS* puts the window in the desktop's own full-screen mode, because KDE
  will not resize a Proton game's fixed-size window or remove its title bar. *Windowed* takes it out again.
- **Extended styles:** Topmost, Tool Window, Layered, No Activate. These are toggles;
  green means on. On Linux only Topmost and Tool Window work (see [Linux](linux.md)).

### Position & Size
- **Move:** the arrow pad moves the window by *Step* pixels.
- **Resize:** Width/Height +/- grows or shrinks around the window centre by *Step*.
- **Exact size:** type width and height, then **Set size**.
- **Overscan:** shrinks the window by a number of pixels on each side of monitor 1's work
  area, horizontally, vertically or both.
- **Saved profile:** **Save position** stores the current rectangle for this game;
  **Force saved settings** applies it again.

### Presets
One click sets a size and keeps the position: 1920x1080, 1920x1200, 1920x1440, 2048x1152, 2048x1536,
2560x1440, 2560x1600, 2880x1800, 3840x2160, and the high-resolution group 4096x2160, 5120x2880, 6016x3384,
7680x4320. The large ones are for testing and need a lot of video memory. The names are in
[Window terms](window-terms.md#predefined-sizes).

### Downscale
Renders the game larger than the monitor and shows it scaled down. See
[Downscaled view](downscaled-view.md).

### Display, Game config, GPU & Power, HelixSR
See [Display settings](display-settings.md), [Game config](game-config.md), [GPU and power](gpu-and-power.md)
and [HelixSR](helixsr.md).

### Capture
Screenshots the window as PNG. Choose how many shots for a burst (80 ms apart).
**Open folder** shows `snapshots/<game id>`. On Linux it captures the game window itself.

## Stability

Some games move or resize their window after you placed it. With **Stability** on, the
window is returned to its position. It is switched off automatically when the downscaled
view starts, because it would fight that view.
