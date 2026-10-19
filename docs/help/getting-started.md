# Getting started

WindowManager takes control of the windows of a game or emulator: true borderless
fullscreen, exact sizes, moving between monitors, keeping a window in place, a sharper
downscaled picture and screenshots.

It runs on Windows, and as a beta on Linux for Steam games under Proton (Bazzite, KDE Plasma).
See [Linux](linux.md) for what works there.

## The first five minutes

1. **Add a game.** Games installed through Steam are already in the dropdown. For anything else
   click **Add game...** (File > Add game..., Ctrl+O) and pick an `.exe`, `.bat` or `.cmd`. Give it a
   short name. It is saved in the dropdown for next time.
2. **Start it.** Click **Start**. For a batch file the app watches for the real game process
   the batch file spawns and switches to it by itself. Already running? Pick the game and
   press **Scan**.
3. **Let it fit.** Once the game window has settled, the *Fit window to monitor* popup
   opens. **Apply correction** makes the picture fill the monitor exactly. See
   [Fit to monitor](fit-to-monitor.md).
4. **Select the window.** Every window the game owns is listed in the table. The largest
   visible one is selected for you. Click another row to act on that one instead.
5. **Adjust.** Use the buttons and tabs. A **green** button means that state is on.
6. **Save the result.** On *Position & Size*, **Save position** remembers the window for
   this game and **Force saved settings** applies it again later.

## Where things are

| Area | What it is |
|---|---|
| Game bar | Saved games, Add game, Start, Restart, Kill. See [Games and launching](games-and-launching.md). |
| Action row | Scan, Monitor 1 / 2, Stability, Focus, Destroy, Reset All, Shutdown. |
| Window table | The windows of the selected game. |
| Tabs | Window, Position & Size, Presets, Downscale, Capture, HelixSR. See [Window controls](window-controls.md) and [HelixSR](helixsr.md). |
| Status bar | The result of your last action (left) and the target program (right). |

Every action is also written to the game's log. See [Logs and troubleshooting](logs-and-troubleshooting.md).

## Next

- [Games and launching](games-and-launching.md)
- [Window controls](window-controls.md)
- [Fit to monitor](fit-to-monitor.md)
- [Downscaled view](downscaled-view.md)
- [HelixSR](helixsr.md)
- [Linux (beta)](linux.md)
- [Logs and troubleshooting](logs-and-troubleshooting.md)
- [Shortcuts](shortcuts.md)
- [Reference](reference.md)
