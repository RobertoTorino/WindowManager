# HelixSR

[HelixSR](https://github.com/lonewolf0622/HelixSR) replaces a game's AMD FSR 3.1 DLL with one that runs
NVIDIA's DLSS network as compute shaders, so it works on AMD GPUs (RDNA 1 and newer).

1. **Check this PC**: shader model 6.6 and the tools the setup needs.
2. **Set up HelixSR**: downloads and builds it on this PC (about 5 to 6 minutes).
3. Pick a game with an FSR 3.1 DLL and press **Run with HelixSR**; choose *AMD FSR* in the game.
   **Remove** puts the game's own DLL back.

- HelixSR 2.x is set up without its own game step: only the games you pick get it, always as the FSR drop-in.
- The built DLL contains NVIDIA's network: keep it on your PC.
- Anti-cheat may not accept a replaced DLL.

---

**More in the online wiki:** [HelixSR](https://robertotorino.github.io/WindowManager/wiki/helixsr/)
