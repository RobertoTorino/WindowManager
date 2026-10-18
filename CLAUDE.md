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

### Compiler

The presets use a bare `clang-cl` from PATH. Use the VS 2026 one (Clang 22, `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin\clang-cl.exe`). The VS 2022 Clang 19 fails with `STL1000: expected Clang 20 or newer`. If the build dir was configured with the old one, remove `CMakeCache.txt` and `CMakeFiles/`, then:

`cmake --preset windows-clang-debug -DCMAKE_C_COMPILER=<path> -DCMAKE_CXX_COMPILER=<path>`

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
- The repo build dir builds with Clang 22 and the 5 tests pass.
- Ideas, not started: a toolbar button for the log viewer (there is no settings dialog), more detailed logging inside `WindowManager.cpp`, a GitHub Pages workflow for the MkDocs site.
