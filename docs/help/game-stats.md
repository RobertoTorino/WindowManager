# Game stats, GPU and power

The bar above the status bar shows the running game's **FPS** (and 1 % low), **CPU**, **GPU** and the card it
renders on, **RAM**, **VRAM**, and plugged in / on battery. Windows only.

- An **orange warning** names what slows the game down: on battery, battery saver, power-saving mode, the
  integrated GPU, the CPU fully used. Hover over it for what to do.
- **FPS needs administrator rights**, or your account in *Performance Log Users* once (PowerShell as
  administrator, then sign out and in):
  `Add-LocalGroupMember -SID S-1-5-32-559 -Member <your name>`
- **Minimise** the app and it becomes a small always-on-top **floating bar** (Ctrl+B).
- **GPU & Power tab:** set games to **High performance** (a driver update can change what *Let Windows
  decide* picks), check for NVIDIA driver updates, switch the power plan and mode.
- The game log gets a stats summary every minute.

---

**More in the online wiki:** [Game stats](https://robertotorino.github.io/WindowManager/wiki/game-stats/) [GPU and power](https://robertotorino.github.io/WindowManager/wiki/gpu-and-power/)
