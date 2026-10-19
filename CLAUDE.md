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

### Linux (Bazzite, `linux-port` branch)

The host is immutable and has no toolchain. Build inside the `wm-dev` distrobox (Fedora 44 toolbox image, with
cmake, ninja, clang, gcc, qt6-qtbase-devel, qt6-qttools-devel, libX11/xcb devel and xterm). Prefix commands with
`distrobox enter wm-dev --`:

- Configure: `cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Debug`
- Build: `cmake --build build/linux`
- Tests: `ctest --test-dir build/linux --output-on-failure` (10 tests; `tst_x11backend` runs for real against XWayland)
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
- Ideas, not started: a toolbar button for the log viewer (there is no settings dialog), more detailed logging inside `WindowManager.cpp`, a GitHub Pages workflow for the MkDocs site.

## Bugs found on Linux that also affect Windows

Found while testing on Bazzite. Each is fixed in shared code on `linux-port` (there is one source tree;
`main` has the released Windows app without the Linux work). Checked on Windows on 2026-10-19: the
`linux-port` branch builds with Clang 22 and the 9 tests pass.

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
