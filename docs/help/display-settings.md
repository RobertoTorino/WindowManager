# Display settings

The **Display** tab changes the PC's own display settings, the ones Windows shows under
*Settings > System > Display*, without leaving the app. It can also give a game its own settings
(for example 1280 x 720 with HDR on) that are switched on when the game starts and undone when it
closes.

Windows only for now: on Linux the tab says it is not available yet (see [Linux](linux.md)).

## This PC's displays

Pick the **Monitor** first. Everything on this part of the tab is for that display. The settings
are read from Windows when the tab opens, after every change and with **Re-read**.

| Setting | What it does |
|---|---|
| **Resolution**, **Refresh rate**, **Orientation** | Pick them, then press **Apply**. Like in Windows, a box asks whether to keep the new mode. If you do not answer within 15 seconds (for example because the screen went black), the old mode comes back. |
| **Scale** | Size of text and apps (100 %, 125 %, ...). Applied at once. The steps on offer depend on the resolution. Some programs only follow after a restart. |
| **HDR** | Switches HDR on or off. Shows *Not supported* for a display without HDR. |
| **Brightness** | A laptop's built-in panel is set through Windows. An external monitor is set through DDC/CI, which most monitors support (some have it switched off in their own menu). *n/a* means the display does not take brightness commands. |
| **Color profile** | The profiles Windows has for this display. To add one, use *Settings > Display > Color profile*. |
| **Multiple displays** | Like Win+P: *PC screen only*, *Duplicate*, *Extend*, *Second screen only*. Only available with two or more displays connected. Asks to keep the change, like the resolution. |
| **Night light...** | Opens the night light page in Windows Settings. Windows has no supported way for other programs to switch night light. |

The scale is set the same way Windows Settings sets it, through a part of Windows that Microsoft
has not documented. If a Windows update changes it, the scale shows as unknown and cannot be set.
The rest of the tab is not affected.

## While the game runs

Give the selected game its own display settings:

1. Select the game in the game bar.
2. Tick **Use these settings while <game> runs**.
3. Choose the **Monitor** (*Main display* follows whichever display is the main one), then what to
   change: **Resolution**, **Refresh rate**, **HDR**, **Scale**, **Brightness**. *Keep* leaves a
   setting as it is.

The choice is saved for that game at once (in `windowmanager.json`).

When you press **Start** or **Restart**, the settings are applied *before* the game starts, because
most games read the resolution when they start. The resolution is changed only while the game runs
and is never stored as Windows' own setting.

The desktop's settings come back when:

- the game closes (the app checks every 2 seconds; a few seconds of absence count as closed, so a
  launcher that restarts the game is not cut off),
- the game did not start within 3 minutes, or the launch failed,
- another game is started from the app,
- you press **Restore now**, or close WindowManager.

If WindowManager itself crashes while a game runs, the desktop settings are restored the next time
it starts (the record is kept in `windowmanager.ini`).

Only games started from WindowManager get their settings. Starting a game from Steam or the
desktop leaves the display alone.

## Good to know

- A resolution change moves windows around, like it does in Windows.
- Changing a setting by hand while a game's settings are on is fine, but the desktop's values come
  back when the game closes.
- The status bar and the log (*File > View logs...*) say what was changed and what failed.
