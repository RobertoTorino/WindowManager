# Safety

What the app may terminate, change and undo.

## Terminating games

- **Shutdown, Kill and Restart ask for confirmation** before terminating anything. Tick *Don't ask again* to
  stop the prompt; turn it back on under *View > Confirm before terminating a game*. **F2 twice** (while a game
  runs) asks for the second press instead.
- Termination is scoped to the session: the game you launched (and everything it started), the selected
  target by full path, and loader processes (for example TeknoParrot or Dolphin) that were started after your
  launch. A loader or emulator you started yourself beforehand is left alone.
- Shell and system processes (`explorer.exe`, `cmd.exe`, browsers, ...) are never terminated.

## Changes and how to undo them

| What | Undo |
|---|---|
| Window styles and positions of the game | **Reset All**, or the **Windowed** button for borderless changes. |
| The game's config files (**Game config** tab) | A copy of each is kept as `<file>.wm-original` before the first change; **Restore originals** puts the game's values back. |
| Resolution, orientation, display arrangement (**Display** tab) | Undone after 15 seconds unless you confirm it. |
| A game's display settings | Undone when the game closes, or at the next start if the app crashed. |
| HelixSR in a game | **Remove** puts the game's own DLL back. |
| Graphics card per game, power plan, power mode | Change them back on the **GPU & Power** tab (they are Windows' own settings). |

## Online games

Moving and resizing windows is what Windows itself does. The downscaled view only reads the window's
picture and sends mouse messages to it. A replaced DLL (HelixSR) may not be accepted by anti-cheat: use it at
your own risk.
