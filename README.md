[![CI Build and Release](https://github.com/RobertoTorino/WindowManager/actions/workflows/release.yml/badge.svg?branch=main)](https://github.com/RobertoTorino/WindowManager/actions/workflows/release.yml)
[![Docs](https://github.com/RobertoTorino/WindowManager/actions/workflows/docs.yml/badge.svg?branch=main)](https://robertotorino.github.io/WindowManager/)
[![Status: Beta](https://img.shields.io/badge/status-beta-orange.svg)](CHANGELOG.md)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](https://www.gnu.org/licenses/gpl-3.0)

<img src="resources/window-manager-icon.png" alt="WindowManager icon" width="96" align="right">

# WindowManager

A Windows tool for taming the windows of games and emulators. Built with C++20 and Qt 6.

**📖 Documentation: the [WindowManager wiki](https://robertotorino.github.io/WindowManager/)**

## What it is for

Games and emulators often open in a window that is the wrong size, sits on the wrong monitor, keeps a title
bar, or drifts after you move it. WindowManager fixes that, and keeps the PC set up the way the game needs it:

- **True borderless full screen**, exact sizes and presets up to 8K, moving a game between monitors, and a
  window that stays where you put it.
- **Fit to monitor:** after Start it measures the game's window and makes the picture fill the monitor to the
  pixel.
- **Downscaled view:** the game renders larger than the monitor and is shown scaled down, for a sharper
  picture.
- **Display settings per game:** resolution, refresh rate, HDR, scale and brightness, put back when the game
  closes.
- **Game stats:** FPS, CPU, GPU, RAM and VRAM, with warnings for what slows games down (on battery, the wrong
  graphics card, power-saving mode), and a **GPU & Power** tab to fix them.
- **Game config** for Unreal Engine 4 games with loose config files (Tekken 7's arcade builds), focus hotkeys
  for games that keep the keyboard, snapshots, and **HelixSR** set-up for AMD GPUs.

WindowManager is in **beta**. It also runs on Linux for Steam games under Proton (Bazzite, KDE Plasma), with
fewer features and no download yet: see [Linux](https://robertotorino.github.io/WindowManager/wiki/linux/).

## Getting started

1. Download `WindowManager-<version>.zip` from the
   [latest release](https://github.com/RobertoTorino/WindowManager/releases/latest) and unzip it into a folder
   of its own. There is no installer; settings and logs are kept next to the exe.
2. Start `WindowManager.exe`. Games installed through Steam are already in the dropdown; add others with
   **Add game...** (Ctrl+O).
3. Pick a game and press **Start**. When its window has settled, the *Fit window to monitor* popup opens:
   **Apply correction** makes the picture fill the monitor.
4. Adjust the window with the buttons and tabs, then **Save position** on *Position & Size* to keep it.

**F1** opens the help in the app; *Help > Online wiki* opens the full documentation. When something goes
wrong, *File > View logs...* (Ctrl+L) shows what happened.

## More

- [Wiki](https://robertotorino.github.io/WindowManager/): every feature in full, shortcuts, troubleshooting
- [Building and testing](docs/development/building.md) and [code layout](docs/ARCHITECTURE.md)
- [Changelog](CHANGELOG.md)

## License

GPL-3.0, see [LICENSE](LICENSE).
