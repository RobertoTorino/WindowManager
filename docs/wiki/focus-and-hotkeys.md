# Focus, mouse and hotkeys

Games often keep the keyboard and the mouse to themselves, and sit on the monitor the app is on. This page is
about reaching WindowManager while a game runs, and keeping it out of the game's way. Windows only.

## Switching the focus, and ending the game

A game like Tekken 7 keeps the keyboard, so the app's own shortcuts do not reach it while the game
is in front. The bar above the status bar shows who has the focus while a game runs: **Focus:
game**, **Focus: app** or **Focus: other**. Two keys work from anywhere while a game runs (they are
registered only then, so F1 still opens the help otherwise):

| Key | What it does |
|---|---|
| **F1** | Switches between the game and this app. The app asks Windows to keep the focus (other programs cannot take it with an ordinary call until you click elsewhere or press Alt). |
| **F2, twice within 2 seconds** | Terminates the game, like **Shutdown** (the game and the loader processes started for it). The first press beeps and asks for the second, so one slip of the finger does not end the game. |

**When the game takes the focus straight back.** Some games and loaders (Tekken 7 with JConfig)
grab the focus back at once, past Windows' lock. Then the app notices it within half a second and
switches to **mouse only**: clicking the app no longer takes the focus from the game, so its buttons
and lists work with the mouse while the game keeps the keyboard and the controller. The bar says
*this app: mouse only*. Typing into the app does not work in this mode; it ends when the game
closes. The *Fit window to monitor* box that opens after Start is a separate window: close it first,
because while it is open the main window does not take clicks.

Choose other keys under **View > Focus switch hotkey...** and **View > Terminate hotkey...** (for
example `Ctrl+Alt+G` or `Pause`; empty turns one off). The game log records every switch: who had
the focus, whether the app kept it, and who took it back. Windows only.

## Using the app over the game (one monitor)

With a single monitor the app has to sit on top of the game. **View > Window opacity** makes the
app's window see-through (90 % down to 40 %), so the game stays visible behind it. With **Only
while a game runs** ticked (the default) the window is see-through only while a game is running and
solid otherwise. Dialogs, like *Fit window to monitor*, stay solid.

It works best together with:

- **View > Always on top** (on by default), so the game does not cover the app;
- **mouse only** (see above), so clicking the app does not take the focus from the game;
- a smaller app window placed in a corner of the screen.

## Out of the game's way

- **One copy only.** WindowManager runs once: starting it again brings the running copy to the front (also out
  of the floating bar).
- **Off the game's monitor.** With *View > Keep this app off the game's monitor* (on by default) the app moves
  to the other monitor whenever the game's window, or the downscaled view, is on the monitor it is on.
  **Ctrl+M** moves it by hand.
- **The mouse.** **Ctrl+Alt+M** frees the mouse from a game that holds it (Unreal Engine games do while they
  have the focus) and brings the app to the front. Click the game to give it the mouse back.
- **The floating bar.** Minimising the app turns it into a small always-on-top bar with the game's name,
  FPS, CPU, GPU and the first warning. See [Game stats](game-stats.md#floating-bar).

The game log shows who had the focus and when the mouse was held (`Foreground` and `Mouse` lines); see
[Logs and troubleshooting](logs-and-troubleshooting.md#the-games-windows-focus-and-mouse).
