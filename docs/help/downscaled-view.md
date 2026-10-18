# Downscaled view

A sharper picture without anything falling off the screen. The game window is made large
(for example 2x the monitor, so the game renders 4 times as many pixels) and kept out of
sight, while a full-screen overlay on the monitor shows a live, scaled-down copy.

## Using it (Downscale tab)

1. Select the game window first (Scan).
2. **Show on:** the monitor that shows the picture.
3. **Sharpness:** 1.5x, 2x, 3x or 4x. The line below says exactly what will happen.
4. **Picture shape:** the game's current shape, the monitor's, or 4:3 / 16:9 / 16:10.
   A mismatch gets black bars, never stretching.
5. **Forward the mouse to the game** (on by default).
6. **Start downscaled view.**

**Stop** with the Stop button, **Ctrl+Alt+D** (works from anywhere), *View > Stop downscaled
view*, by closing the game, or by closing this app. The game window is restored exactly
as it was.

## What to expect

- It only adds detail if the game re-creates its picture at the window size (RPCS3 and most
  GPU-rendered emulators do). If it renders at a fixed size and stretches, nothing improves.
- Keyboard and gamepad keep going to the game.
- Mouse forwarding works for games that read the mouse from window messages (Qt, SDL, GLFW).
  Games using raw input or DirectInput cannot see it: untick the option for those.
- While running, the game window loses its borders and is parked behind the overlay. This
  app stays above the overlay so you can still use it.
- **Stability** is switched off when the view starts.
- It costs GPU power and video memory. Render sizes above 16384 pixels are refused.
- Windowed and borderless games only. If the game runs as administrator, run this app as
  administrator too.
