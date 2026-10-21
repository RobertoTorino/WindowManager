# Magpie (window upscaler)

[Magpie](https://github.com/Blinue/Magpie) (free, open source, GPL-3.0) scales a window up to the full monitor
with an effect of your choice: FSR 1, Lanczos, FSRCNNX, Anime4K, CRT shaders or integer scaling. It captures the
window instead of going into the game, so it works with any game: DirectX 11 or 12, Vulkan, emulators. The game
runs **windowed at a lower size** and Magpie fills the monitor with it.

Where HelixSR needs a DirectX 12 game with AMD FSR 3.1, Magpie works with any window, on any graphics card that
has DirectX feature level 11 (Windows 10 1903 or newer).

## Set up Magpie (once)

On the **Magpie** tab press **Set up Magpie**, read the notice and choose **Download**.

1. Magpie is downloaded from its GitHub releases (about 10 MB) and checked against the SHA-256 GitHub publishes
   for it. A file that does not match is deleted.
2. It is unpacked into the `magpie` folder next to the program. A `config` folder in it makes Magpie keep its
   settings there (its *portable mode*), so this copy is separate from a Magpie you installed yourself.
3. Magpie is started once in the tray, so it writes its default settings, and closed again.

It takes a few seconds. **Open folder** shows the copy.

**Only one Magpie can run at a time.** If your own Magpie is running, the set-up stops before downloading and
says so: close it (its tray icon > *Exit*) and try again.

## Updates

When Magpie is set up, the app checks GitHub for a newer release a few seconds after it starts (one small
request); **Check for updates** does it on demand. When there is one, the tab is named *Magpie (update)* and the
button reads *Update to x.y.z*. An update closes the app's Magpie if it runs, replaces the program files and
keeps Magpie's settings, its shader cache and its logs.

## Use it for a game

1. Select the game (the game bar at the top).
2. On the **Magpie** tab, under **Selected game**, choose the effect in **Scale with**. The list is Magpie's own
   (*Lanczos*, *FSR*, *FSRCNNX*, *CuNNy*, *Anime4K*, *CRT-Geom*, *Integer Scale 2x*, and any you add in Magpie).
   *Off* means Magpie is not used for this game. The choice is saved with the game.
3. Set the game to **a window at the size it should render**, for example 1280 x 720 on a 1920 x 1080 monitor
   (the game's own settings, or **Position & Size** in this app).
4. Press **Start** (or **Restart**).

What happens then:

- Once the game's window has settled (a few seconds), the app writes a Magpie profile for it, named
  *WindowManager: &lt;game&gt;*, with the window's class and exe path, and starts Magpie in the tray. If the app's
  Magpie was running already, it is restarted first, because Magpie reads its settings only when it starts.
  If the game swaps its first window for another (a splash screen first, as Unreal games do), the profile is
  written again for the new window and Magpie restarted.
- From then on Magpie scales the game whenever the game is in front, and stops while another window is (F1 to
  this app and back works).
- When the game ends (also with **Shutdown**, F2 twice, or closing it yourself), Magpie is closed.
- The status bar and the log (*Magpie* lines) say what was done, and why not when something is in the way.

| Rule | Why |
|---|---|
| The game must be windowed, not maximized or borderless full screen | Magpie does not scale those. If the game is, the status bar says so. |
| The game's window must be smaller than its monitor | Magpie only scales up: a bigger window is shrunk to fit, squeezed if its shape differs. The status bar warns. Don't combine Magpie with a resolution above the monitor's on the **Game config** tab (that is for the downscaled view). |
| The fit check after Start is skipped for this game | It proposes borderless full screen. **Fit to monitor** by hand still opens. |
| **Stability** is turned off on Start | Magpie restarts its scaling whenever the window is moved or resized. |
| Only one Magpie can run | If your own Magpie runs, the app does not start its copy and says so. |

**3D game mode** (next to *Scale with*) is for games that turn the camera with the mouse. Magpie then keeps the
mouse inside the game **all the time**, also while another window is in front, until Magpie closes (it does when
the game ends). Leave it off unless the game needs it.

Magpie draws the picture in its own process: the game info overlay shows its GPU use apart from the game's
(`GPU 29 % + Magpie 6 %`). Whether Magpie is worth it for a game: the [Reports](reports.md) tab compares sessions
with and without it, and **Compare picture** measures how much sharper it is than a plain stretch. The overlay shows whether Magpie scales and from what size to what size; see [Game info overlay](game-stats.md#why-the-picture-can-look-wrong-with-magpie).

The plan and what was found while trying Magpie are in [Magpie (analysis)](../MAGPIE.md).
