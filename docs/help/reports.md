# Reports

While a game runs, the app records once a second how it ran (FPS, 1 % low, CPU, GPU, Magpie's GPU, memory) and
what showed its picture: the **setup**, like `Magpie FSR 1600 × 900 → 1920 × 1080` or `No scaler 1920 × 1080`.

On the **Reports** tab, pick a game:

- **Sessions** (left): pick one to see it over time in **Session**: FPS above, GPU below (Magpie's dashed),
  coloured by setup. Hover for the values.
- **Setups**: the game's setups over all sessions: average FPS, 1 % low, GPU (game + Magpie). The same FPS
  with a lower GPU % means upscaling saves work; the same FPS and GPU % means the game was not GPU-bound.
- **Compare picture**: with Magpie scaling and the game on a still screen, it captures the game's own frame
  and Magpie's picture at the same moment, stretches the frame plainly (bilinear) for comparison, and scores
  the sharpness: *Magpie FSR is 15 % sharper than a plain stretch*, with a side-by-side zoom (**Pictures**).

Files: `reports/<game>/` next to the program (CSV per second, JSON per session and comparison, PNG), and
`reports/sessions.csv` for all games. No private data is stored. Windows only.

---

**More in the online wiki:** [Reports](https://robertotorino.github.io/WindowManager/wiki/reports/)
