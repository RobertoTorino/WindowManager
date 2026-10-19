# Linux port (branch `linux-port`)

Working notes for porting WindowManager to Linux, with Bazzite (Desktop Mode, Proton games) as the main
target. Read this first when continuing on a Linux machine.

## Goal

Manage the windows of Windows games that run under Steam/Proton: borderless fullscreen, exact size,
monitor moves, fit-to-monitor, snapshots. Proton games are Wine processes shown through XWayland, so
the port controls **X11 windows** (XWayland included). Wayland-native windows are out of scope.

Out of scope: Bazzite's Gaming Mode (gamescope owns every window there; gamescope's own FSR/integer
scaling covers the downscaled view).

## Design

All OS access sits behind `PlatformBackend` ([src/platform/PlatformBackend.h](../src/platform/PlatformBackend.h)),
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
| 2. X11 backend | done, tested on Ubuntu 26.04 / WSLg with xterm only (WSLg's window manager has no `_NET_CLIENT_LIST`; the fallbacks cover that) |
| 3. Capture and downscaled view | capture works via `xcb_get_image`; the downscaled view is stubbed (`src/platform/unsupported/ScaledViewUnsupported.cpp`) |
| 4. Packaging | not started. Plan: AppImage first. Flatpak breaks process access (own PID namespace) |
| 5. Steam-aware launch and identify | the matching logic is done and tested (`src/window/SteamMatch.*`, `tst_steammatch`); not wired into the backend or the UI yet, which needs the Bazzite probe answers (see below) |
| CI | `.github/workflows/linux-ci.yml` builds on Ubuntu and runs the tests. The `tst_x11backend` step runs under Xvfb without a window manager, is untested there, and is allowed to fail |

Not done on Linux: global hotkey (Ctrl+Alt+D), Layered / No-activate styles (`supportsFlag` is false),
HelixSR (Windows only; the GPU probe reports no adapters), launching a `.exe` (currently just `wine <path>`).

## Open questions to answer on Bazzite

1. What does KWin (Plasma Wayland) expose to XWayland clients? `xprop -root _NET_SUPPORTED _NET_CLIENT_LIST`.
2. For a running Proton game, what are `_NET_WM_PID`, `STEAM_GAME` and `WM_CLASS` (expect `steam_app_<appid>`)?
   Does the PID match the host `/proc` (pressure-vessel may use its own PID namespace)?
3. What does `/proc/<pid>/cmdline` of the game process look like? `X11Backend::processImagePath` assumes the
   Windows image path (`Z:\...\game.exe`) comes first.
4. Do move, resize, strip frame, fit-to-monitor and snapshot work on a real game under KWin?
   (KWin may ignore `_MOTIF_WM_HINTS`; Wine may re-apply its own frame.)
5. Does `xcb_get_image` of the root return the game picture under XWayland, or black?

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
git clone -b linux-port <repo url> WindowManager && cd WindowManager
distrobox create -n wm-dev -i fedora:latest
distrobox enter wm-dev
sudo dnf install -y gcc-c++ cmake ninja-build pkgconf-pkg-config qt6-qtbase-devel libxcb-devel \
    xterm xdotool xprop xwininfo
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
```

The dnf line is untested; fix package names if a first run complains. `BUILD.md` lists the Ubuntu names.

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
