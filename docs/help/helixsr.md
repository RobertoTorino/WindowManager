# HelixSR (FSR 3.1 upscaler for AMD GPUs)

[HelixSR](https://github.com/lonewolf0622/HelixSR) is an unofficial, free upscaler for Direct3D 12
games. It replaces a game's AMD FSR 3.1 DLL with one that runs NVIDIA's DLSS network as plain
compute shaders, so it works on AMD GPUs (RDNA 1 and newer). The **HelixSR** tab sets it up for
you and puts it into your games. WindowManager does not change HelixSR itself; it runs HelixSR's
own setup.

## Does this PC qualify?

HelixSR runs as Direct3D 12 compute shaders. The line *This PC* at the top of the tab, and **Check this PC**,
test your graphics adapters (shader model 6.6, 16-bit math, SIMD width), the free disk space (about 1.5 GB
while setting up), `tar.exe` and PowerShell. The check also runs before every set-up.

| Result | Meaning |
|---|---|
| supported | An AMD RDNA 1 or newer card (RX 5000, 6000, 7000, 9000 series, recent Ryzen graphics) with everything above. |
| experimental | Works on paper but HelixSR was not tested on it: AMD Vega and older, NVIDIA (GTX 10 "should work"; GTX cards have no DLSS, so HelixSR is useful there; on RTX the game's own DLSS is better) or Intel. You are warned and may continue. |
| not supported | No Direct3D 12 device, shader model below 6.6 (update the graphics driver), too little disk space, or a missing tool. Set-up is stopped. |

## Set up HelixSR (once)

Press **Set up HelixSR**, read the notice and choose **Download and build**.

1. HelixSR is downloaded from its GitHub releases and checked against the published SHA-256.
   A file that does not match is deleted.
2. HelixSR's own setup downloads Python, a shader compiler and NVIDIA's DLSS 310.7.0 DLL, and
   builds the network on this PC. **This takes about 5 to 6 minutes.** A progress bar and a
   running timer show how far it is. **Cancel** stops it and removes what it made.
3. Afterwards the app deletes what is no longer needed: the downloaded DLSS DLL, the shader
   compiler, numpy, the archives and HelixSR Scaler's files. HelixSR and a small Python stay.

Everything is kept in the `helixsr` folder next to the program (and `%LOCALAPPDATA%\HelixSR` for
Python). The built DLL contains NVIDIA's network: it is for this PC, so do not share or upload it.

## Updates

If HelixSR is set up, the app checks GitHub for a newer release when it starts (one small request).
**Check for updates** does it on demand. When a newer one exists the tab is named
*HelixSR (update)* and the button reads *Update to x.y.z*. The update downloads and builds the new
version, removes the old one, and offers to update the games that still have the older DLL.

## Games with HelixSR

HelixSR works in games that ship AMD FSR 3.1 (a file named `amd_fidelityfx_dx12.dll` or
`amd_fidelityfx_upscaler_dx12.dll`).

1. Add the game with **Add game...** as usual and select it in the game bar.
2. On the HelixSR tab press **Check selected game**. If it has an FSR 3.1 DLL it joins the list;
   if not, you are told why.
3. Pick it in the list and press **Install**, or just **Run with HelixSR** (double-click does the
   same): it installs when needed and then starts the game like **Start** does.
4. In the game's graphics settings choose **AMD FSR** as the upscaler.

| Status in the list | Meaning |
|---|---|
| not installed | The game has an FSR 3.1 DLL; HelixSR is not in it yet. |
| HelixSR x.y.z installed | The game's FSR DLL is HelixSR's. "is available" means a newer build can be installed. |
| the game put its own DLL back | A game update or "verify files" restored the original. Install again (Run does it for you). |
| no AMD FSR 3.1 DLL found | The game moved or no longer has the DLL. |

**Remove** puts the game's own DLL back (HelixSR keeps it as `*.original.dll` next to it). **Forget**
only takes the game off the list. Close the game before installing or removing.

## Good to know

- Games that only offer DLSS, XeSS or FSR 2 need HelixSR Scaler; this app does not install it.
- Online games with anti-cheat may not accept a replaced DLL. Use it at your own risk.
- HelixSR's settings live in `helixsr.ini` next to the game's DLL (`Sharpener`, `Sharpness`,
  `WaveSize`); restart the game after changing them. Its log is `helixsr.log` in the same folder.
- Everything the app does for HelixSR is written to the application log and the game's log;
  see [Logs and troubleshooting](logs-and-troubleshooting.md).
