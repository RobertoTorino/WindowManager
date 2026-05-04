# Building WindowManager

## Prerequisites

- [CMake 3.21+](https://cmake.org/download/) — installed at `C:/Program Files/CMake/bin/cmake.exe`
- [Qt 6.10.2 MSVC 64-bit](https://www.qt.io/download) — installed at `C:/Qt/6.10.2/msvc2022_64`
- [Visual Studio 2022 Build Tools](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) — installed at `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools`
- LLVM / clang-cl — available on PATH

---

## Build (clean, self-contained)

Double-click **`scripts\build.cmd`** — or run it from any terminal:

```cmd
C:\development\WindowManager\scripts\build.cmd
```

The script will:
1. Load the MSVC environment (clang-cl needs it)
2. Delete the previous build output (guarantees nothing is stale)
3. Configure with CMake
4. Compile
5. Run `windeployqt6` to copy all required Qt DLLs next to the exe
6. Open the output folder in Explorer

The finished, runnable executable will be at:

```
build\windows-clang-debug\WindowManager.exe
```

---

## Incremental build (after code changes)

As long as you are already in a VS Dev Command Prompt, you only need:

```cmd
cd C:\development\WindowManager
cmake --build build\windows-clang-debug --target WindowManager
```

---

## When to do a clean build

| Situation | Clean build needed? |
|-----------|-------------------|
| Edited `.cpp` / `.h` files | No |
| Changed `CMakeLists.txt` | Reconfigure only: `cmake --preset windows-clang-debug` |
| Ninja reports "no work to do" but binary is stale | Yes — delete and reconfigure |
| Switched presets | Yes |

---

## Available presets

| Preset | Compiler | Config |
|--------|----------|--------|
| `windows-clang-debug` | clang-cl | Debug |
| `windows-clang-release` | clang-cl | Release |
| `windows-msvc-debug` | cl.exe | Debug |
| `windows-msvc-release` | cl.exe | Release |

Build directory for each preset: `build\<preset-name>\`
