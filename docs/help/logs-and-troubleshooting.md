# Logs and troubleshooting

## Logs

The app keeps a log for the program and one for every game, so you can see what happened
with a specific game without wading through the rest.

| File (next to the program) | Contents |
|---|---|
| `windowmanager.log` | Everything: start-up, config, monitors, and all games. |
| `logs/<game>.log` | Only the entries made while that game was the selected target. |

Both roll over at 1 MB: the full file is renamed to `*_old.log` and a new one starts.

**Viewing:** *File > View logs...* (Ctrl+L). Pick the application log or a game from the
dropdown; it opens on the current game. **Refresh** reloads, **Open folder** shows the
files in Explorer. Newest entries are at the bottom.

Each line is `[time] [level] [source] message`. Every action you take is logged with its
result; a result that failed is logged as `WARN`. Start with the last `WARN` or `ERROR`
lines when something goes wrong.

## The game's windows, focus and mouse

Some games fight over their window, the focus or the mouse: a start screen in another size than the
game, a picture that jumps, a mouse that stays locked in the game. While a game runs (and the stats bar
or the floating bar is shown), the app writes what happens to the game log, under the source `Watch`.
Windows only.

| Line | Meaning |
|---|---|
| `Game window 0x… 'TEKKEN 7' (UnrealWindow): window 1280x720 at 320,180, drawn area …, borderless` | A window of the game was shown for the first time. Windows that never show (input method, tooltips) are left out. |
| `Game window 0x…: window 1280x720 at 320,180 -> 1920x1080 at 0,0` | The window changed size or place (read once it has settled for 0.3 s). Also *hidden* / *shown*, *frame added* / *removed*, a new title. |
| `Game window 0x… closed` | The window was closed. |
| `Foreground: TekkenGame-Win64-Shipping.exe 'TEKKEN 7' (the game)` | Which window has the keyboard now: *the game*, *this app*, or *another program* (only its exe name: other programs' titles can be private). |
| `Mouse: held inside 1920x1080 at 0,0` / `Mouse: free` | The game locked the mouse inside that area, or let it go. |

The downscaled view logs its own layout under `ScaledView`: the monitor, the black overlay and its display
scale, the game window, the area it draws, and where the picture goes. It writes a line when any of
these changes, when it puts a game window back that the game resized, and when it gives up doing so.
A line ending in *THE OVERLAY DOES NOT COVER THE MONITOR* means the picture is drawn bigger or smaller
than the screen. It also logs when it brings the game to the front.

## Common problems

| Symptom | What to try |
|---|---|
| Table is empty after Start | The game has not created its window yet or runs under another process. Press **Scan**. Check the game log for *no matching windows*. |
| *Batch scan timed out* | The batch file did not start a new process within 15 seconds. Start the game, then press **Scan**. |
| Buttons do nothing to the game | The game runs as administrator. Run WindowManager as administrator too. |
| Window jumps back after moving it | The game repositions itself. Turn on **Stability**. |
| Fit is a few pixels off | The game resizes its window after being moved. The popup reports how far off; apply again, and remove the title bar and borders first. |
| Downscaled view looks the same | The game stretches a fixed-size picture instead of re-rendering at the window size. |
| Game runs slowly | Look at the [game stats bar](game-stats.md): an orange warning names the usual causes (on battery, integrated GPU, power-saving mode). The game log has a stats summary every minute. The two most common causes on a laptop: the charger is not plugged in (the CPU and GPU then run at a fraction of their speed), or the game renders on the integrated GPU. Set the game to *High performance* on the **GPU & Power** tab, especially after a graphics driver update, which can change what *Let Windows decide* picks. |
| Stats bar shows *FPS needs admin* | Recording frames needs administrator rights. Add your account to the Performance Log Users group once, then sign out and in: steps in [Adding your account to Performance Log Users](game-stats.md#adding-your-account-to-performance-log-users). |
| Mouse stays locked in the game | Many games (Unreal Engine ones such as Tekken 7) hold the mouse inside their window while they have the focus. Press **Ctrl+Alt+M**: the mouse is free and this app is in front. Click the game to give it the mouse back. The `Foreground` and `Mouse` lines in the game log show who has the focus and when the mouse is held. |
| Game window goes back to a small size when it gets the focus | The game, or its loader, applies its own window settings again. Set them to the monitor's size and position in the game's own options. For arcade games started through **JConfig** (Tekken 7 Fated Retribution) these are *DX Resolution Width / Height*, *Window Position X / Y* and *Window Width / Height* with *DX Window Mode* ticked: JConfig's Direct3D wrapper places the window itself. The `Watch` lines show each change. |
| A button does nothing to the game | The active window (the green row) may be a helper window of the game. Click the game's real window in the table, or press **Scan**. |
| Downscaled view shows only part of the picture | Check the `ScaledView layout` lines in the game log: the overlay size against the monitor, and the game window against the drawn area. |
| Game was fine before, now slow | Check the same two causes first. A driver update can switch a game to the integrated GPU without warning. The game log's `[Stats]` lines show which card it rendered on and whether the PC was on battery. |
| Fit popup does not open by itself | *View > Check window fit after launching a game* is off, or no window appeared within two minutes. Use Ctrl+F. |

## Files

| File | Contents |
|---|---|
| `windowmanager.json` | Saved games, the last target, per-game window profiles. |
| `windowmanager.ini` | App window position, Always on top, terminate prompt, automatic fit check, stats bar shown, floating bar on / position. |
| `windowmanager.log`, `logs/` | Logs, see above. |
| `snapshots/<game id>/` | Screenshots from the Capture tab. |

To start completely fresh, close the app and delete the files you want to reset.
