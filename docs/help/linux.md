# Linux (Bazzite, Steam and Proton)

WindowManager also runs on Linux, for Windows games that run through Steam and Proton. It was built for
Bazzite in Desktop Mode (KDE Plasma) and tested there with GOD EATER 3 under Proton. The Linux version is a
**beta**: it works for the things listed below, but not every feature of the Windows version is there yet.

## What works

| Feature | On Linux |
|---|---|
| Steam games | Found by themselves at start-up and with *File > Find Steam games*, the same as on Windows. |
| Start, Restart, Kill | Steam games start through Steam (`steam://rungameid/<AppID>`), so Proton, DRM and the overlay work. A game is never started twice. |
| Scan, Focus, Shutdown | Work on the game's window. |
| Monitor 1 / 2, Position & Size, Presets | Work on windows that may be resized. Many Proton games have a fixed-size window that KDE will not resize. |
| True Borderless FS | Puts the window in the desktop's own full-screen mode: no frame, exactly the monitor's size. **Windowed** takes it out again. |
| Topmost, Tool Window | Work. |
| Capture (snapshots) | Captures the game window itself. |

## Not yet on Linux

| Feature | Why |
|---|---|
| Downscaled view and Ctrl+Alt+D | Not built for Linux yet. The app says *The downscaled view is not available on this system yet*. |
| Layered, No Activate | These are Windows window styles. The app says *This style is not supported on this system*. |
| HelixSR tab | The app sets HelixSR up with Windows tools (PowerShell, Direct3D 12), so the system check finds no usable card. |
| Non-Steam `.exe` games | Started with plain `wine`, without Proton. Start them through Steam instead (add them to Steam as a non-Steam game). |
| Gaming Mode | Out of scope: in Bazzite's Gaming Mode, gamescope owns every window. Use Desktop Mode. |

## Things that work differently

- **Only X11 windows.** Proton games are shown through XWayland, which the app can control. Programs that
  draw directly on Wayland are not listed. The app itself also runs through XWayland.
- **Fixed-size windows.** Wine tells the desktop that many game windows may not be resized. KDE then
  refuses a new size and keeps the title bar. Use **True Borderless FS** for a full-screen window without a
  frame.
- **Focus.** KDE's focus-stealing prevention ignores ordinary requests, so the app asks the way a task bar
  does.
- **Snapshots** read the game window, because under Wayland the screen itself cannot be read.

## Getting it

There is no Linux download yet: the releases on GitHub are for Windows. For now the app is built from source
inside a container. On Bazzite, which has no compilers of its own, use `distrobox`:

```
git clone https://github.com/RobertoTorino/WindowManager.git && cd WindowManager
distrobox create -n wm-dev -i registry.fedoraproject.org/fedora-toolbox:44
distrobox enter wm-dev
sudo dnf install -y cmake ninja-build clang gcc-c++ qt6-qtbase-devel qt6-qtbase-private-devel \
    qt6-qttools-devel libX11-devel libXrandr-devel libXext-devel libxcb-devel xterm git
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/linux
./build/linux/WindowManager
```

Start the app from the container in a Desktop Mode session. Its settings, games and logs are kept next to
the program, as on Windows (see [Reference](reference.md)).

A packaged download (an AppImage) is planned. A Flatpak is not: its sandbox hides other programs'
processes, which the app has to see.

## See also

- [Games and launching](games-and-launching.md) for Steam games
- [Window controls](window-controls.md) for True Borderless FS
- [Logs and troubleshooting](logs-and-troubleshooting.md)
