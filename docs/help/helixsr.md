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
| not supported | No Direct3D 12 device, shader model below 6.6, too little disk space, or a missing tool. Set-up is stopped. |

**Shader model below 6.6.** HelixSR's shaders are built for shader model 6.6, so a card that reports less cannot
run them. The graphics driver decides what the card reports: install the newest driver for the card and check
again. If the newest driver still reports 6.5 or lower (this can happen with data-centre and older cards),
HelixSR cannot run on that card. The app cannot work around this.

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

The app sets up HelixSR 1.x only. HelixSR 2.0 changed its setup: run unattended, it installs HelixSR
Scaler into every game it finds. Until the app supports that, the tab shows *HelixSR 2.0.0 is out; this
app does not support it yet* and keeps offering the newest 1.x version.

## Games

HelixSR works in games that ship AMD FSR 3.1 (a file named `amd_fidelityfx_dx12.dll` or
`amd_fidelityfx_upscaler_dx12.dll`).

1. The list shows every saved game (Steam games are added by themselves). The app looks through each
   game's folder in the background. Games with an FSR 3.1 DLL come first. Games without one are greyed
   out and cannot be selected.
2. Pick a game in the list and press **Install**, or just **Run with HelixSR** (double-click does the
   same): it installs when needed and then starts the game like **Start** does.
3. In the game's graphics settings choose **AMD FSR** as the upscaler.

| Status in the list | Meaning |
|---|---|
| not installed | The game has an FSR 3.1 DLL; HelixSR is not in it yet. |
| HelixSR x.y.z installed | The game's FSR DLL is HelixSR's. "is available" means a newer build can be installed. |
| the game put its own DLL back | A game update or "verify files" restored the original. Install again (Run does it for you). |
| no AMD FSR 3.1 DLL: HelixSR cannot be used | Greyed out. The game has no FSR 3.1 (it may only offer DLSS, XeSS or an older FSR). |

**Remove** puts the game's own DLL back (HelixSR keeps it as `*.original.dll` next to it). Close the game
before installing or removing.

## Settings for a game

With a game that has HelixSR installed selected, the row under the list edits that game's `helixsr.ini`
(next to HelixSR's DLL):

| Setting | Values | Meaning |
|---|---|---|
| Sharpening | off, nvidia, rcas | An optional sharpening pass after upscaling. |
| Strength | 0.00 to 1.00 | Used when sharpening is on. |
| Wave size | auto, 64, 32 | Leave on auto unless the picture is broken. |

**Save settings** writes the file (comments and other keys in it are kept; a missing file is created).
Restart the game for the change to apply.

## If installing is switched off

After a build the app tries HelixSR's installer on a throw-away folder. If a newer HelixSR changed its
installer so that WindowManager's helper no longer fits, the tab says so and **Install**, **Remove** and
**Run** are disabled until WindowManager is updated. The built HelixSR is not lost: **Open folder** shows it,
and running `helixsr-setup` there installs it into games by hand.

## Good to know

- Games that only offer DLSS, XeSS or FSR 2 need HelixSR Scaler; this app does not install it.
- Online games with anti-cheat may not accept a replaced DLL. Use it at your own risk.
- HelixSR's log is `helixsr.log` next to the game's DLL.
- Looking through a game's folder happens in the background, so a big game never blocks the app; a list
  entry reads "checking..." for a moment.
- Everything the app does for HelixSR is written to the application log and the game's log;
  see [Logs and troubleshooting](logs-and-troubleshooting.md).
