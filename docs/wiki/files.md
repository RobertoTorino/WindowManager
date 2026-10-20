# Files and settings

Everything is kept next to `WindowManager.exe`. Nothing goes in the registry.

| File | Contents |
|---|---|
| `windowmanager.json` | Saved games (with each game's display settings and game config), the last target and per-game window profiles. Written atomically. *File > Open config file* opens it. |
| `windowmanager.ini` | App window size/position, Always on top, the terminate-confirmation preference, the automatic fit check on/off, the game stats bar shown or hidden, the floating bar (on / off, position), the window opacity and the focus switch and terminate hotkeys. While a game's display settings are on, also how to undo them. |
| `windowmanager.log`, `windowmanager_old.log` | Rolling application log (the old one is the previous file after rotation at 1 MB). |
| `logs/<game>.log`, `logs/<game>_old.log` | Rolling log per game. View all logs in the app with **File > View logs...** (Ctrl+L). |
| `snapshots/<game id>/` | Screenshots from the Capture tab. |
| `helixsr/` | The built HelixSR (`HelixSR-<version>/`), the install helper script, and downloads while a set-up runs. HelixSR's Python lives in `%LOCALAPPDATA%\HelixSR`. |

Outside that folder the app only writes where you ask it to:

- **Game config** edits the game's own config files and keeps `<file>.wm-original` copies next to them. See
  [Game config](game-config.md#which-files-and-keys).
- **HelixSR** replaces a game's FSR DLL and keeps the original as `*.original.dll`, and writes the game's
  `helixsr.ini`. See [HelixSR](helixsr.md).
- **GPU & Power** changes Windows' own graphics setting per exe and the power plan / mode. See
  [GPU and power](gpu-and-power.md).

To start completely fresh, close the app and delete the files you want to reset.
