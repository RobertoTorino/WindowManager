# Building WindowManager

## Prerequisites

- [CMake 3.21+](https://cmake.org/download/) - `C:/Program Files/CMake/bin/cmake.exe`
- [Qt 6.11.1 MSVC 64-bit](https://www.qt.io/download) - `C:/Qt/6.11.1/msvc2022_64`
  (the *Qt Test* module is part of the standard install; it is needed for the tests)
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) - `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools`
- LLVM / clang-cl - available on PATH
- Ninja (ships with the VS Build Tools CMake integration)

If Qt is installed elsewhere or at another version, edit `CMAKE_PREFIX_PATH` and
`Qt6_DIR` in `CMakePresets.json` (and `WINDEPLOYQT` in `scripts/build.cmd`).

---

## Build (clean, self-contained)

Double-click **`scripts\build.cmd`**, or run it from any terminal:

```cmd
C:\development\WindowManager\scripts\build.cmd
```

The script will:
1. Load the MSVC environment (clang-cl needs it)
2. Delete the previous build output (guarantees nothing is stale)
3. Configure with CMake
4. Compile
5. Run `windeployqt6` to copy the Qt DLLs next to the exe
6. Open the output folder in Explorer

The runnable executable is:

```
build\windows-clang-debug\WindowManager.exe
```

### Qt DLLs are deployed on every build

CMake runs `windeployqt` as a post-build step, so the exe starts by double-click
without Qt's `bin` folder on `PATH`. If you ever see *"Qt6Widgetsd.dll was not found"*,
rebuild once so that step runs.

---

## Incremental build (after code changes)

From a VS Developer Command Prompt:

```cmd
cd C:\development\WindowManager
cmake --build build\windows-clang-debug --target WindowManager
```

Or from any prompt, load the environment first:

```cmd
call "%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64
cmake --preset windows-clang-debug
cmake --build build\windows-clang-debug
```

---

## Tests

Tests are built by default (turn off with `-DWINDOWMANAGER_BUILD_TESTS=OFF`).

```cmd
cmake --build build\windows-clang-debug
ctest --test-dir build\windows-clang-debug --output-on-failure
```

Five QtTest suites run: `tst_utilities`, `tst_killplanner`, `tst_fit`, `tst_supersample`, `tst_appconfig`.
Run one directly with `build\windows-clang-debug\tst_killplanner.exe`, or one case with
`tst_killplanner.exe unrelatedLoaderIsSpared`. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)
for what each covers.

---

## When to do a clean build

| Situation | Clean build needed? |
|-----------|-------------------|
| Edited `.cpp` / `.h` files | No |
| Added/removed a source file or changed `CMakeLists.txt` | Reconfigure only: `cmake --preset windows-clang-debug` |
| Changed Qt version or moved the project folder | Yes - delete `build\<preset>` (the cache stores absolute paths) |
| Ninja reports "no work to do" but the binary is stale | Yes - delete and reconfigure |
| Switched presets | No, each preset has its own folder |

---

## Available presets

| Preset | Compiler | Config |
|--------|----------|--------|
| `windows-clang-debug` | clang-cl | Debug |
| `windows-clang-release` | clang-cl | Release |
| `windows-msvc-debug` | cl.exe | Debug |
| `windows-msvc-release` | cl.exe | Release |

Build directory for each preset: `build\<preset-name>\`

---

## Versioning and releases

The version lives in one place: `set(APP_VERSION "x.y.z")` at the top of
`CMakeLists.txt`. It is shown in *Help > About* and read by the release workflow to
name the tag. The GitHub workflows install their own Qt (`.github/workflows/*.yml`),
which is set independently of your local install.
