# Window terms

Background for the buttons and toggles. Nothing here needs to be learned to use the app.

## Common window modes

| Mode | Description |
|---|---|
| Fullscreen | Covers the entire screen, often an exclusive mode for games. |
| Windowed | Standard resizable window with title bar and borders. |
| Borderless | Looks fullscreen but is a window without borders. |
| Hidden | The window exists but is invisible. |

## Window states

| State | WinAPI constant | Description |
|---|---|---|
| Normal / Restored | SW_SHOWNORMAL / SW_RESTORE | Standard size. |
| Minimized | SW_MINIMIZE | Shrunk to the taskbar. |
| Maximized | SW_SHOWMAXIMIZED | Fills the screen. |
| Hidden | SW_HIDE | Exists but invisible. |
| Shown / Activated | SW_SHOW / SW_SHOWNA | Visible and optionally activated. |

## Window styles

| Style | Description |
|---|---|
| WS_OVERLAPPEDWINDOW | Typical app window: border, title bar, min/max buttons. |
| WS_POPUP | Borderless window, often used for fullscreen. |
| WS_BORDER | Thin border. |
| WS_CAPTION | Title bar. |
| WS_SYSMENU | System menu. |
| WS_MINIMIZEBOX | Minimize/maximize buttons. |
| WS_SIZEBOX / WS_THICKFRAME | Resize by dragging edges. |
| WS_DISABLED | Cannot receive input. |
| WS_VISIBLE | Initially visible. |

## Extended window styles (the toggles on the Window tab)

| Style | Description |
|---|---|
| WS_EX_TOPMOST | Stays above all other windows. |
| WS_EX_TOOLWINDOW | Small title bar, used for floating tool windows. |
| WS_EX_APPWINDOW | Forces a taskbar button. |
| WS_EX_NOACTIVATE | Shows without stealing focus. |
| WS_EX_LAYERED | Allows transparency and alpha blending. |

## Predefined sizes

| Name | Resolution |
|---|---|
| Full HD | 1920 x 1080 |
| Quad HD (2K) | 2560 x 1440 |
| 4K Ultra HD | 3840 x 2160 |
| 5K | 5120 x 2880 |
| 6K | 6016 x 3384 |
| 8K Ultra HD | 7680 x 4320 |
