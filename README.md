[![CI Build and Release](https://github.com/RobertoTorino/WindowManager/actions/workflows/release.yml/badge.svg?branch=main)](https://github.com/RobertoTorino/WindowManager/actions/workflows/release.yml)
[![Status: Beta](https://img.shields.io/badge/status-beta-orange.svg)](CHANGELOG.md)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

# WindowManager

A Windows tool for taming the windows of games and emulators: force true borderless
fullscreen, set exact sizes, nudge and resize, move a game between monitors, keep a
window from drifting, and grab screenshots. It also changes the PC's display settings
(resolution, HDR, scale, brightness, ...), per game if you like. Built with C++20 and Qt 6.

WindowManager is in **beta**. It also runs on Linux for Steam games under Proton (Bazzite, KDE Plasma),
with fewer features and no download yet: see [Linux](docs/help/linux.md).

- [Using WindowManager](#using-windowmanager)
- [Fit to monitor (automatic)](#fit-to-monitor-automatic)
- [Downscaled view (sharper picture)](#downscaled-view-sharper-picture)
- [Display settings](#display-settings)
- [HelixSR (FSR 3.1 upscaler for AMD GPUs)](#helixsr-fsr-31-upscaler-for-amd-gpus)
- [The window at a glance](#the-window-at-a-glance)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Files it creates](#files-it-creates)
- [Safety notes](#safety-notes)
- [Help pages](docs/help/getting-started.md) (also shown in the app under Help)
- [Building and testing](BUILD.md)
- [Changelog](CHANGELOG.md)
- [Linux (beta): what works, how to build it](docs/help/linux.md)
- [Linux port notes (development)](docs/LINUX_PORT.md)
- [Code layout](docs/ARCHITECTURE.md)

---

## Using WindowManager

1. **Add a game.** Games installed through Steam are added by themselves at start-up (and with
   *File > Find Steam games*). For anything else click **Add game...** (or *File > Add game...*)
   and pick an `.exe`, `.bat` or `.cmd`. Give it a short name; it is saved in the dropdown for next time.
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

## Display settings

The **Display** tab has the settings of *Settings > System > Display* for each monitor:
resolution, refresh rate and orientation (**Apply**, then confirm within 15 seconds or the old
mode comes back), scale, HDR, brightness (a laptop panel through Windows, an external monitor
through DDC/CI), colour profile, and how several displays are used (like Win+P, also confirmed).
**Night light...** opens its Windows Settings page, as Windows has no supported way to switch it.

**While the game runs:** tick *Use these settings while <game> runs* and choose a resolution,
refresh rate, HDR, scale and brightness for the selected game (*Keep* leaves one alone). They are
applied when you press **Start** / **Restart**, before the game starts, and undone when the game
closes, when it did not start within 3 minutes, on **Restore now**, or when WindowManager closes.
After a crash the desktop's settings are restored at the next start. The resolution is changed
only for the game and never stored as Windows' setting.

The scale goes through the same undocumented part of Windows that Settings uses; the rest uses
documented Windows functions. Windows only for now. Details:
[Display settings help page](docs/help/display-settings.md).

## Game config (Unreal Engine 4 games, Tekken 7)

The **Game config** tab writes settings into the selected game's own config files on **Start** /
**Restart**: the **render scale** (Unreal's `r.ScreenPercentage`: above 100 % the engine draws
larger and scales down, a sharper picture in the same window; below 100 % it is faster), the
**resolution** (engine settings, user settings, `r.setres`), and for Tekken 7's arcade builds
**JConfig's window** (`monitor_misc.bin`), so the game stops putting its window back to 1280 x 720.
For Unreal Engine 4 games with loose config files (`<Project>/Config/DefaultEngine.ini` next to
`<Project>/Binaries/Win64`). Only those keys change; the first change keeps a copy of each file
(`*.wm-original`), and **Restore originals** puts the game's own values back. Details:
[Game config help page](docs/help/game-config.md).

**Focus switch and terminate key.** While a game runs, the stats bar shows **Focus: game / app /
other**. **F1** switches between the game and this app; **F2 pressed twice within 2 seconds**
terminates the game like *Shutdown*. Both work from the game and are registered only while it runs.
A game that grabs the focus straight back (Tekken 7 with JConfig) puts the app in **mouse only**: its
buttons work with the mouse without taking the focus from the game. With one monitor, **View > Window
opacity** makes the app see-through (90 % to 40 %, by default only while a game runs), so the game stays
visible behind it. Change the keys under
*View > Focus switch hotkey...* / *Terminate hotkey...*.

---

## Game stats bar

A second bar above the status bar shows the running game's **FPS** (with the 1 % low), **CPU**,
**GPU** use and the graphics card it renders on, **RAM**, **VRAM**, and whether the PC is
plugged in or on battery. An orange warning appears when something is known to slow games down:
on battery, battery saver, the *Best power efficiency* power mode, the game on the integrated GPU
while there is a faster one, or the whole CPU busy. Hover over it for what to do.

While a game runs, a summary goes into the game's log every minute (and each warning once), so a
slow session can be checked afterwards. FPS needs administrator rights (or the *Performance Log
Users* group) and counts DirectX games; the rest works without. *View > Game stats bar* hides it.
Windows only for now. Details: [Game stats help page](docs/help/game-stats.md).

To get FPS without running as administrator, add your account to *Performance Log Users* once, in
PowerShell as administrator (put your name from `whoami` in place of `Admin`), then sign out and in:

```powershell
Add-LocalGroupMember -SID S-1-5-32-559 -Member Admin
```

The SID works in every Windows language. Full steps, and how to undo it:
[Adding your account to Performance Log Users](docs/help/game-stats.md#adding-your-account-to-performance-log-users).

The **GPU & Power** tab lists the PC's graphics cards with their driver version, asks NVIDIA whether a
newer driver is out (Intel / AMD: a button to their driver page), and sets which card the selected game
uses (*Let Windows decide*, *Power saving* or *High performance*: the setting of *Settings > System >
Display > Graphics*, applied when the game starts next). It also switches the power plan (also the
hidden ones, like *Power saver*) and the Windows power mode.

> **Tip for laptops:** set your games to *High performance*. What *Let Windows decide* picks can change:
> after an NVIDIA driver update it switched from the GeForce to the Intel UHD on a test laptop, so games
> without their own setting rendered on the slow card. And play plugged in: on battery the CPU and GPU
> run at a fraction of their speed.

**Out of the game's way.** WindowManager runs once: starting it again brings the running copy to the
front (also out of the floating bar). With *View > Keep this app off the game's monitor* (on by default)
it moves to the other monitor whenever the game's window, or the downscaled view, is on the monitor it
is on. **Ctrl+Alt+M** frees the mouse from a game that holds it (Unreal Engine games do while they have
the focus) and brings the app to the front.

**Floating bar.** Minimising the app turns it into a small always-on-top bar with the game's name, FPS,
CPU, GPU and the first warning. Drag it anywhere; double-click it or press **Open** to get the window
back (Ctrl+B switches to it directly; *View > Minimize to floating bar* turns it off).

---

## HelixSR (FSR 3.1 upscaler for AMD GPUs)

[HelixSR](https://github.com/lonewolf0622/HelixSR) is an unofficial free upscaler that replaces a
game's AMD FSR 3.1 DLL with one that runs NVIDIA's DLSS network as compute shaders, so it works
on AMD GPUs. The **HelixSR** tab automates its setup:

0. **Check this PC** (also run automatically before a set-up) tests the graphics card, shader model 6.6,
   16-bit math, free disk space and the tools the setup needs. An AMD RDNA 1+ card is supported; other
   cards are "experimental" (you are warned); a PC that cannot run it is stopped before the build.
1. **Set up HelixSR** downloads the newest HelixSR 1.x release (checked against its published SHA-256), lets
   HelixSR's own setup build the network on this PC (about 5 to 6 minutes, with a progress bar and
   timer), then deletes the downloads, the DLSS DLL and the build tools.
2. The app checks for a newer release at start-up (when HelixSR is set up) and on **Check for
   updates**; the tab shows *HelixSR (update)* when there is one. HelixSR 2.x is not offered yet: its
   setup installs into every game it finds by itself.
3. The tab lists every saved game. Games with an `amd_fidelityfx_dx12.dll` /
   `amd_fidelityfx_upscaler_dx12.dll` can be selected; the others are greyed out.
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
| [ Window | Position & Size | Presets | Downscale | Display | GPU & Power | Capture | HelixSR ] | Tabs
| +------------------------------------------------------------------------+ |
| |  controls for the selected tab                                         | |
| +------------------------------------------------------------------------+ |
| FPS 58 · 1% low 41  CPU 23 %  GPU 97 % · GeForce GTX 1650  RAM ...  Plugged in |   Game stats bar
| status message                                               target exe    |   Status bar
+----------------------------------------------------------------------------+
```

### Game bar
| Control | What it does |
|---|---|
| Dropdown | Your saved games. Type to filter. Picking one makes it the target. |
| **Add game...** | Register an `.exe`/`.bat`/`.cmd` and name it. Steam games are added by themselves. |
| **Start** | Launch the selected game. If a game is already running, asks whether to close it first (a game is never started twice). |
| **Restart** | Terminate it, then launch it again. |
| **Kill** | Terminate it. |

### Main actions
| Button | What it does |
|---|---|
| **Scan** | Read the window list again and pick the best window (the list also follows the game by itself). |
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

The **active window**, the one every button and tab acts on, is the green, bold row with Status
*Active*. Click another row to make that one active.

The table follows the game by itself: when the game opens, closes, moves or resizes a window (at
once on Windows, otherwise within 3 seconds). A window you picked stays active as long as it exists;
when it closes, the best remaining window (visible, largest) becomes active. **Scan** reads the list
again on demand and picks the best window.

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

**Display** - the PC's display settings, and the selected game's own settings while it runs; see
[Display settings](#display-settings).

**Game config** - the selected game's own config files (render scale, resolution, JConfig's window for
Tekken 7); see [Game config](#game-config-unreal-engine-4-games-tekken-7).

**GPU & Power** - the PC's graphics cards and their drivers (with an update check for NVIDIA), the card the
running game renders on, Windows' graphics setting for the game (Let Windows decide / Power saving / High
performance), the power plan and the power mode; see [Game stats bar](#game-stats-bar).

**HelixSR** - set up and update HelixSR, and install it into / remove it from your FSR 3.1 games;
see [HelixSR](#helixsr-fsr-31-upscaler-for-amd-gpus).

**Capture** - screenshot the active window as PNG; choose how many shots for a burst
(80 ms apart). **Open folder** shows the game's snapshot folder.

### Menus
| Menu | Items |
|---|---|
| **File** | Add game..., Find Steam games, Open config file, View logs..., Exit |
| **View** | Always on top, Window opacity, Confirm before terminating a game, Check window fit after launching a game, Fit window to monitor..., Game stats bar, Keep this app off the game's monitor, Release the mouse, Switch focus: game / this app, Focus switch hotkey..., Terminate the game (press twice), Terminate hotkey..., Minimize to floating bar, Switch to floating bar, Stop downscaled view, Move this app to the other monitor |
| **Help** | Help topics (F1), Keyboard shortcuts, Logs and troubleshooting, About |

---

## Keyboard shortcuts

| Keys | Action |
|---|---|
| Ctrl+S | Toggle the game between monitor 1 and monitor 2 |
| Ctrl+F | Fit window to monitor... (measure and propose the correction) |
| Ctrl+Alt+D | Stop the downscaled view (global: works even when this app is not focused) |
| Ctrl+Alt+M | Release the mouse from a game that holds it, and bring this app to the front (global) |
| Ctrl+M | Move this app's window to the other monitor |
| Ctrl+B | Switch to the floating bar |
| Ctrl+O | Add game... |
| F1 | Help. While a game runs: switch the focus between the game and this app (global; *View > Focus switch hotkey...*) |
| Ctrl+L | View logs... |
| F2, twice | While a game runs: terminate it, like Shutdown (global; *View > Terminate hotkey...*) |
| Ctrl+Q | Exit |

---

## Files it creates

All next to `WindowManager.exe`:

| File | Contents |
|---|---|
| `windowmanager.json` | Saved games (with each game's display settings and game config), the last target and per-game window profiles. Written atomically. |
| `windowmanager.ini` | App window size/position, Always on top, the terminate-confirmation preference, the automatic fit check on/off, the game stats bar shown or hidden, the floating bar (on / off, position), the window opacity and the focus switch and terminate hotkeys. While a game's display settings are on, also how to undo them. |
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
- The **Game config** tab edits the game's own config files. Before the first change it keeps a copy
  of each (`<file>.wm-original`, next to it); **Restore originals** puts the game's values back.
- A new resolution, orientation or display arrangement from the **Display** tab is undone after
  15 seconds unless you confirm it. A game's display settings are undone when it closes (or at the
  next start, if the app crashed).

## License

GPL-3.0, see [LICENSE](LICENSE).
