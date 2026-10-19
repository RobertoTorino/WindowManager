# Games and launching

## The game bar

| Control | What it does |
|---|---|
| Dropdown | Your saved games. Type to filter. Picking one makes it the target. |
| **Add game...** | Register an `.exe`, `.bat` or `.cmd` and name it. Adding an existing path lets you rename it. |
| **File > Find Steam games** | Look through the Steam libraries again (this also happens at every start). |
| **Start** | Launch the selected game. If a game is already running, asks whether to close it first (a game is never started twice). |
| **Restart** | Terminate it, then launch it again. |
| **Kill** | Terminate it. |

The selected game is the *target*: every button acts on the windows of that program. It is
shown at the right of the status bar and remembered for the next start.

## Steam games

Every game installed through Steam is added to the dropdown by itself, at start-up and with
**File > Find Steam games**. The app reads Steam's library files (`libraryfolders.vdf` and the
`appmanifest_*.acf` files). Proton, the Steam Linux Runtime and the Steamworks redistributables are
left out, and so are games that are still downloading.

- The target is the game's largest executable that is not a crash reporter, installer or anti-cheat
  helper. For Unreal Engine games that is the `...-Shipping.exe`, which owns the window. If the app picks
  the wrong one, use **Add game...** on the right exe: the game keeps its Steam launch.
- New games get Steam's name. Rename one with **Add game...** on the same exe; the name is kept.
- A game you uninstall is taken off the list, but its saved position is kept. Games on a drive that
  is not connected stay on the list.

## Starting a game

- **Steam games** are started through Steam (`steam://rungameid/<AppID>`), so Proton, DRM and the
  overlay work as usual.
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
