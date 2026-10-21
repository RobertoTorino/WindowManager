# Game info overlay, GPU and power

While a game runs, a see-through **game info overlay** in a corner of the game's monitor shows:

- **Scaler** and **Picture**: Magpie (and its effect), the downscaled view or none, from what size to what size,
  the scale and *upscaling* / *downscaling*; *paused* while another window is in front, *not scaling* with the
  reason.
- The resolution and **render scale** in the game's own files (when the Game config tab changed them), the
  game's **window** and **monitor**.
- **FPS** (and 1 % low), **CPU** / **GPU** and the card it renders on, **RAM** / **VRAM**, plugged in / on battery.
- **Orange warnings** with what to do: a window bigger than its monitor (Magpie then shrinks it), Magpie not
  scaling, on battery, battery saver, power-saving mode, the integrated GPU, the CPU fully used.

**Ctrl+Alt+I** shows or hides it (also while the game has the keyboard); the corner is under *View > Game info
overlay*. Clicks go through it, and it is left out of screenshots and snapshots. The **status bar** has the
scaler line at all times. Windows only.

- **FPS needs administrator rights**, or your account in *Performance Log Users* once (PowerShell as
  administrator, then sign out and in):
  `Add-LocalGroupMember -SID S-1-5-32-559 -Member <your name>`
- **Minimise** the app and it becomes a small always-on-top **floating bar** (Ctrl+B).
- **GPU & Power tab:** set games to **High performance** (a driver update can change what *Let Windows
  decide* picks), check for NVIDIA driver updates, switch the power plan and mode.
- The game log gets a stats summary every minute, and a `[Picture]` line when the scaling changes.

---

**More in the online wiki:** [Game info overlay](https://robertotorino.github.io/WindowManager/wiki/game-stats/) [GPU and power](https://robertotorino.github.io/WindowManager/wiki/gpu-and-power/)
