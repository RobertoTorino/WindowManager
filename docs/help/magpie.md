# Magpie

[Magpie](https://github.com/Blinue/Magpie) scales a game's window up to the full monitor (FSR, Lanczos, CRT
and more). It works with any game, DirectX 11 or 12, Vulkan and emulators, and puts nothing into the game.

1. **Set up Magpie** on the *Magpie* tab: it downloads Magpie (about 10 MB), checks it and keeps the app's own
   copy in the `magpie` folder next to the program.
2. Select the game and choose an effect under **Selected game > Scale with** (*Off*: not scaled).
3. Run the game **windowed at the size it should render** and press **Start**. The app gives Magpie the game's
   window and starts it; Magpie fills the monitor while the game is in front, and closes when the game ends.

- The window must be **smaller than the monitor** (Magpie only scales up: a bigger one looks squeezed), and not
  maximized or borderless full screen (Magpie does not scale those). The fit check after Start is skipped for
  such a game, and *Stability* is turned off (moving the window restarts the scaling).
- **3D game mode** (for mouse-look games) keeps the mouse inside the game until Magpie closes.
- Only one Magpie can run at a time: close your own Magpie first.
- Worth it? The **Reports** tab compares sessions with and without Magpie, and **Compare picture** measures how
  much sharper it is than a plain stretch. The overlay shows Magpie's own GPU use (`+ Magpie 6 %`).

---

**More in the online wiki:** [Magpie](https://robertotorino.github.io/WindowManager/wiki/magpie/)
