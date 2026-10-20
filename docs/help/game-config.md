# Game config

The **Game config** tab writes settings into the selected game's own config files on **Start** / **Restart**,
for Unreal Engine 4 games with loose config files (Tekken 7's arcade builds).

| Setting | What it does |
|---|---|
| **Render scale** | Above 100 % the game draws larger and scales down (sharper, slower); below 100 % faster but softer. |
| **Resolution** | The game's resolution in all its config places. |
| **JConfig window** | Tekken 7 arcade: the rectangle Tekken puts its window back to. Pick your monitor. |

- A copy of each file is kept as `*.wm-original`; **Restore originals** puts the game's values back.
- A running game keeps what it read at start-up: restart it to see a change.
- Quick test: render scale 50 % must look clearly blurry.

---

**More in the online wiki:** [Game config](https://robertotorino.github.io/WindowManager/wiki/game-config/)
