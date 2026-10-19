[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

# WindowManager

A Windows tool for taming the windows of games and emulators: force true borderless
fullscreen, set exact sizes, nudge and resize, move a game between monitors, keep a
window from drifting, and grab screenshots. Built with C++20 and Qt 6.

- [Using WindowManager](#using-windowmanager)
- [Fit to monitor (automatic)](#fit-to-monitor-automatic)
- [Downscaled view (sharper picture)](#downscaled-view-sharper-picture)
- [HelixSR (FSR 3.1 upscaler for AMD GPUs)](#helixsr-fsr-31-upscaler-for-amd-gpus)
- [The window at a glance](#the-window-at-a-glance)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Files it creates](#files-it-creates)
- [Safety notes](#safety-notes)
- [Help pages](docs/help/getting-started.md) (also shown in the app under Help)
- [Building and testing](BUILD.md)
- [Changelog](CHANGELOG.md)
- [Linux port (work in progress)](docs/LINUX_PORT.md)
- [Code layout](docs/ARCHITECTURE.md)

---

## Using WindowManager

1. **Add a game.** Click **Add game...** (or *File > Add game...*) and pick an `.exe`,
   `.bat` or `.cmd`. Give it a short name; it is saved in the dropdown for next time.
2. **Start it.** Click **Start**. For batch files the app watches for the real game
   process that the batch file spawns and switches to it automatically.
   Once the game's window has settled, the **Fit window to monitor** popup opens (see
   below). Already running? Just pick the game and press **Scan**.
3. **Select the game window.** Every top-level window owned by the game is listed in
   the table. The largest visible one is selected automatically; click another row to
   change which window the buttons act on.
4. **Adjust it** with the buttons and tabs below. Buttons that turn **green** show a
   state that is currently on (for example *Topmost*, or *Monitor 2* when the game is
   on monitor 2).
5. **Save the result.** On the *Position & Size* tab, **Save position** remembers the
   window rectangle for that game, and **Force saved settings** re-applies it later.

---

## Fit to monitor (automatic)

The manual workflow - nudge the position, grow or shrink the width and height until the
picture exactly fills the screen - is calculated for you.

**What happens when you press Start**

1. The app waits for the game's real window. Helper windows a game owns (input-method
   and console windows, hidden or tiny windows) are ignored.
2. It waits until that window has kept the same position and size for about two
   seconds. Games and emulators often rebuild their window while their graphics engine
   starts, so measuring earlier would measure the wrong thing.
3. The **Fit window to monitor** popup opens (it can also be opened any time with
   **Fit to monitor...** on the *Window* tab, *View > Fit window to monitor...* or Ctrl+F).
4. Press **Apply correction** to acknowledge it, or **Cancel** to leave the window alone.

**What the popup shows**

| Section | Meaning |
|---|---|
| Target monitor / Fit to | Which monitor, and whether to fill the *full monitor* (true borderless) or the *work area* (taskbar stays visible). Defaults to the monitor the game is on. |
| Remove title bar and borders first | Games with a normal frame can have it removed first, so the whole window becomes the picture. |
| Measured | The window rectangle, the picture (client area) rectangle, and the target, each as X / Y / width / height. Also the frame around the picture (title bar and borders) and any invisible resize border Windows adds. |
| Correction | The exact change to apply: **Position** X and Y, **Size** width and height - the same amounts you would otherwise reach with the move and width/height buttons. |

Changing the monitor or the mode recalculates the correction instantly.

**How the correction is calculated.** The picture is the *client area*; the title bar,
borders and invisible resize border sit between it and the window rectangle. To put the
picture exactly on the target, the window rectangle must be the target grown by that
frame. The correction is the difference between that rectangle and the current one.
When the frame is removed first, the frame is zero and the window rectangle simply
becomes the target.

**After you apply it** the app re-measures. Some games nudge their window right after it
is moved, so it corrects up to three times and then reports honestly - either
"Picture fits monitor N exactly (WxH at X,Y)" or how far off it still is. When the
result is borderless, the final position is also saved, so **Force saved settings**
can restore it later.

Turn the automatic popup off with *Don't open this automatically...* in the popup, or
under *View > Check window fit after launching a game*. If no window appears within two
minutes, the check is cancelled; use **Fit to monitor...** later.

**Good to know**
- Keeping the title bar (unticking *Remove title bar and borders*) puts the title bar
  outside the monitor. With another monitor next to it, part of the window lands there and
  Windows may rescale it, so the fit can drift. The popup warns about this.
  Removing the frame is recommended and fits exactly, including on a second monitor.
- A maximized window is restored first, because Windows ignores positions for it.
- Fitting only changes the window; it does not change the game's own render resolution.

---

## Downscaled view (sharper picture)

Making the window bigger than the monitor makes parts of the picture fall off the
screen. The downscaled view gets the extra detail *without* that: the game window is
made large (for example 2x the monitor, so the game renders 4 times as many pixels) and
kept out of sight, while a full-screen overlay on the monitor shows a live, scaled-down
copy of it. Everything fits on the monitor and looks smoother, like the "internal
resolution" setting of an emulator or the GPU's DSR/VSR.

**Using it** (*Downscale* tab, with the game window selected)

1. **Show on** - which monitor shows the picture (default: the one the game is on).
2. **Sharpness** - 1.5x, 2x, 3x or 4x. The line below tells you exactly what will happen,
   for example *"The game will render 3160 x 2160, shown as 1580 x 1080 on monitor 1."*
3. **Picture shape** - the game's current shape (default), the monitor's shape, or
   4:3 / 16:9 / 16:10. A picture that does not match the monitor gets black bars; it is
   never stretched.
4. **Forward the mouse to the game** (on by default) - see below.
5. **Start downscaled view.** To go back, press **Stop**, use **Ctrl+Alt+D** (works from
   anywhere, because the overlay covers the screen), *View > Stop downscaled view*, close
   the game, or close this app. The game window is restored exactly as it was.

**What to expect**
- It only adds real detail if the game *re-creates its picture at the window's size*
  (RPCS3 and most GPU-rendered emulators do). If an emulator renders at a fixed internal
  size and merely stretches it into the window, the view looks the same as before. A quick
  test: after starting, the game's own picture should look crisper than at normal size.
- **Keyboard and gamepad** keep going to the game (it stays the focused window).
- **Mouse.** With *Forward the mouse to the game* ticked, the position and clicks on the
  picture are mapped back to the game's own coordinates and sent to it: movement, left /
  right / middle / side buttons, double-click and the wheel. A click at a point on the
  picture lands on the same point of the game's picture, however much it is scaled down.
  The black bars are ignored, and dragging with a button held keeps tracking even if
  you leave the picture. This works for games that read the mouse from their window
  messages (Qt, SDL and GLFW based programs, which should include RPCS3's basic mouse
  handler). Games that read the mouse through raw input, DirectInput or the global cursor
  position cannot see forwarded input; for those, untick the option.
- While it runs, the game window loses its title bar and borders and is parked behind
  the overlay (or just beyond the right edge of the desktop if it would otherwise show on
  another monitor). This app's own window is kept above the overlay, so you can still use it.
- The Stability option is switched off when the view starts, because it would pull the
  window back.
- Costs GPU power and video memory: 4x on a 1080p monitor means rendering at 4K or more.
  Render sizes above 16384 pixels are refused.
- Windowed and borderless games only; exclusive fullscreen cannot be mirrored. If the
  game runs as administrator, run this app as administrator too, or Windows refuses to
  mirror it.
- Tested with a Direct3D 11 test program (the whole surface, including the parts far
  beyond the monitor edge, is mirrored and updates live). Software-drawn (GDI-only)
  windows are only painted inside the desktop, so only that part would show.

---

## HelixSR (FSR 3.1 upscaler for AMD GPUs)

[HelixSR](https://github.com/lonewolf0622/HelixSR) is an unofficial free upscaler that replaces a
game's AMD FSR 3.1 DLL with one that runs NVIDIA's DLSS network as compute shaders, so it works
on AMD GPUs. The **HelixSR** tab automates its setup:

0. **Check this PC** (also run automatically before a set-up) tests the graphics card, shader model 6.6,
   16-bit math, free disk space and the tools the setup needs. An AMD RDNA 1+ card is supported; other
   cards are "experimental" (you are warned); a PC that cannot run it is stopped before the build.
1. **Set up HelixSR** downloads the newest release (checked against its published SHA-256), lets
   HelixSR's own setup build the network on this PC (about 5 to 6 minutes, with a progress bar and
   timer), then deletes the downloads, the DLSS DLL and the build tools.
2. The app checks for a newer release at start-up (when HelixSR is set up) and on **Check for
   updates**; the tab shows *HelixSR (update)* when there is one.
3. Select a game in the game bar and press **Check selected game**: games with an
   `amd_fidelityfx_dx12.dll` / `amd_fidelityfx_upscaler_dx12.dll` join the HelixSR list.
4. **Run with HelixSR** installs it into the game (the game's own DLL is kept as
   `*.original.dll`) and starts the game; choose *AMD FSR* in the game. **Remove** restores it.

With HelixSR installed in a game, the tab also edits its `helixsr.ini` (sharpening, strength, wave size). After a
build the app checks HelixSR's installer on a throw-away folder; if a new HelixSR changed it, installing is switched
off with a message instead of failing in front of a game.

Only the FSR 3.1 drop-in is supported (not HelixSR Scaler). The built DLL contains NVIDIA's network:
keep it on your PC. Details: [HelixSR help page](docs/help/helixsr.md).

---

## The window at a glance

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
| [ Window | Position & Size | Presets | Downscale | Capture | HelixSR ]     |   Tabs
| +------------------------------------------------------------------------+ |
| |  controls for the selected tab                                         | |
| +------------------------------------------------------------------------+ |
| status message                                               target exe    |   Status bar
+----------------------------------------------------------------------------+
```

### Game bar
| Control | What it does |
|---|---|
| Dropdown | Your saved games. Type to filter. Picking one makes it the target. |
| **Add game...** | Register an `.exe`/`.bat`/`.cmd` and name it. |
| **Start** | Launch the selected game. If a game is already running, asks whether to close it first (a game is never started twice). |
| **Restart** | Terminate it, then launch it again. |
| **Kill** | Terminate it. |

### Main actions
| Button | What it does |
|---|---|
| **Scan** | Refresh the table of windows for the target. |
| **Monitor 1 / Monitor 2** | Move the selected window to that monitor (the current one is green). |
| **Stability** | While on, if the game moves or resizes its window away from where you put it, it is put back. |
| **Focus** | Bring the game window to the front. |
| **Destroy** | Ask the selected window to close (`WM_CLOSE`). |
| **Reset All** | Restore normal window styles and forget the saved position for this game. |
| **Shutdown** | Terminate this session's game and its loader processes (asks first). |

### Window table
Lists title, class, visibility, PID and rectangle for each window of the target.
Hidden windows are dimmed. When it is empty it tells you why: no game selected, or no
windows found yet (start the game, then press **Scan**).

### Tabs
**Window**
- *Visibility:* Show, Hide, Minimize, Maximize, Restore.
- *Window mode:* **Windowed** (bordered), **True Borderless FS** (no borders, covers the
  whole monitor), **Fit Screen** (fills the work area, leaving the taskbar visible),
  **Fit to monitor...** (measures the window and opens the calculated correction; see
  [Fit to monitor](#fit-to-monitor-automatic)).
- *Extended styles (toggle):* Topmost, Tool Window, Layered, No Activate.

**Position & Size**
- *Move:* arrow pad moves the window by **Step** pixels.
- *Resize:* Width/Height +/- grows or shrinks around the window's centre by **Step**.
- *Exact size:* type a width and height, then **Set size**.
- *Overscan:* shrinks the window by the given pixels on each side of monitor 1's work
  area, horizontally, vertically or both.
- *Saved profile:* **Save position** / **Force saved settings**.

**Presets** - one click sets a size (position is kept): 1920x1080, 1920x1200,
1920x1440, 2048x1152, 2048x1536, 2560x1440, 2560x1600, 2880x1800, 3840x2160, and the
high-resolution group 4096x2160, 5120x2880, 6016x3384, 7680x4320.

**Downscale** - renders the game larger than the monitor and shows it scaled down so it
looks sharper without anything falling off the screen; see
[Downscaled view](#downscaled-view-sharper-picture).

**HelixSR** - set up and update HelixSR, and install it into / remove it from your FSR 3.1 games;
see [HelixSR](#helixsr-fsr-31-upscaler-for-amd-gpus).

**Capture** - screenshot the active window as PNG; choose how many shots for a burst
(80 ms apart). **Open folder** shows the game's snapshot folder.

### Menus
| Menu | Items |
|---|---|
| **File** | Add game..., Open config file, View logs..., Exit |
| **View** | Always on top, Confirm before terminating a game, Check window fit after launching a game, Fit window to monitor..., Stop downscaled view, Move this app to the other monitor |
| **Help** | Help topics (F1), Keyboard shortcuts, Logs and troubleshooting, About |

---

## Keyboard shortcuts

| Keys | Action |
|---|---|
| Ctrl+S | Toggle the game between monitor 1 and monitor 2 |
| Ctrl+F | Fit window to monitor... (measure and propose the correction) |
| Ctrl+Alt+D | Stop the downscaled view (global: works even when this app is not focused) |
| Ctrl+M | Move this app's window to the other monitor |
| Ctrl+O | Add game... |
| F1 | Help |
| Ctrl+L | View logs... |
| Ctrl+Q | Exit |

---

## Files it creates

All next to `WindowManager.exe`:

| File | Contents |
|---|---|
| `windowmanager.json` | Saved games, the last target and per-game window profiles. Written atomically. |
| `windowmanager.ini` | App window size/position, Always on top, the terminate-confirmation preference and the automatic fit check on/off. |
| `windowmanager.log`, `windowmanager_old.log` | Rolling application log (the old one is the previous file after rotation). |
| `logs/<game>.log`, `logs/<game>_old.log` | Rolling log per game. View all logs in the app with **File > View logs...** (Ctrl+L). |
| `snapshots/<game id>/` | Screenshots from the Capture tab. |
| `helixsr/` | The built HelixSR (`HelixSR-<version>/`), the install helper script, and downloads while a set-up runs. HelixSR's Python lives in `%LOCALAPPDATA%\HelixSR`. |

---

## Safety notes

- **Shutdown, Kill and Restart ask for confirmation** before terminating anything.
  Tick *Don't ask again* to stop the prompt; turn it back on under
  *View > Confirm before terminating a game*.
- Termination is scoped to the session: the game you launched (and everything it
  started), the selected target by full path, and loader processes (for example
  TeknoParrot or Dolphin) that were started after your launch. A loader or emulator
  you started yourself beforehand is left alone. Shell and system processes
  (`explorer.exe`, `cmd.exe`, browsers, ...) are never terminated.
- The tool changes window styles and positions of other programs. **Reset All** and the
  **Windowed** button undo borderless changes.

## License

GPL-3.0, see [LICENSE](LICENSE).
