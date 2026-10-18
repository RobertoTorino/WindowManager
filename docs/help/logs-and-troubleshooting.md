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

## Common problems

| Symptom | What to try |
|---|---|
| Table is empty after Start | The game has not created its window yet or runs under another process. Press **Scan**. Check the game log for *no matching windows*. |
| *Batch scan timed out* | The batch file did not start a new process within 15 seconds. Start the game, then press **Scan**. |
| Buttons do nothing to the game | The game runs as administrator. Run WindowManager as administrator too. |
| Window jumps back after moving it | The game repositions itself. Turn on **Stability**. |
| Fit is a few pixels off | The game resizes its window after being moved. The popup reports how far off; apply again, and remove the title bar and borders first. |
| Downscaled view looks the same | The game stretches a fixed-size picture instead of re-rendering at the window size. |
| Fit popup does not open by itself | *View > Check window fit after launching a game* is off, or no window appeared within two minutes. Use Ctrl+F. |

## Files

| File | Contents |
|---|---|
| `windowmanager.json` | Saved games, the last target, per-game window profiles. |
| `windowmanager.ini` | App window position, Always on top, terminate prompt, automatic fit check. |
| `windowmanager.log`, `logs/` | Logs, see above. |
| `snapshots/<game id>/` | Screenshots from the Capture tab. |

To start completely fresh, close the app and delete the files you want to reset.
