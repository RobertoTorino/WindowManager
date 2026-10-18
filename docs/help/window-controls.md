# Window controls

All of these act on the **selected window** in the table.

## Window table

Shows title, class, visibility, process id and rectangle of each window of the target.
Hidden windows are dimmed. If it is empty it says why: no game selected, or no windows
found yet (start the game, then press **Scan**).

## Action row

| Button | What it does |
|---|---|
| **Scan** | Refresh the table. |
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
- **Extended styles:** Topmost, Tool Window, Layered, No Activate. These are toggles;
  green means on.

### Position & Size
- **Move:** the arrow pad moves the window by *Step* pixels.
- **Resize:** Width/Height +/- grows or shrinks around the window centre by *Step*.
- **Exact size:** type width and height, then **Set size**.
- **Overscan:** shrinks the window by a number of pixels on each side of monitor 1's work
  area, horizontally, vertically or both.
- **Saved profile:** **Save position** stores the current rectangle for this game;
  **Force saved settings** applies it again.

### Presets
One click sets a size and keeps the position: common sizes from 1920x1080 up to 7680x4320.
The large ones are for testing and need a lot of video memory.

### Downscale
Renders the game larger than the monitor and shows it scaled down. See
[Downscaled view](downscaled-view.md).

### Capture
Screenshots the window as PNG. Choose how many shots for a burst (80 ms apart).
**Open folder** shows `snapshots/<game>`.

## Stability

Some games move or resize their window after you placed it. With **Stability** on, the
window is returned to its position. It is switched off automatically when the downscaled
view starts, because it would fight that view.
