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
