# Linux port

Working notes for porting WindowManager to Linux, with Bazzite (Desktop Mode, Proton games) as the main
target. Read this first when continuing on a Linux machine.

## Goal

Manage the windows of Windows games that run under Steam/Proton: borderless fullscreen, exact size,
monitor moves, fit-to-monitor, snapshots. Proton games are Wine processes shown through XWayland, so
the port controls **X11 windows** (XWayland included). Wayland-native windows are out of scope.

Out of scope: Bazzite's Gaming Mode (gamescope owns every window there; gamescope's own FSR/integer
scaling covers the downscaled view).

## Design

All OS access sits behind `PlatformBackend` (`src/platform/PlatformBackend.h`),
returned by `platform()`. `WindowManager`, `MonitorHelper`, the controller and `SnapshotCapture` only
call that interface. Implementations:

| Platform | Files |
|---|---|
| Windows | `src/platform/win32/` (`Win32Backend.cpp`, `Win32WindowStabilizer.cpp`) |
| Linux | `src/platform/x11/X11Backend.cpp` (xcb + EWMH, QScreen for monitors, `/proc` for processes), `src/platform/PollingWindowStabilizer.cpp` |

Tests: `tst_windowmanager` runs the logic against a fake backend; `tst_x11backend` (Linux) drives the real
backend against an `xterm` and skips itself without a display.

## Status

| Step | State |
|---|---|
| 1. `PlatformBackend` interface, Win32 implementation | done, tested on Windows |
| 2. X11 backend | done; tested on Ubuntu 26.04 / WSLg with xterm, and on Bazzite (KDE Plasma Wayland) with GOD EATER 3 under Proton: scan, borderless full screen and back, focus, start guard and Shutdown work |
| 3. Capture and downscaled view | snapshots read the window itself (`captureWindow`), as the root window has no pixels under XWayland; the downscaled view is stubbed (`src/platform/unsupported/ScaledViewUnsupported.cpp`) |
| 4. Packaging | not started. Plan: AppImage first. Flatpak breaks process access (own PID namespace) |
| 5. Steam-aware launch and identify | installed Steam games are added by themselves (`src/window/SteamLibrary.*`, `GameRegistry::syncSteamGames`, `tst_steamlibrary`) and started with `steam://rungameid/<appid>` (from a container through `distrobox-host-exec xdg-open`). Window matching is still by exe path; matching by AppID (`SteamMatch`) waits for the probe answers (see below) |
| 6. Game stats, GPU & Power, floating bar | added on Windows (2026-10-20). On Linux the stats backend is a stand-in (`src/platform/unsupported/StatsUnsupported.cpp`): no stats bar, the GPU & Power tab says it is not available. The floating bar is plain Qt and should work. See *Next steps* below |
| CI | `.github/workflows/linux-ci.yml` builds on Ubuntu and runs the tests. The `tst_x11backend` step runs under Xvfb without a window manager, is untested there, and is allowed to fail |

Not done on Linux: global hotkey (Ctrl+Alt+D), Layered / No-activate styles (`supportsFlag` is false),
HelixSR (Windows only; the GPU probe reports no adapters), launching a non-Steam `.exe` (currently just `wine <path>`).

## Answers from Bazzite (2026-10-19, KDE Plasma 6 Wayland, GOD EATER 3, Proton Experimental)

1. KWin supports 69 EWMH atoms, including `_NET_CLIENT_LIST`, `_NET_MOVERESIZE_WINDOW` and `_NET_ACTIVE_WINDOW`.
2. The game window has `_NET_WM_PID` (the host pid), `STEAM_GAME` = the AppID, and `WM_CLASS`
   `steam_app_<appid>`. The pid is valid in the host `/proc` and in the distrobox.
3. `cmdline` starts with the Windows image path, but on drive **S:** (`S:\steamapps\common\GOD EATER 3\ge3.exe`):
   Proton maps the Steam folder to S:, not Z:. `processImagePath` now follows the prefix's `dosdevices` links
   (`WINEPREFIX` from `/proc/<pid>/environ`). From the distrobox, `exe` and `environ` of a game in Steam's
   sandbox cannot be read (permission denied; `cmdline` can), so it falls back to looking under the Steam folder.
4. Wine marks the window fixed-size (min = max size in `WM_NORMAL_HINTS`), so KWin refuses a resize, and a
   changed `_MOTIF_WM_HINTS` does not remove KWin's title bar. Borderless full screen therefore uses
   `_NET_WM_STATE_FULLSCREEN` (frame gone, exact monitor size); *Windowed* leaves that state. Activation is
   sent with source "pager" (2): KWin's focus-stealing prevention ignores the application source.
5. `GetImage` on the root window fails (BadMatch) under XWayland; on the game window it returns the real picture.
6. Inside the container `/home/admin` and `/var/home/admin` are separate mounts of the same folder (no symlink).
   Image paths are rewritten to the `$HOME` form (`inHomeForm`) and Steam libraries are compared by file identity.

Still open: matching by AppID (`SteamMatch`) for games whose exe the app picks wrongly; two monitors (only
one was connected); Stability (the polling stabilizer) on a real game.

## Next steps (planned 2026-10-20)

The Windows side gained a game stats bar, a *GPU & Power* tab and a floating bar (see `docs/ARCHITECTURE.md`,
*Game stats bar*). Everything below needs the Bazzite machine: a real Proton game, KWin, and the Linux
power / GPU files. Work through it in this order.

### 1. Build and check what is there

Already checked (2026-10-20, Ubuntu 26.04 in WSL on the Windows PC, Qt 6.10, `~/wm-build`): the Linux build
compiles without warnings and all tests pass (13 since `tst_gamewatch`) (`QT_QPA_PLATFORM=offscreen`, so `tst_x11backend` had no X
display there). That check found a build break from the Display tab: its namespace `Display` clashed with X11's
`typedef struct _XDisplay Display` (declared by `QGuiApplication` through `qguiapplication_platform.h`); it is
now `DisplayModel`. **Never name anything `Display`, `Window`, `Screen`, `Status`, `Bool` or `None` at global or
namespace scope**: Xlib declares them and Qt pulls Xlib in on Linux.

1. Pull, then build and test in `wm-dev` (see *Setting up on Bazzite*). There are 13 tests now; `tst_stats`
   runs its pure parts on Linux (GPU counter names, CPU / frame-rate maths, warnings, driver and NVIDIA lookup
   parsing) and skips the live sample (`supported() == false`).
2. Start the app. Expected: no stats bar, *View > Game stats bar* absent, the *GPU & Power* tab shows
   "not available on this system yet".
3. **Floating bar:** minimise the app (or Ctrl+B). Check under KWin that it
   - appears at the top right and stays above other windows, also above a running Proton game in
     borderless full screen (keep-above for an XWayland window);
   - can be dragged (`QWindow::startSystemMove`) and keeps its spot after a restart (`miniBarPos` in
     `windowmanager.ini`);
   - does not take the keyboard from the game (`WindowDoesNotAcceptFocus`, `WA_ShowWithoutActivating`);
   - brings the window back with double-click and *Open*, and *Exit* in its right-click menu closes the app.
   Write down what KWin does differently.

### 2. A Linux stats backend

New file `src/platform/linux/LinuxStats.cpp` (or under `x11/`) implementing `StatsBackend`, replacing the stand-in
for Linux in `CMakeLists.txt` (app and `tst_stats`). Keep parsing in pure functions with tests, like the Windows
side does in `StatsModel` (for example `parseProcStat`, `parseFdinfo`, `parsePowerSupply`). `Stats::Sampler`,
the warnings and the log line are shared and need no change. The game's pid already comes from
`_NET_WM_PID` (the host pid, readable in the container, see answer 2 above).

| Item | Source | Notes |
|---|---|---|
| CPU | `/proc/<pid>/stat` fields 14 + 15 (utime + stime, clock ticks: `sysconf(_SC_CLK_TCK)`), `/proc/stat` first line for the system | `RawSample::processCpu100ns` is in 100 ns units: convert the ticks. Wine runs a game as several processes (`wineserver`, the game exe): count the game process, maybe its children |
| RAM | `/proc/<pid>/status` `VmRSS` | Same meaning as the working set |
| GPU use, VRAM per process | `/proc/<pid>/fdinfo/*`: `drm-engine-gfx` (ns busy, cumulative), `drm-memory-vram` / `drm-resident-vram`, `drm-pdev` (which card) | Busy % = delta of `drm-engine-gfx` / wall time. First find out which GPU the machine has (`lspci -k | grep -A3 VGA`) |
| GPU use, whole card | `/sys/class/drm/card*/device/gpu_busy_percent`, `mem_info_vram_used` (amdgpu) | Fallback when fdinfo is not readable (game in Steam's sandbox: check permissions, see answer 3) |
| Adapters | `/sys/class/drm/card*/device/{vendor,device}`, name from `lspci` or `/sys/.../uevent`; `luid` can be the card index | `Adapter::dedicatedBytes` from `mem_info_vram_total` |
| Power | `/sys/class/power_supply/*/{type,online,status,capacity}` | A desktop has no battery: `batteryPercent = -1` |
| Power plan / mode | power-profiles-daemon over D-Bus (`net.hadess.PowerProfiles`, `ActiveProfile`: power-saver / balanced / performance); Bazzite may run `tuned-ppd`, which offers the same D-Bus API. `powerprofilesctl` to try it by hand | Map to `powerPlans()` (three entries) and leave `PowerMode` empty, or map the profiles to `PowerMode` and show one list. Decide when you see what Bazzite runs |
| GPU preference | Not a thing on a single-GPU PC. On a hybrid laptop: `DRI_PRIME=1` (Steam launch options `DRI_PRIME=1 %command%`) | `gpuPreferenceSupported()` false at first; the tab can say why |
| Driver | Mesa version (`glxinfo -B` / `vulkaninfo --summary`, or `/sys/module/amdgpu/version` is empty for in-kernel): show it, no update check | Updates come with the Bazzite image (`ujust update`, `rpm-ostree status`). The tab could show the image date instead of a vendor check |
| FPS | No equivalent of the DirectX present events. Options: (a) MangoHud, which Bazzite ships: with `MANGOHUD=1` and `output_folder` / `autostart_log` it writes frame times to a CSV the backend can tail; (b) gamescope's own stats; (c) a Vulkan layer of our own (much more work) | Start with (a). `FpsSource::Unsupported` with a hint ("Enable MangoHud logging for FPS") until then |

Then enable the stats bar on Linux (`StatsBar::available()` follows `supported()`), check every value against
MangoHud's overlay or `btop` / `amdgpu_top` with a running game, and fill in the warnings that make sense
(on battery for a laptop; power-saver profile as the "Power-saving mode" warning).

The game watch (`src/window/GameWatch.h`, diagnostic log of the game's windows, focus and mouse) is a stand-in on
Linux too. An X11 version can watch `PropertyNotify` / `ConfigureNotify` on the game's windows and
`_NET_ACTIVE_WINDOW` on the root; the mouse grab has no equivalent of `GetClipCursor`. `Watch::WindowTracker` and the
line functions are shared.

### 3. GPU & Power tab on Linux

With the backend in place, the tab works unchanged for what the backend supports: power profile switching,
the card list with the driver line. Hide or explain the parts that do not apply (GPU preference, NVIDIA
check on an AMD card already falls back to "Driver page"; on Linux that page should point to Bazzite's
update instead).

### 4. AppImage

As planned under step 4 of *Status*: an AppImage first (`linuxdeploy` with the Qt plugin), not Flatpak
(its own PID namespace breaks process access). Test it on the host, outside the distrobox.

### Probes for step 2 (game running, in the container)

```
lspci -k | grep -A3 -E "VGA|3D"
ls /sys/class/drm/; cat /sys/class/drm/card*/device/gpu_busy_percent
cat /proc/<pid>/stat; grep VmRSS /proc/<pid>/status
for f in /proc/<pid>/fdinfo/*; do grep -H drm- "$f"; done 2>/dev/null | head -40
ls /sys/class/power_supply/; cat /sys/class/power_supply/*/type
powerprofilesctl; busctl --system list | grep -i -E "PowerProfiles|tuned"
ls ~/.config/MangoHud 2>/dev/null; which mangohud gamescope
```

Paste the answers into this file, like the 2026-10-19 answers above.

## Original questions

1. What does KWin (Plasma Wayland) expose to XWayland clients? `xprop -root _NET_SUPPORTED _NET_CLIENT_LIST`.
2. For a running Proton game, what are `_NET_WM_PID`, `STEAM_GAME` and `WM_CLASS` (expect `steam_app_<appid>`)?
   Does the PID match the host `/proc` (pressure-vessel may use its own PID namespace)?
3. What does `/proc/<pid>/cmdline` of the game process look like? `X11Backend::processImagePath` assumes the
   Windows image path (`Z:\...\game.exe`) comes first.
4. Do move, resize, strip frame, fit-to-monitor and snapshot work on a real game under KWin?
   (KWin may ignore `_MOTIF_WM_HINTS`; Wine may re-apply its own frame.)
5. Does `xcb_get_image` of the root return the game picture under XWayland, or black?
6. Bazzite's home is `/var/home/<user>` with `/home` as a link. Steam's `libraryfolders.vdf` says `/var/home/...`,
   the container sees `/home/...`. Does the image path of a running game match the saved exe path, or do both
   sides need `canonicalFilePath`?

## Step 5 plan: identify games by Steam AppID

Exe-path matching can fail if the PID namespace differs. Steam sets `STEAM_GAME` (the AppID) on game
windows, and the window class is `steam_app_<appid>`. Plan: match by AppID first, exe name second; launch
Steam games with `steam steam://rungameid/<appid>`; add a Steam game picker. Keep the pure matching
logic in its own unit with tests, like `KillPlanner`.

## Setting up on Bazzite

Bazzite is immutable, so build in a distrobox container. Distrobox shares the host's `/proc`, X11 socket
and Steam processes. Use a **Desktop Mode** session. Open the repo in VS Code *inside* the container so
Claude Code can build and run things.

```
git clone <repo url> WindowManager && cd WindowManager
distrobox create -n wm-dev -i registry.fedoraproject.org/fedora-toolbox:44
distrobox enter wm-dev
sudo dnf install -y cmake ninja-build clang gcc-c++ qt6-qtbase-devel qt6-qtbase-private-devel \
    qt6-qttools-devel libX11-devel libXrandr-devel libXext-devel libxcb-devel xterm git
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
```

This dnf line was used on Bazzite (Fedora 44 toolbox image). `BUILD.md` lists the Ubuntu names. The host has
`xdotool`, `xprop`, `wmctrl` and `spectacle` for the probes. On a machine with little RAM, run
`scripts/free-memory.sh` (add `--close` to close browsers and IDEs) before starting a game.

Run the app from the container: `./build/linux/WindowManager` (it sets `QT_QPA_PLATFORM=xcb` itself).

### Probes (start a Proton game first, then run these in the container)

```
xprop -root _NET_SUPPORTED _NET_CLIENT_LIST
xdotool search --class steam_app          # window ids of Proton games
xprop -id <id> _NET_WM_PID STEAM_GAME WM_CLASS _NET_WM_NAME _NET_FRAME_EXTENTS _NET_WM_STATE
ps -eo pid,ppid,args | grep -i "<game name>"
tr '\0' ' ' < /proc/<pid>/cmdline; echo
readlink /proc/<pid>/exe
```

Paste or summarise the output in the repo (for example under this file) so the answers are not lost.

## Working rules

Same as `CLAUDE.md`: the user does all git operations; never commit, stage or push.
