# Games and launching

## The game bar

| Control | What it does |
|---|---|
| Dropdown | Your saved games. Type to filter. Picking one makes it the target. |
| **Add game...** | Register an `.exe`, `.bat` or `.cmd` and name it. Adding an existing path lets you rename it. |
| **Start** | Launch the selected game. |
| **Restart** | Terminate it, then launch it again. |
| **Kill** | Terminate it. |

The selected game is the *target*: every button acts on the windows of that program. It is
shown at the right of the status bar and remembered for the next start.

## Starting a game

- **Executables** are started directly.
- **Batch files** (`.bat`, `.cmd`) start a short-lived launcher. The app takes a snapshot of
  the running processes first, then looks for the new game process once a second for up to
  15 seconds and switches to it. If nothing is found, the status bar says
  *Batch scan timed out* and you can start the game yourself and press **Scan**.
- After a successful start the app waits for the game window and opens the
  [Fit to monitor](fit-to-monitor.md) popup (when that option is on in the View menu).

## Terminating

**Kill**, **Restart** and **Shutdown** ask first. Tick *Don't ask again* to stop the prompt;
turn it back on under *View > Confirm before terminating a game*.

Termination is limited to this session:

- the game you launched and everything it started,
- the selected target by full path,
- loader programs (for example TeknoParrot or Dolphin) started *after* your launch.

A loader or emulator you started yourself beforehand is left alone. Shell and system
processes (`explorer.exe`, `cmd.exe`, browsers, ...) are never terminated.

## What is remembered per game

Everything is stored in `windowmanager.json` next to the program: the game list, the last
selected game and each game's saved window position. Screenshots go to
`snapshots/<game id>/`, and each game has its own log. See
[Logs and troubleshooting](logs-and-troubleshooting.md).
You can open the config file with *File > Open config file*.
