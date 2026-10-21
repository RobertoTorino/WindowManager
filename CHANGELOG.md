# Changelog

Notable changes to WindowManager. The version number lives in `APP_VERSION` in `CMakeLists.txt`
(CI bumps it); the build number and date are shown in the window title and Help > About.

## Unreleased

### Added
- **Game info overlay** (Windows): a see-through panel in a corner of the game's monitor, once a second: which
  scaler shows the picture (Magpie and its effect, the downscaled view, none), from what size to what size, the
  scale and *upscaling* / *downscaling* (Magpie's numbers are read from its own scaling window), *starting* /
  *paused* / *not scaling* with the reason, the resolution and render scale in the game's files, the game's window
  and monitor (with refresh rate), FPS, CPU / GPU, RAM / VRAM, power, and the warnings with what to do (a window
  bigger than its monitor, Magpie not scaling, on battery, integrated GPU...). Clicks go through it, it never takes
  the keyboard, stays above Magpie's full-screen picture and is left out of screenshots, snapshots and recordings
  (unless ticked). *View > Game info overlay* (show, corner, in screenshots), **Ctrl+Alt+I** from anywhere.
  The **status bar** shows the scaler line at all times (orange with the reason in the tooltip), and the game log
  gets a `[Picture]` line when it changes. `tst_picture`.
  The overlay and the log also show **Magpie's own GPU use** (`GPU 29 % + Magpie 6 %`: its upscaling runs in
  `Magpie.exe`), and while Magpie waits or pauses they name the program in front (`JConfigTK7R2.exe is in front,
  click the game`).
- **Reports tab** (Windows): every game session is recorded once a second (`reports/<game>/<start>-samples.csv`:
  FPS, 1 % low, CPU, GPU, Magpie's GPU, RAM, VRAM, the setup), with a summary per session (`<start>.json`, per setup
  averages and peaks) and a row in `reports/sessions.csv`; no private data. Pick a game to see its sessions, a
  session over time (FPS above, GPU below, coloured by setup, hover for values), and its setups compared over all
  sessions (average FPS, 1 % low, GPU game + Magpie). **Compare picture** captures the game's own frame and Magpie's
  picture at the same moment, stretches the frame plainly (bilinear) for comparison, scores the sharpness and saves a
  side-by-side zoom (*Magpie FSR is 15.5 % sharper than a plain stretch*). Charts are drawn by the app (no Qt Charts).
  *Larger view* opens the tab in a window. Help page *Reports*, wiki page *Reports*; `tst_report`.
- Wiki: the *Game config* page warns that turning the tab off does not undo *Write now* (the settings stay in the
  game's files until *Restore originals*), and has Tekken 7 settings for Magpie (a resolution below the monitor's).
- **Magpie tab** (Windows): sets up and updates the app's own copy of [Magpie](https://github.com/Blinue/Magpie), a
  window upscaler that works with any game (DirectX 11/12, Vulkan, emulators). The release zip is checked against
  its SHA-256 digest and kept in the `magpie` folder next to the program in Magpie's portable mode, so a Magpie the
  user installed is not changed; Magpie is started once so it writes its default settings. The set-up stops when
  another Magpie is running (only one can run). Updates keep Magpie's settings, shader cache and logs.
  **Per game** (*Selected game > Scale with*: an effect from Magpie's list, or Off; optional 3D game mode): on
  Start the app writes the game's Magpie profile from its settled window (class and exe path) and starts Magpie,
  which scales the window whenever it is in front; Magpie closes when the game ends. For such a game the fit check
  after Start is skipped and Stability is turned off (both would stop the scaling). A game that shows a splash
  window first gets its profile written again for its real window (Tekken 7's first run had a profile for its
  splash screen, so it was not scaled). A game window bigger than its monitor gets a warning: Magpie only scales
  up and shrinks such a window, squeezed (Tekken 7 with a 3840 x 2160 resolution on a 1920 x 1080 monitor).
  Help page *Magpie*; `tst_magpie`.
- **Demo video** on the wiki's home page (*See it in action*): a one-minute tour of Start, Fit to monitor, the
  F1 focus switch, Windowed, exact sizes, snapshots and Kill. `scripts/demo-video/record-demo.ps1` records it
  again (stand-in game, captions, 1080p for YouTube and 720p for the wiki).
- **Game config tab**: settings written into the selected game's own config files on Start / Restart, for
  Unreal Engine 4 games with loose config files (first for Tekken 7's arcade builds). **Render scale**
  (`r.ScreenPercentage`: above 100 % the engine supersamples in the same window, below 100 % it is faster),
  **Resolution** (engine settings, user settings, `r.setres`, JConfig's DX resolution) and **JConfig window**
  (`monitor_misc.bin`: the rectangle Tekken puts its window back to on every focus). Only those keys change; a copy
  of each original is kept as `*.wm-original`, and **Restore originals** puts the game's values back. Each change
  is logged (`GameConfig` lines). Help page *Game config*.
- **Focus switch**: while a game runs, the stats bar shows **Focus: game / app / other**, and **F1** switches the
  keyboard between the game and this app, also while the game has it (Tekken 7 keeps the focus, so the app's own
  shortcuts never arrive). The app locks the foreground (`LockSetForegroundWindow`); a game that grabs it back anyway
  (Tekken 7 with JConfig) puts the app in **mouse only** (`WS_EX_NOACTIVATE`): its buttons work with the mouse while
  the game keeps the keyboard and controller, until the game closes. Each switch is logged with who had the focus
  and who took it back. Registered only while a game runs; another key under *View > Focus switch hotkey...*.
- **See-through window**: *View > Window opacity* (100 % to 40 %) makes the app's window transparent so it can sit
  over the game on a single monitor; by default only while a game runs (*Only while a game runs*). Dialogs stay solid.
- **Terminate key**: **F2 pressed twice within 2 seconds** terminates the game like *Shutdown*, also while the game
  has the focus (the first press beeps). Registered only while a game runs; *View > Terminate hotkey...*.
- **Game stats bar** (Windows): a second bar above the status bar with the running game's FPS and 1 % low,
  CPU, GPU use and the graphics card it renders on, RAM, video memory, and the power state. A warning
  appears for what is known to slow games down: on battery, battery saver, the power-saving power mode,
  the integrated GPU while a faster one is there, or the whole CPU busy. A summary goes into the game log
  every minute, and each warning once. FPS comes from the DirectX present events and needs administrator
  rights (or the Performance Log Users group). *View > Game stats bar* hides it. Help page *Game stats, GPU
  and power*.
- **GPU & Power tab** (Windows): the PC's graphics cards with their driver version and date, the card the running
  game renders on, and Windows' graphics setting for the selected game (Let Windows decide / Power saving / High
  performance, with the card each one picks). It is the setting of *Settings > System > Display > Graphics*; other
  entries Windows keeps for the exe are left alone. A change while the game runs offers *Restart game*.
- **Driver update check**: for an NVIDIA card the tab asks NVIDIA's driver lookup for the newest Game Ready driver
  and shows *Up to date* or *Update available*; Intel and AMD get a button to their driver page.
- **Power plan and power mode** on the same tab: every power plan (also the hidden ones, like *Power saver*) and
  Windows' power mode (with the Balanced plan).
- **Floating bar**: minimising the app turns it into a small always-on-top, draggable bar with the game's name,
  FPS, CPU, GPU and the first warning; double-click or *Open* brings the window back. Ctrl+B switches to it;
  *View > Minimize to floating bar* turns it off. It does not take the keyboard from the game.
- **Docs for slow games and FPS without admin.** Help, README and *Logs and troubleshooting* now explain the two
  common causes of a slow game on a laptop (on battery; the integrated GPU) and advise setting games to *High
  performance*: a graphics driver update can change what *Let Windows decide* picks (seen after an NVIDIA update:
  GeForce -> Intel UHD). Step-by-step: adding your account to *Performance Log Users* (by the group's SID, so it
  works in every Windows language), signing in again, checking it, and undoing it.
- **Display tab: the PC's display settings in the app** (Windows). Per monitor: resolution, refresh rate
  and orientation (kept only if confirmed within 15 seconds), scale, HDR, brightness (laptop panel through
  WMI, external monitors through DDC/CI), colour profile, and the display arrangement (PC screen only /
  Duplicate / Extend / Second screen only, also confirmed). *Night light...* opens its Windows Settings
  page. On Linux the tab says it is not available yet.
- **A game's own display settings.** *While the game runs* on the Display tab: resolution, refresh rate,
  HDR, scale and brightness for the selected game, applied on Start / Restart before the game starts and
  undone when it closes (or did not start within 3 minutes, on *Restore now*, or when the app closes).
  The undo record is kept in `windowmanager.ini`, so a crash is undone at the next start. Help page
  *Display settings*.
- Help page *Linux (beta)* (in the app under Help and on the MkDocs site): what works on Bazzite with Proton
  games, what does not yet, and how to build it. Linux notes on the downscaled view, window styles and
  shortcuts pages. The MkDocs site gets a *Development* section with the Linux port and HelixSR 2.x notes.
- README badges: the release build's status and *Beta*.
- **Steam games are found by themselves.** At start-up (and with *File > Find Steam games*) the app reads
  Steam's library files and adds every installed game to the game list, with Steam's name and the game's
  main executable. Proton, the Steam Linux Runtime and the redistributables are left out. An uninstalled
  game is removed (its saved position is kept); names you gave are kept. Works on Windows and Linux.
- **Steam games start through Steam** (`steam://rungameid/<AppID>`), so Proton, DRM and the overlay work.
- `scripts/free-memory.sh` (Linux): shows what uses memory and closes browsers / IDEs before testing a game.
- **The wiki**: the full documentation as a site on GitHub Pages (<https://robertotorino.github.io/WindowManager/>),
  with a blue theme, a home page, a header with the icon and *GitHub* / *Wiki* links (search on the wiki pages), a
  footer and placeholder screenshots. Built by the new `docs.yml` workflow on every
  push to `main` that touches `docs/`, `overrides/` or `mkdocs.yml`. New pages: Download and install, The main
  window, Focus, mouse and hotkeys, GPU and power, Files and settings, Safety, and under Development: Building and
  testing (was `BUILD.md`) and The docs site.
- **Help > Online wiki**, and a link to the wiki in the help window. Links to the wiki in the help pages open in the
  browser.

### Changed
- **The game stats bar is gone**: its numbers and warnings are on the game info overlay. The floating bar shows
  the game's name and *Open* only. The stats are read whenever a game runs (the game log's minute summary no
  longer depends on a bar being shown). The status bar shows the target's file name (full path in the tooltip).
- New app icon (window, taskbar and the `.exe`).
- **The README and the in-app help are short now**: the README says what the app is for and how to get started;
  each help page has the highlights and a link to its wiki page. New help topic *Focus and hotkeys*; the window
  terms reference moved to the wiki.
- **The window list follows the game by itself**: when the game opens, closes, moves or resizes a window (at once on
  Windows through the game watch, otherwise every 3 seconds), without a log line each time. A window you picked
  stays active while it exists; when it closes, the best remaining one becomes active. Before, the list only
  changed on **Scan**, so actions could go to a window the game had already replaced.
- **The active window stands out**: its row is green and bold, with Status *Active*, also when the list does not
  have the focus.
- **WindowManager runs once.** Starting it again brings the running copy to the front, also out of the floating
  bar, instead of opening a second one (two copies managed the same game and wrote every log line twice).
- **The app keeps off the game's monitor** (*View > Keep this app off the game's monitor*, on by default): when the
  game's window or the downscaled view is on the app's monitor, the app moves to the other one.
- **Ctrl+Alt+M releases the mouse** from a game that holds it (Unreal Engine games such as Tekken 7 do while they
  have the focus) and brings the app to the front, from anywhere. Also *View > Release the mouse*.
- **More in the game log for games that fight over their window, focus or mouse** (Windows). While a game runs,
  the log gets each of the game's windows (shown, resized, moved, frame, hidden, closed), every change of the
  foreground window (the game, this app, or another program by exe name only) and whether the mouse is locked.
  The downscaled view logs its layout (monitor, overlay and its display scale, game window, drawn area, picture)
  when it changes, each time it puts the game window back, and when it gives up. For diagnosing Tekken 7's start
  screen, its quarter picture in the downscaled view, and the mouse lock.
- No more nightly builds: the *Nightly / Beta* workflow (pre-releases from `develop` and `test-v*` tags) is
  removed. Releases come from `main` only.
- **The HelixSR tab lists every game.** Games without an AMD FSR 3.1 DLL are greyed out and cannot be
  selected; the others come first. *Check selected game* and *Forget* are gone, as the list now checks
  every game by itself.
- With no game selected, the game dropdown is empty instead of showing the first game (which looked
  selected but was not the target).
- *File > Open config file* opens the file in the default editor on Linux (it ran `notepad.exe`).
- The *Add game...* dialog on Linux also offers `*.sh`, `*.x86_64` and all files.
- Snapshots go through `PlatformBackend::captureWindow` (on Windows the same screen capture as before).

### Fixed
- *View > Check window fit after launching a game* turned off did not survive a restart: the setting was saved,
  but at the next start the fit check was on again.
- **Downscaled view drawn far too small (or too big)** on a monitor with another display scale: the black overlay
  was resized by Windows / Qt (seen at 640 x 480 on a 1920 x 1080 monitor). It is now pinned to the monitor.
- **Downscaled view gave up after 5 corrections.** Tekken 7 sets its window back to 1280 x 720 every time it gets
  the focus; the view now puts it back every time, and only follows the game when it resizes more than 5 times
  within 10 seconds.
- The *Integrated GPU* warning fired while a game's splash screen started, although the game itself rendered on
  the dedicated GPU. It now waits until the game uses the GPU (10 % or more).
- **The Linux build failed** since the Display tab: its `Display` namespace clashed with X11's `Display` type,
  which Qt's `QGuiApplication` header declares on Linux. The namespace is now `DisplayModel`. Checked with a
  Linux build in WSL (Ubuntu 26.04, Qt 6.10): it builds and the 12 tests pass. The floating bar hides its stats
  where there is no stats backend (Linux), instead of always saying "Game not running".
- **HelixSR 2.x is set up without its game step.** HelixSR 2.0's `helixsr-setup.ps1`, run unattended, goes on to
  install HelixSR Scaler into every game it finds with frame generation on, and its installer no longer has the
  functions the app's 1.x helper calls. The app no longer runs the `.ps1`: it reads the addresses and SHA-256 sums
  of Python, numpy and the shader compiler from it, downloads and checks them itself, and runs HelixSR's build
  script (`setup/helixsr_setup.py --yes --no-fg`; no frame generation, which only HelixSR Scaler uses). Games are
  installed into and removed from through HelixSR 2's JSON bridge (`setup/helixsr_bridge.py`), always as the FSR
  drop-in (`--way fsr`), one game at a time. 1.x packages already built keep using the old helper. HelixSR 3 and
  later are not offered until the app supports them. The system check no longer needs PowerShell.
- The HelixSR installer self-test left its throw-away folder behind (`helixsr/selftest`): the restored test DLL
  was still open when the folder was deleted.
- The system check's message for a card below shader model 6.6 no longer promises that a driver update
  fixes it: HelixSR's shaders are built for 6.6, and a card whose newest driver stops at 6.5 cannot run it.
- Windows: a Steam game that was already in the list (added by hand) was added a second time, because
  Steam writes its library path in lower case. Steam paths now use the case on disk.
- *Capture* no longer says "Snapshot saved" when no shot was written; it reports the failure.
- The HelixSR tab's buttons no longer overlap its game list when the window is short (larger fonts or
  display scaling): the window grows to fit the tab, including the extra lines of wrapped text, and
  stays on the screen.
- `tst_steamlibrary` failed on Windows (a test path without a drive letter).
- Launching a Steam game logs the `steam://` URL it opens.
- Linux: Proton games are found again. Their image path is on drive S: (the Steam folder), which is now
  translated through the Wine prefix; `/var/home` and `/home` paths compare equal on Fedora Atomic systems.
- Linux: *True Borderless FS* uses the window manager's full-screen state, as KWin will not resize Wine's
  fixed-size windows or drop their title bar; *Windowed* leaves it.
- Linux: *Focus* works under KWin (its focus-stealing prevention ignored the request).
- Linux: snapshots capture the game window; under Wayland the screen itself cannot be read.
- **OS access moved behind `PlatformBackend`** (`src/platform/`), the first step of a Linux port. Window
  listing, moving, styles, monitors, processes and screen capture now go through one interface;
  `Win32Backend` is the only implementation, and behaviour on Windows is unchanged. The downscaled
  view, the global hotkey and the Helix system probe are still Windows-only code.
- **Linux (X11 / XWayland) backend**, work in progress: the app builds on Linux and `X11Backend` lists,
  moves, resizes, strips the frame of, focuses and closes windows through xcb and the EWMH
  conventions; processes come from `/proc` (a Wine/Proton process is reported under its Windows image
  path). Not yet: the downscaled view, global hotkey, Layered / No-activate styles, launching through
  Steam/Proton. `tst_x11backend` tests the backend against a real xterm.
- `SteamMatch` (not used by the app yet): reads a Steam AppID from a window class or pasted text and
  decides whether a window is the target game, for Proton games whose process cannot be matched by path.
- New `tst_windowmanager` test runs `WindowManager` and `MonitorHelper` against a fake backend.

### Added
- **HelixSR tab.** Sets up [HelixSR](https://github.com/lonewolf0622/HelixSR) (an FSR 3.1 upscaler
  for AMD GPUs) without leaving the app:
  - downloads the newest release, checks its SHA-256, lets HelixSR's own setup build the network
    on this PC (about 5 to 6 minutes, with a progress bar and a running timer, cancellable), then
    deletes the downloads, the DLSS DLL and the build tools;
  - checks GitHub for a newer release at start-up (when HelixSR is set up) and on demand; the tab
    shows *HelixSR (update)* and offers to update games that still have the older DLL;
  - a list of games confirmed to have an AMD FSR 3.1 DLL (**Check selected game**) with
    **Install**, **Remove**, **Forget** and **Run with HelixSR** (installs when needed, reinstalls
    after a game update put its own DLL back, then starts the game). Only the FSR drop-in is supported.
- **System check for HelixSR** (**Check this PC**, and automatically before a set-up): lists the
  graphics adapters and tests Direct3D 12, shader model 6.6, 16-bit math and SIMD width, free disk
  space, `tar.exe` and PowerShell. A PC that cannot run HelixSR is stopped before the 5-minute
  build; an untested card (Vega, NVIDIA, Intel) gets a warning first.
- **Build number and date** in the window title (`WindowManager 0.1.3 - build 16 - 2026-10-19`) and
  in Help > About (with the commit; `+` marks uncommitted changes). The number is the CI run number,
  or the commit count for local builds.
- **HelixSR settings per game.** For a game with HelixSR installed, the tab edits `helixsr.ini`:
  sharpening (off / nvidia / rcas), strength (0 to 1) and wave size (auto / 64 / 32). Comments, other
  keys and line endings in the file are kept; a missing file is created.
- **Installer self-test.** After a build (and whenever this app's helper script changes) the install
  helper installs into and removes from a throw-away game folder with the built HelixSR. If HelixSR's
  installer has changed so that the helper no longer fits, installing into games is switched off with
  a clear message (and an **Open folder** button for installing by hand) instead of a Python error.
- Help page *HelixSR*, `tst_helix` (versions, releases, checksums, game detection, paths, system
  verdicts) and a `HelixConfirmed` flag per game in `windowmanager.json`.

### Changed
- Looking through a game's folder (list, **Check selected game**, **Run with HelixSR**, install and remove)
  now runs in the background, so a huge game folder no longer freezes the app.
- The automatic fit check after launching a game no longer opens the dialog when the window already
  fits the monitor exactly.

### Fixed
- A game can no longer be started twice. Launching refuses while a process runs the same exe, whichever
  button is used (Start, Run with HelixSR, Restart), and says so. **Run with HelixSR** used to open a
  second launcher this way. If a game (this one or another of your games) is running, a dialog offers
  to close it and start the new one. Restart waits for the old process to exit before it launches.
  For a `.bat`/`.cmd` game this works after its first launch: the game exe it started is remembered
  (`ChildExePath` in `windowmanager.json`), and Kill/Restart use it too.
- `CMakeLists.txt` was committed with unresolved merge-conflict markers (two copies of the
  script), so the project could not be configured.
