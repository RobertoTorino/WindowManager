# Game stats, GPU and power

The bar above the status bar shows how the running game uses the PC, once a second:

| Item | What it shows |
|---|---|
| **FPS** | Frames per second over the last second, and the *1 % low*: the frame rate of the slowest 1 % of frames in the last 5 seconds. A 1 % low far below the FPS means stutter. The tooltip has the longest frame. |
| **CPU** | The game's share of the whole CPU (all cores together, as in Task Manager). The tooltip has the whole system's use. |
| **GPU** | The game's busiest GPU engine, as in Task Manager, and the graphics card it renders on. |
| **RAM** | The memory the game has in RAM now (its working set). |
| **VRAM** | The video memory the game has on its graphics card. |
| Power | *Plugged in* or *On battery*, with the battery level. The tooltip has the power plan and the Windows power mode. |

It follows the window selected in the table, or else the selected game's process, also when the
game was started outside the app. Hide it with *View > Game stats bar*; while neither it nor the
floating bar is shown, nothing is read.

## Floating bar

WindowManager stays on top of other windows (*View > Always on top*), which can be in the way. Minimise
it and it turns into a small **floating bar** instead: always on top, but only one line high, with the
game's name, FPS, CPU, GPU and the first warning. It never takes the keyboard from the game.

- **Drag** it anywhere; the spot is remembered.
- **Double-click** it, or press **Open**, to bring the window back.
- **Right-click** for *Open WindowManager*, *Focus the game* and *Exit*.
- *View > Switch to floating bar* (Ctrl+B) goes there directly. Turn off *View > Minimize to floating bar*
  to minimise to the taskbar as before.

A game in exclusive full screen draws over every window, the floating bar included; in borderless full
screen it stays visible.

Windows only for now: on Linux the bar is not shown (see [Linux](linux.md)).

## Warnings

An orange warning appears on the bar when something is known to slow games down. Hover over it for the
reason; with more than one, the bar shows the first and *(+n)*.

| Warning | Why it slows the game | What to do |
|---|---|---|
| **On battery** | On battery, Windows and the graphics driver lower the CPU and GPU speed, on many laptops to less than half. | Plug in the charger. |
| **Battery saver on** | Battery saver limits the CPU speed and background work. | Switch it off under *Settings > System > Power & battery*. |
| **Power-saving mode** | The Windows power mode is *Best power efficiency*. | Choose *Best performance* under *Settings > System > Power & battery*. |
| **Integrated GPU** | The game renders on the processor's graphics (for example Intel UHD) while the PC has a faster graphics card. | On the **GPU & Power** tab, set the game to *High performance*, then restart the game. |
| **CPU fully used** | All CPU cores are busy, often because of another program. | Check Task Manager. |

## Choosing the graphics card (GPU & Power tab)

Laptops often have two graphics cards: one in the processor (Intel UHD, AMD Radeon Graphics), which
saves power, and a faster one (NVIDIA GeForce, AMD Radeon RX). Windows decides per program which one
renders. The **GPU & Power** tab shows:

- **Graphics cards in this PC**: each card, which one is the dedicated (faster) one, and its driver
  version and date (NVIDIA's own number, such as 617.42, for NVIDIA cards).
- **Graphics card for <game>**: Windows' setting for the selected game's exe. Each choice shows the card
  Windows picks for it:
  - *Let Windows decide*: Windows chooses. This is not always the fast card (see below).
  - *Power saving*: the processor's graphics.
  - *High performance*: the dedicated card.
- **Renders on now**: the card the running game actually uses (needs the stats bar shown).

**Set your games to High performance.** What *Let Windows decide* picks can change without warning. On a
laptop with an Intel UHD and a GeForce GTX 1650 it meant the GTX 1650, but after an NVIDIA driver update it
meant the Intel UHD, so every game without its own setting rendered on the slow card. Choosing *High
performance* for a game fixes its card. Check the choice again after a driver update: the tab shows which
card each option means at that moment.

The setting is the same one as in *Settings > System > Display > Graphics* (**Windows graphics
settings...** opens that page), so it also applies when the game is started some other way. It is stored
per exe and takes effect when the game starts, so changing it while the game runs offers **Restart game**.
A game started through a `.bat` file must have been started once from the app, so the app knows its exe.

### Driver updates

The first time the tab opens (and with **Check for driver updates**) the app asks NVIDIA, through the
lookup behind nvidia.com/Download, for the newest Game Ready driver of an NVIDIA card, and shows
*Up to date* or, in orange, *Update available: <version> (<date>)*. **Driver page** then opens that
driver's page. Intel and AMD have no such lookup: **Driver page** opens their download / detection page.
The app never downloads or installs a driver itself.

## Power plan and power mode

The **Power** part of the tab has the same choices as *Control Panel > Power Options*:

- **Power plan**: every plan, also the ones Control Panel hides under *Show additional plans*
  (*Power saver*, and *Ultimate performance* where it exists). Applied at once.
- **Power mode**: Windows' *Best power efficiency* / *Balanced* / *Best performance* (Settings > System >
  Power & battery). Windows only offers it with the *Balanced* plan; with another plan it is greyed out.
- Below: plugged in or on battery, the battery level and battery saver.

**Power options...** opens the Control Panel page, for editing a plan's own settings.

## FPS needs administrator rights

Frames are counted from the DirectX present events that Windows records for every program (the same
source PresentMon and other overlays use). Recording them needs administrator rights, so the bar shows
*needs admin* when the app runs normally. Either start WindowManager with *Run as administrator* each
time, or add your account to the **Performance Log Users** group once. That group may record these
events without administrator rights.

### Adding your account to Performance Log Users

1. Find your account name: open a terminal and run `whoami`. It shows `PC-NAME\name`; you need `name`.
   A Microsoft account shows its local name here too.
2. Open **PowerShell as administrator** (right-click Start > *Terminal (Admin)*).
3. Run, with your name in place of `Admin`:

   ```powershell
   Add-LocalGroupMember -SID S-1-5-32-559 -Member Admin
   ```

   `S-1-5-32-559` is the group's fixed ID, so this works in every Windows language (the group's name is
   translated, for example *Gebruikers van prestatielogboek* in Dutch). The classic command does the same
   on an English Windows: `net localgroup "Performance Log Users" Admin /add`.
4. **Sign out and in again** (or restart). Group membership only counts from the next sign-in.
5. Check it: `whoami /groups | findstr /i "S-1-5-32-559"` prints a line. In the app the bar then shows FPS
   while a DirectX game runs.

Without PowerShell: *Computer Management* (`compmgmt.msc`) > *Local Users and Groups* > *Groups* >
*Performance Log Users* > *Add...* (not on Windows Home, which lacks that page; use the command there).
To undo it: `Remove-LocalGroupMember -SID S-1-5-32-559 -Member Admin`.

DirectX 9 to 12 games are counted. Vulkan and OpenGL games show *-*, and so does a game that is paused
or minimised and draws nothing.

## In the logs

While a game runs, the bar writes to the game's log (see [Logs and troubleshooting](logs-and-troubleshooting.md)):
a summary 10 seconds after the game starts and then every minute, and each warning the first time it
appears. A slow session can then be checked afterwards:

```
[Stats] FPS 24 (1% low 9, worst frame 180.2 ms), CPU 31 % (system 45 %), GPU 99 % on NVIDIA GeForce GTX 1650, RAM 7.2 GB, VRAM 3.6 GB, on battery (battery 72 %), plan High performance
[Stats] On battery: The PC runs on its battery. ...
```
