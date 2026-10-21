# Magpie (window upscaler): analysis

Looked at and tried on 2026-10-20 (see *Trial*) as a possible integration; nothing is built yet. Source read: `Blinue/Magpie` tag
`v0.13.0` (released 2026-10-17, commit `1a6f6c6`), GPL-3.0, about 16k stars, pushed daily.

## What it is

[Magpie](https://github.com/Blinue/Magpie) captures a window and draws it upscaled in its own topmost window
(full screen on the window's monitor, or windowed since 0.12). Effects: Lanczos, FSR 1 (EASU + RCAS), FSRCNNX,
CuNNy, Anime4K, CRT shaders, integer scaling. It works on any window and any graphics API, because it captures
instead of injecting: D3D11 (Tekken 7), Vulkan (shadPS4), emulators. Needs Windows 10 1903+ and DirectX feature
level 11. Not installed on this PC.

Why it fits this app: the game runs **windowed at a lower resolution** (an exact size, which the app already
sets) and Magpie fills the monitor with it. That is the reverse of the Downscale view, and it covers the games
HelixSR cannot (HelixSR needs a DX12 game with FSR 3.1). Nothing goes into the game's folder, so there is no
anti-cheat risk from files, and the app would only download and start Magpie, never ship it.

## How another app can drive it

| Need | What Magpie offers (v0.13.0) |
|---|---|
| Start scaling a game | **No command line or API for it.** The only argument is `-t` (start in the tray). Scaling starts from Magpie's hotkey (Alt+Shift+A / Alt+Shift+Q windowed) or from a **profile with `autoScale`**: Magpie checks the foreground window on a timer and starts scaling when it matches. |
| Profile matching | `classNameRule` must equal the window class exactly, and `pathRule` must equal the exe path exactly (case-sensitive string compare, as Windows reports it for the window's process: backslashes, case on disk). `packaged: false` for normal games. |
| Profile settings | `autoScale` (0 off, 1 full screen, 2 windowed), `scalingMode` (index into the top-level `scalingModes` array; -1 = none), `captureMethod`, `multiMonitorUsage`, `3DGameMode`, `graphicsCardId`, `launcherPath`, `launchParameters`, frame-rate limit, cropping, cursor scaling. Default modes on a fresh config: 0 Lanczos, 1 FSR, 2 FSRCNNX, 3 CuNNy, 4 Anime4K, 5 CRT-Geom, 6 Integer Scale 2x (look modes up by `name`, not by index). |
| Config file | `config.json`. **Portable mode** when `config\config.json` exists next to `Magpie.exe` (the config then lives there), else `%LOCALAPPDATA%\Magpie\config\v5\config.json`. Read **once at start-up**; Magpie writes its in-memory settings back on every change, so an edit made while it runs is lost. |
| Quit / show | Registered window messages `WM_MAGPIE_QUIT` and `WM_MAGPIE_SHOWME`, posted to `HWND_BROADCAST` (that is how Magpie's own second instance talks to the first). Single instance per user (mutex). |
| Follow its state | Registered message `MagpieScalingChanged`: wParam 0 = stopped (lParam 1: the game lost the focus), 1 = started (lParam = Magpie's window), 2 = its window moved / resized, 3 = the user is moving it. The scaling window has the fixed class `Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22` and the properties `Magpie.SrcHWND`, `Magpie.Src*`, `Magpie.Dest*`, `Magpie.Windowed`. An elevated receiver must allow the message (`ChangeWindowMessageFilterEx`). Coordinates are physical (Per-Monitor V2). |
| Download | GitHub release `Magpie-v<version>-x64.zip`, about 10 MB, with a SHA-256 `digest` on the asset (same check as HelixSR). |

## What it refuses (and what that means here)

- **A borderless full-screen or maximized game** (`ScalingError::Maximized`), unless the global setting
  `allowScalingMaximized` is on. So with Magpie the app must leave the game **windowed at its render size**;
  *Fit to monitor* and true borderless full screen must not run on that game.
- **A window of a higher integrity level** (a game started elevated while Magpie is not). The app should start
  Magpie with the same elevation as the game.
- Windows off every monitor, hung windows (it waits), transparent (`WS_EX_TRANSPARENT`) windows, minimized ones.
- **A source window that changes size while scaled** stops the scaling (`源窗口意外出现尺寸变化`). Tekken's JConfig
  resets the window on every focus. That is harmless if the reset size is the size Magpie scales (set JConfig's
  window to the render size), but not if the app resizes the game afterwards.
- Exclusive full screen cannot be captured; the game has to be windowed or borderless windowed at its size.

Magpie also special-cases TeknoParrot's window class (the arcade loader), so arcade-loader games are a known use.

## Possible integration (not started)

1. **Pure unit `MagpieConfig`** (done 2026-10-20: `src/magpie/MagpieConfig.*`, `tst_magpie`; a config it wrote
   made the real Magpie 0.13.0 scale the stand-in, and after `removeGameProfiles` it did not): read `config.json`, find a scaling mode by name, add or update one
   profile for a game (`name`, `packaged`, `pathRule`, `classNameRule`, `autoScale`, `scalingMode`, `3DGameMode`),
   keep everything else in the file. Profiles the app made are recognised by name prefix and removed on request.
2. **Own portable copy** (done 2026-10-20: `MagpieRelease`, `MagpieProcess`, `MagpieSetup`, the Magpie tab;
   set-up, the refusal while another Magpie runs, and an update that closed the app's running Magpie and kept
   its config were tried in a scratch copy of the app): download the release zip into `<app dir>/magpie`, check its digest, create
   `config/` so Magpie runs in portable mode. The app then owns that config and never touches a Magpie the user
   installed themselves. Update check like HelixSR's.
3. **Per game "Upscale with Magpie"** (done 2026-10-20: `Magpie::GameSettings` in the game's entry, the Magpie tab's
   *Selected game* box, `MagpieSession`; `fitProposed(bool automatic)` lets the automatic fit check be skipped.
   Tried in a scratch copy of the app with the stand-in: profile written 3 s after Start, scaling at 6 s, the
   automatic fit check skipped, Stability turned off, an already running Magpie restarted with the new profile,
   Magpie closed when the game ended (F2, or killed from outside); the effect picked in the tab was saved and
   used) (off / FSR / Lanczos / ...; on the game's tab): on Start, quit Magpie
   (`WM_MAGPIE_QUIT`, wait for the process), write the profile with the game's exe path and window class (taken
   from the running game window, so the strings match exactly), start Magpie with `-t`, set the game's window to
   its render size. On the game's exit, quit Magpie again (optional).
4. **Cooperate** (partly: the fit check and Stability above; not yet: the Downscale view, the floating bar and
   opacity treating Magpie's window as the game's picture): listen to `MagpieScalingChanged` and log it; while Magpie scales, the Downscale view, *Fit to
   monitor* and the exact-size re-assert stay off for that game; the floating bar, the opacity and "keep off the
   game's monitor" treat Magpie's window as the game's picture.

## Trial (2026-10-20)

Magpie 0.13.0 (zip checked against the release digest) in a scratch folder, portable, with a listener for
`MagpieScalingChanged`; the stand-in game (`scripts/demo-video/DemoGame.cs`, WinForms, 856x488 client) on
monitor 2 (1920x1080 at 0,-1080); a scratch copy of WindowManager for the app tests. All on this PC.

| Test | Result |
|---|---|
| Portable start | An empty `config\config.json` next to `Magpie.exe` gives portable mode; Magpie fills it with its defaults (4.8 KB; `profiles[0]` is the default profile) and stays in the tray with `-t`. |
| Quit | `WM_MAGPIE_QUIT` to `HWND_BROADCAST`: Magpie exits at once and saves. |
| Profile written by another program | A profile with the game's exe path and window class (`WindowsForms10.Window.8.app.0.34f5582_r6_ad1`, read from the running window), `autoScale` 1 and the mode found by name (`FSR`) scaled the game full screen with no clicks; Magpie kept the profile when it rewrote its config. |
| Start time | From starting Magpie (game in front) to its scaling window: 0.44 s with an empty shader cache, 0.34 s with it (FSR). Started from the app: scaling 2.6 s after **Start** (game start included). |
| Messages | `started` arrives **twice** per start; `stopped` with lParam 1 when the game loses the focus. |
| Focus away and back (F1, also charmap) | Magpie pauses ("source lost focus") and keeps its window; back on the game it scales again within about 50 ms. The app's F1 both ways works. |
| Moving / resizing the game while scaled | Scaling stops and the auto-scale profile starts it again (0.4 s after a resize, 1.8 s after a move). One reset is a short flash; an app that keeps re-asserting the size would make it restart over and over. |
| Borderless full screen / maximized | Not scaled (`源窗口已最大化`, error 6, with Magpie's own error popup). |
| F2 twice (app ends the game) | Magpie stops by itself and stays in the tray. |
| Stats (now the game info overlay) | Measures the game's process only: Magpie's upscaling work (in `Magpie.exe`) is not in its GPU figure. The stand-in draws with GDI, so FPS and GPU need a real game. |
| **Mouse with `3DGameMode` on** | Magpie confines the cursor to the game **every frame and never lets go**, also while another window is in front and after the app's Ctrl+Alt+M; the mouse cannot reach the app on the other monitor until Magpie quits (its source: "3D game mode: clip every frame, never release, no multi-monitor"). |
| Mouse with `3DGameMode` off | The cursor is released when the game loses the focus and can reach the other monitor; scaling resumes when the game is in front again. |

What this means for an integration:

- Write profiles with **`3DGameMode` off** (offer it per game only with a warning: mouse-look games may need
  it, and then the mouse is the game's until Magpie quits; the app's F2 / Shutdown should quit Magpie too).
- Take the exe path and window class from the running game window; quit Magpie before editing its config.
- While Magpie scales a game, the app must not move or resize that window (exact-size re-assert, Fit to
  monitor, borderless), and the game has to stay windowed.
- Treat the double `started` as one event; log `stopped` / `started` with the reason.
- The watch's "Mouse: held inside ..." line comes from Magpie's cursor capture while scaling, not from the game.
- **Tekken 7, first real try (2026-10-21):** the picture looked wrong because the game's window was bigger
  than the monitor: the Game config tab had written resolution 3840 x 2160 (also JConfig's DX resolution), the
  window follows the DX resolution, and Windows clamps it to about the size of the whole desktop (two stacked
  1920 x 1080 monitors: 1942 x 2176, drawn area 1926 x 2160). Magpie fitted that tall window into one monitor
  (the cursor box was 976 x 2160, centred). The first start also wrote the profile for Tekken's splash window
  (`SplashScreenClass`), not `UnrealWindow`. Fixed: the session follows a window swap, and warns when the
  window is bigger than its monitor.
- Still to try with a real game: Tekken 7 through JConfig (its window reset, the input emulator), FPS and GPU
  figures, the shader-cache delay of heavier modes (CuNNy, FSRCNNX), and an elevated game (Magpie must then run
  elevated too).
