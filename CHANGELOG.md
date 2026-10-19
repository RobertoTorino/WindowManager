# Changelog

Notable changes to WindowManager. The version number lives in `APP_VERSION` in `CMakeLists.txt`
(CI bumps it); the build number and date are shown in the window title and Help > About.

## Unreleased

### Added
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

### Changed
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
- **HelixSR 2.0.0 is not set up.** Its setup, run unattended as this app does, goes on to install HelixSR
  Scaler into every game it finds and turns on frame generation, and its installer no longer has the
  functions the app's install helper calls. The app now sets up and updates HelixSR 1.x only (1.9.3) and
  the tab says when a newer version is out that it does not support yet.
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
