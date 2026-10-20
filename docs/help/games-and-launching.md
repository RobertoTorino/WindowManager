# Games and launching

- **Dropdown:** your saved games; picking one makes it the *target* that every button acts on.
- **Add game...** registers an `.exe`, `.bat` or `.cmd`. Adding the same path again renames it.
- **Steam games** are added by themselves (and with *File > Find Steam games*) and start through Steam, so
  Proton, DRM and the overlay work. Wrong exe picked? **Add game...** on the right one.
- **Batch files:** the app finds the real game process the script starts (up to 15 seconds). *Batch scan timed
  out*: start the game yourself and press **Scan**.
- **Start** never starts a game twice. **Restart** and **Kill** ask first (*View > Confirm before terminating a
  game*).
- Termination is limited to this session: the game you launched, the target by full path, and loaders started
  after your launch. System processes are never terminated.

---

**More in the online wiki:** [Games and launching](https://robertotorino.github.io/WindowManager/wiki/games-and-launching/) [Safety](https://robertotorino.github.io/WindowManager/wiki/safety/)
