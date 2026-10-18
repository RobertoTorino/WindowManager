# Changelog

Notable changes to WindowManager. The version number lives in `APP_VERSION` in `CMakeLists.txt`
(CI bumps it); the build number and date are shown in the window title and Help > About.

## Unreleased

### Added
- **HelixSR tab.** Sets up [HelixSR](https://github.com/lonewolf0622/HelixSR) (an FSR 3.1 upscaler
  for AMD GPUs) without leaving the app:
  - downloads the newest release, checks its SHA-256, lets HelixSR's own setup build the network
    on this PC (about 5 to 6 minutes, with a progress bar and a running timer, cancellable), then
    deletes the downloads, the DLSS DLL and the build tools;
  - checks GitHub for a newer release at start-up (when HelixSR is set up) and on demand; the tab
    shows *HelixSR (update)* and offers to update games that still have the older DLL;
  - a list of games confirmed to have an AMD FSR 3.1 DLL (**Check selected game**) with
    **Install**, **Remove**, **Forget** and **Run with HelixSR** (installs when needed, reinstalls
    after a game update put its own DLL back, then starts the game). Only the FSR drop-in is supported.
- **System check for HelixSR** (**Check this PC**, and automatically before a set-up): lists the
  graphics adapters and tests Direct3D 12, shader model 6.6, 16-bit math and SIMD width, free disk
  space, `tar.exe` and PowerShell. A PC that cannot run HelixSR is stopped before the 5-minute
  build; an untested card (Vega, NVIDIA, Intel) gets a warning first.
- **Build number and date** in the window title (`WindowManager 0.1.3 - build 16 - 2026-10-19`) and
  in Help > About (with the commit; `+` marks uncommitted changes). The number is the CI run number,
  or the commit count for local builds.
- Help page *HelixSR*, `tst_helix` (versions, releases, checksums, game detection, paths, system
  verdicts) and a `HelixConfirmed` flag per game in `windowmanager.json`.

### Fixed
- `CMakeLists.txt` was committed with unresolved merge-conflict markers (two copies of the
  script), so the project could not be configured.
