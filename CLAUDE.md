# WindowManager

Windows-only Qt 6 / C++20 app for managing a game's or emulator's windows: borderless fullscreen, exact sizes, monitor moves, fit-to-monitor, downscaled view, snapshots. See [README.md](README.md) for user docs and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the code layout.

## Working rules

- **Never commit, stage or push.** The user does all git operations. Don't offer to either.
- Match the surrounding code: comment density, naming, idiom.
- Keep pure logic (no Win32, no widgets) separate and unit-tested, like `KillPlanner`, `FitCalculator` and `Supersample`.
- Update README.md and docs/ARCHITECTURE.md when files or user-visible behaviour change.

## Build and test

- Configure/build dir: `build/windows-clang-debug` (CMake + Ninja + `clang-cl`, Qt at `C:\Qt\6.11.1\msvc2022_64`).
- Build: `cmake --build build/windows-clang-debug`
- Tests (QtTest): `ctest --test-dir build/windows-clang-debug --output-on-failure`
- New source files must be added to `WINDOW_MANAGER_SOURCES` in [CMakeLists.txt](CMakeLists.txt).
- The app version lives in `APP_VERSION` in CMakeLists.txt. CI bumps it, so don't edit it by hand.
- The build number and date come from `cmake/BuildInfo.cmake`, which rewrites `build_stamp.h` in the build dir on every
  build (CI run number, else the commit count). `BuildInfo` ([src/core/BuildInfo.h](src/core/BuildInfo.h)) shows them in the title and About box.
- Keep [CHANGELOG.md](CHANGELOG.md) up to date under *Unreleased*.

### Compiler

The presets use a bare `clang-cl` from PATH. Use the VS 2026 one (Clang 22, `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin\clang-cl.exe`). The VS 2022 Clang 19 fails with `STL1000: expected Clang 20 or newer`. If the build dir was configured with the old one, remove `CMakeCache.txt` and `CMakeFiles/`, then:

`cmake --preset windows-clang-debug -DCMAKE_C_COMPILER=<path> -DCMAKE_CXX_COMPILER=<path>`

### Linux (Bazzite)

The host is immutable and has no toolchain. Build inside the `wm-dev` distrobox (Fedora 44 toolbox image, with
cmake, ninja, clang, gcc, qt6-qtbase-devel, qt6-qttools-devel, libX11/xcb devel and xterm). Prefix commands with
`distrobox enter wm-dev --`:

- Configure: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- Build: `cmake --build build/linux`
- Tests: `ctest --test-dir build/linux --output-on-failure` (11 tests; `tst_x11backend` runs for real against XWayland)
- Before testing a game: `scripts/free-memory.sh [--close]` (the machine has 9.4 GB of RAM next to 6 GB of VRAM).
  KDE asks before `xdotool` may send input; the user decides that prompt.

The desktop is KDE Plasma on Wayland. The X11 backend only sees XWayland windows.

## Logging

- `Logger` ([src/core/Logger.h](src/core/Logger.h)) writes to `windowmanager.log` next to the exe. While a game is selected (`setGame`), it also writes to `logs/<game>.log`. Both rotate at 1 MB to `*_old.log`.
- The controller switches the game log on target change. `publishStatus` logs the outcome of every user action (warning if the text contains "fail").
- **File > View logs...** (Ctrl+L) opens `LogViewerDialog`, with the app log and every per-game log.
- `Logger.cpp` is linked into `tst_appconfig`, so keep it free of Qt and widget dependencies.

## Help and docs

- In-app help (**Help > Help topics**, F1) is `HelpDialog`, fed by the markdown pages in `docs/help/`. They are embedded through `resources/resources.qrc` (alias `help/<name>.md`). A new page needs an entry in the qrc **and** in the `kTopics` list in [src/ui/HelpDialog.cpp](src/ui/HelpDialog.cpp).
- Pages link to each other as `topic.md`, so they work in the app, on GitHub and in MkDocs.
- `mkdocs.yml` builds the same pages as a site: `pip install mkdocs-material`, then `mkdocs serve` (output `site/` is git-ignored).
- When behaviour changes, update the matching help page as well as README.md.

## Status

- Per-game logging, the log viewer and the help window are implemented. The user tested them in the running app and they work.
- The repo build dir builds with Clang 22 and the 5 tests pass (Windows, before the Linux work).
- Linux (2026-10-19, Bazzite, GOD EATER 3 under Proton): Steam detection, launch through Steam, scan, true
  borderless full screen and back, focus, snapshot, start-while-running guard and Shutdown all work.
  TEKKEN 7 and Split/Second do not start: their Steam launch options run a missing
  `bc250-bazzite-test` wrapper (the user's Steam setting, not this app).
- Display tab (2026-10-19, Windows only): built, 10 tests pass, `tst_display` reads this PC's displays
  correctly (laptop panel 1920x1080, 125 %, brightness 100 %, no HDR). **Not tried in the running app yet:**
  changing a setting, the 15 s keep-or-revert box, DDC/CI on an external monitor, HDR, a game's settings
  being applied on Start and restored on exit. The scale uses an undocumented DisplayConfig call.
- Game stats bar (2026-10-20, Windows only): built, 11 tests pass, `tst_stats` reads this laptop (both adapters
  with their LUIDs, CPU, RAM, on battery). FPS needs admin: **not tried yet** elevated, nor with a running game
  (GPU %, adapter name, the warnings, the log lines). Made after Silent Hill f ran slowly: the laptop was on
  battery (unplugged at 00:12, game started 00:28); the app's logs showed nothing wrong.
- GPU & Power tab, driver check, power plans, floating bar (2026-10-20, Windows only): built, 11 tests pass,
  screenshots checked; the live NVIDIA lookup says 617.42 is current; minimize -> floating bar -> Open -> exit
  works (UI Automation). After the NVIDIA driver update (01:21) Windows maps *Let Windows decide* to the Intel
  UHD (before: GTX 1650), so games without a setting may render on Intel. **Not tried yet:** changing the GPU
  setting / power plan / power mode from the tab, the floating bar over a running game, the right-click Exit.
  The user is now in Performance Log Users (FPS without admin after signing in again; not seen yet).
  The user has a second monitor attached (may be used for tests).
- Linux build check from this PC (2026-10-20): WSL `Ubuntu-26.04` has cmake, ninja, g++ and Qt 6.10; build dir
  `~/wm-build` (source `/mnt/c/development/WindowManager`): `wsl.exe -d Ubuntu-26.04 -- bash -lc 'cd ~/wm-build &&
  cmake --build . -j16 && QT_QPA_PLATFORM=offscreen ctest --output-on-failure'`. Builds, 13 tests pass. It found
  the `Display` namespace clash with Xlib (now `DisplayModel`): never name anything `Display`, `Window`, `Screen`,
  `Status`, `Bool` or `None` at namespace scope. Next Linux steps: docs/LINUX_PORT.md, *Next steps*.
- Tekken 7 (2026-10-20): game watch (`Watch` lines) and the view's layout lines; the user's session showed two app
  copies running, the overlay at Qt's 640x480 on monitor 2 (tiny picture), and Tekken resetting its window to
  1280x720 on every focus. Fixed: one instance (QLocalServer), overlay pinned (WM_WINDOWPOSCHANGING),
  `Supersample::ReassertPolicy`, Ctrl+Alt+M releases the mouse, the app keeps off the game's monitor, the integrated
  GPU warning waits for 10 % GPU. Checked in a scratch copy with charmap (single instance from the floating bar,
  overlay 1920x1080 on monitor 2, app moved off monitor 2). **Not tried yet:** Ctrl+Alt+M against a game holding
  the mouse, all of it with Tekken. Tekken here is the arcade build started through JConfig: its *DX Window Mode*
  settings (resolution 1280x720, window 1280x720 at 16,16) are what resets the window; the user should set them to
  the monitor (or a higher DX resolution with a 1920x1080 window to supersample).
- Window list (2026-10-20): follows the game by itself (`refreshWindows`: watch events + every 3 s, quiet), the
  active row is green / bold / "Active". Checked in the scratch copy with charmap (row moved 100,100 -> 400,300
  without Scan).
- Ideas, not started: a toolbar button for the log viewer (there is no settings dialog), more detailed logging inside `WindowManager.cpp`, a GitHub Pages workflow for the MkDocs site.

## Where we left off (2026-10-20)

Open work, in this order. Windows: 14 tests, Linux (WSL): 15 tests, all passing; nothing committed by Claude.

1. **Tekken 7 test by the user, with the new Game config tab and focus switch** (built 2026-10-20). Tekken here is
   the arcade build (TEKKEN 7 Fated Retribution R2, `.../TEKKEN_7_FATED_RETRIBUTION_ROUND_2`) started through
   **JConfig**. Findings: no `Saved/` folder, so the game reads the loose `TekkenGame/Config/Default*.ini` and
   `Engine/Config/**`; `Engine/Config/Windows/WindowsEngine.ini` has `r.setres=1280x720`; JConfig keeps its
   settings in `Binaries/Win64/monitor_misc.bin` (244 bytes, int32: 60/64 window pos, 68/72 window size,
   76/80 DX resolution; confirmed by the user, whose JConfig showed window 1280x720 at 16,16 and DX 1920x1080).
   In window mode the window follows the DX resolution, so in-engine supersampling = `r.ScreenPercentage`.
   **Checked in a scratch copy only** (fake Tekken folder with copies of the user's files, charmap as the game):
   Write now / Restore originals (files back byte for byte, copies removed), config written on Start before the
   launch, F1 game -> app -> game with the label following.
   **Focus (2026-10-20, user's first Tekken run):** F1 brought the app up for a moment, then Tekken took the
   foreground back (the watch did not even log it). Reproduced with a test program that attaches to the foreground
   thread's input (scratch `stealer.exe`): it gets past `LockSetForegroundWindow` and past `EnableWindow(FALSE)` on
   its window. Now: F1 locks the foreground, checks after 0.5 s and logs who took it; if taken back, the app goes
   **mouse only** (`WS_EX_NOACTIVATE`): real clicks on its buttons work while the stealer keeps the foreground
   (checked). **F2 twice within 2 s** terminates the game (checked with the stealer). The fit-check box after Start
   is modal and blocks clicks on the main window: close it first. **View > Window opacity** (100-40 %, by default
   only while a game runs) for one monitor: checked in the scratch copy (layered alpha 60 % while the stand-in ran,
   solid before and after). Fixed: the fit check turned off came back on at every start. **Not tried yet with Tekken:** mouse-only clicks,
   F2, whether the game reads
   the edited files (render scale 50 % is the visible test), whether the JConfig window stops the 1280x720
   resets, F1 while Tekken holds the focus (JConfig's input emulator might swallow keys), and from the earlier
   list: Ctrl+Alt+M with the mouse held, FPS in the stats bar (Performance Log Users), one instance only.
   Suggested settings: Resolution 1920 x 1080, JConfig window *Monitor 2* (0,-1080), Render scale 150 %.
2. **Game session reports** (decided with the user): a **tab first** (a separate window maybe later); per session
   one **JSON** (`reports/<game>/<date-time>.json`, `schemaVersion`), a row in **`reports/sessions.csv`**, and a
   **per-second samples CSV** (for charts later); "where it was played" is a **label the user chooses** (no PC
   name). Content: game (name, exe name, Steam AppID), start / end / time played, app version, CPU / RAM / GPUs +
   drivers / Windows version, monitor (resolution, refresh, scale, HDR), game window (monitor, rect, mode,
   downscale factor), GPU setting, power plan / mode / share on battery, FPS avg / 1 % / 0.1 % lows, frame-time
   percentiles, CPU / GPU avg + peak, RAM / VRAM peak, the card it rendered on, warnings with durations, app actions
   with timestamps. **No private data:** no user names or paths with the user folder, no PC name, IP, MAC or
   monitor serials. Phases: (1) recorder + files, pure parts tested; (2) the tab (list, details); (3) compare two
   sessions + a self-drawn FPS chart (no Qt Charts).
3. **More global hotkeys** only if the user asks: the user chose a focus switch (F1, done). Tekken keeps the focus,
   so the app's commands can't be reached while it runs (in Nexus it was the reverse: a button gave the focus back
   to the game, which ignores the controller without it).
4. **Game config for more games**: other UE4 games with loose config; packed / user-folder configs
   (`%LOCALAPPDATA%/<Project>/Saved/Config/WindowsNoEditor/*.ini`) are not handled yet.
5. **Linux** on the Bazzite machine: docs/LINUX_PORT.md, *Next steps* (stats backend, floating bar under KWin,
   AppImage).

**Testing without touching the user's setup:** copy `WindowManager.exe`, the Qt DLLs and the plugin folders
(`generic iconengines imageformats networkinformation platforms styles tls`) from the build dir to a temporary folder
with its own `windowmanager.json` (a game entry for `C:/Windows/System32/charmap.exe` as `lastExePath`). The single
instance is keyed by the install folder, so that copy runs next to the user's. Drive it with UI Automation and
screenshot it with `PrintWindow`; read its own `windowmanager.log`. A foreground test window takes keyboard focus,
so keystrokes the user types may land in it. The user's second monitor (1920x1080 at 0,-1080, 100 %) may be used.
If the user's own app blocks the link, close it with `CloseMainWindow` first (it saves its settings).

## Bugs found on Linux that also affect Windows

Found while testing on Bazzite. Each is fixed in shared code (one source tree for both platforms; the
`linux-port` branch was merged into `main` on 2026-10-19 and removed). Checked on Windows on 2026-10-19:
it builds with Clang 22 and the 9 tests pass.

- **checked on Windows:** with no target, the game dropdown showed the first game as if selected.
  Fix: `GameBar::setGames` selects nothing. Started with an empty `lastExePath`: the dropdown is empty.
- **not checked on Windows:** *Capture* said "Snapshot saved to ..." even when no shot was written. Fix:
  `SnapshotCapture` counts saved shots and reports a failure. Needs a running game to check.
- **checked on Windows:** the HelixSR tab's widgets overlapped (the buttons over the game list). The
  window's minimum ignores the extra lines of word-wrapped labels, so the first fix still overlapped
  at 150 %. Fix: `MainWindow` caps the tabs at the page's height for its width, grows the window to
  give them that (no taller than the screen) and moves it up if it would run off. Checked with
  `QT_SCALE_FACTOR` 1, 1.25 and 1.5 on a 1920x1080 screen.
- **checked by tests:** `AppConfig::load()` kept the previous games when the config file did not exist.
- **not checked on Windows:** launching a Steam game logs the `steam://` URL. Needs a game launch.
- **found and fixed on Windows:** Steam writes its library path in lower case on Windows, so the sync
  added a second entry for a game already added by hand. `SteamLibrary` now uses the case on disk
  (`tst_steamlibrary` `pathsHaveTheCaseOnDisk`).
- **found and fixed on Windows:** `tst_steamlibrary` failed on Windows (`/nowhere/...` is stored as
  `C:/nowhere/...`).

## HelixSR 2.0.0 (released 2026-10-19)

The app sets up and updates HelixSR 1.x only (`Helix::kSupportedMajor`). In 2.0.0, `-Yes` also answers
the setup's game questions, so it would install HelixSR Scaler into every game it finds with frame
generation on. Its installer API changed too: `install_list` and `uninstall_safe` are gone, and
`games_from` moved to `helixsr_games.py`, so `resources/helixsr_driver.py` no longer fits. Supporting
2.x means running HelixSR's `helixsr_setup.py` without the game step, and porting the driver to the new
installer (the FSR DLL now lives in `HelixSR-fsrfg/`). Shader model 6.6 is still required (`cs_6_6`).
Problems, evidence and the step-by-step plan (through 2.0's JSON `helixsr_bridge.py`): [docs/HELIXSR_2.md](docs/HELIXSR_2.md).
