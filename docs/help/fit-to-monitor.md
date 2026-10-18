# Fit to monitor

Makes the game's *picture* exactly fill a monitor, so you do not have to nudge the
position and grow or shrink the width and height by hand.

## When it runs

After **Start**, the app waits for the game's real window (helper windows such as input
method or console windows, and hidden or tiny windows, are ignored) and for it to keep the
same position and size for about two seconds. Games often rebuild their window while the
graphics engine starts, so measuring earlier would be wrong. Then the popup opens.

You can open it at any time with **Fit to monitor...** on the Window tab,
*View > Fit window to monitor...* or Ctrl+F. If no window appears within two minutes the
check is cancelled.

Turn the automatic popup off with *Don't open this automatically...* in the popup or under
*View > Check window fit after launching a game*.

## The popup

| Section | Meaning |
|---|---|
| Target monitor / Fit to | Which monitor, and whether to fill the *full monitor* (true borderless) or the *work area* (taskbar stays visible). Defaults to the monitor the game is on. |
| Remove title bar and borders first | Removes the frame so the whole window is the picture. Recommended. |
| Measured | Window rectangle, picture (client area) rectangle and target, plus the frame around the picture. |
| Correction | The exact change to apply: position X and Y, width and height. |

Changing the monitor or mode recalculates the correction at once.

## Applying

**Apply correction** moves and resizes the window, then measures again. Some games nudge
their window after a move, so it corrects up to three times and then reports honestly:
either *Picture fits monitor N exactly* or how far off it still is. When the result is
borderless, the position is also saved, so **Force saved settings** can restore it.

## Good to know

- Keeping the title bar puts the bar outside the monitor. Next to another monitor, part of
  the window lands there and Windows may rescale it, so the fit can drift.
- A maximized window is restored first, because Windows ignores positions for it.
- Fitting only changes the window. It does not change the game's render resolution; for
  that see [Downscaled view](downscaled-view.md).
