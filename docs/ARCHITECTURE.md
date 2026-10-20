# Code layout

```
src/
  main.cpp                      App start-up: logging, Qt app, theme, MainWindow
  MainWindow.{h,cpp}            Composes the UI (game bar, action row, table, tabs, menus, status bar)
  WindowManagerController.*     Qt-facing facade over the window logic; emits statusChanged()
  config/AppConfig.*            windowmanager.json (games, last target, per-game profiles)
  core/Logger.*                 Thread-safe rolling file logger (app log + one log per game)
  core/BuildInfo.h              Window title / About text: version, build number, date (from build_stamp.h)
  core/Utilities.*              String / path / timestamp helpers
  core/IniText.*                Pure logic: read / edit ini text in place (helixsr.ini, Unreal config files)
  window/WindowManager.*        Window policy: scan, move, resize, fit, launch, kill (no OS calls; uses platform())
  window/MonitorHelper.*        Monitor indexes and geometry, read through platform()
  window/WindowStabilizer.h     Re-applies a window's rectangle when the game moves it (per-platform .cpp)
  platform/PlatformBackend.*    Interface to the OS (windows, monitors, processes, screen / window capture, opening URLs) and platform()
  platform/win32/Win32Backend.cpp           The Windows implementation of PlatformBackend
  platform/win32/Win32WindowStabilizer.cpp  WinEvent hook behind WindowStabilizer
  platform/x11/X11Backend.cpp               The Linux implementation: xcb + EWMH for windows, QScreen for monitors, /proc for processes.
                                            Wine image paths go through the prefix's dosdevices (Proton's S: is the Steam folder);
                                            a whole-monitor rectangle becomes _NET_WM_STATE_FULLSCREEN; snapshots read the
                                            window (captureWindow), as XWayland's root has no pixels
  platform/PollingWindowStabilizer.cpp      WindowStabilizer for systems without a move hook (checks every 100 ms)
  platform/unsupported/                     Stand-ins for features a platform lacks (the downscaled view on Linux)
  window/KillPlanner.*          Pure logic: which processes belong to this session
  window/GameWatch.h            Diagnostic log of the running game's windows, the foreground and the mouse clip
                                (per-platform .cpp: platform/win32/Win32GameWatch.cpp, unsupported/ stand-in)
  window/GameWatchModel.*       Pure logic: what changed about a game window, the foreground / mouse lines
  window/FocusSwitch.*          Pure logic: who has the keyboard (game / app / other), what the focus hotkey does,
                                hotkey text <-> modifiers + virtual-key code
  window/FitCalculator.*        Pure logic: fit-to-monitor maths (Fit::compute) and the
                                launch-settle trigger (Fit::Trigger)
  window/Supersample.*          Pure logic: letterbox, render size, parking position
  window/SteamMatch.*           Pure logic: Steam AppID from a window class / pasted text, the launch URL, and which window is the target game
  window/SteamLibrary.*         Installed Steam games: libraryfolders.vdf / appmanifest parsing, tool filter, main-exe choice (pure, tested) and the folder scan
  display/DisplayModel.*        Pure logic: modes / orientation, scale steps, brightness percent, WMI instance match,
                                a game's display settings (JSON), what to change (plan) and how to undo it (Restore)
  display/DisplayBackend.h      Interface to the system's display settings, and displayBackend()
  display/DisplaySession.*      A game's display settings: applied before the launch, restored when the game closes
  platform/win32/Win32Display.cpp           DisplayBackend on Windows: DisplayConfig, ChangeDisplaySettingsEx,
                                            SetDisplayConfig, WMI + DDC/CI brightness, mscms colour profiles
  platform/unsupported/DisplayUnsupported.cpp  DisplayBackend stand-in (Linux): supported() == false
  gameconfig/GameConfigModel.*  Pure logic: a game's config settings (JSON), an Unreal Engine 4 game's config layout,
                                the keys per file, JConfig's monitor_misc.bin fields
  gameconfig/GameConfigFiles.*  Writes them into the game's files (copy of each original as *.wm-original), restores
  stats/StatsModel.*            Pure logic: GPU counter instance names, GPU / CPU / frame-rate maths, the slow-game
                                warnings, the log line; Sampler turns two raw samples in a row into GameStats
  stats/StatsBackend.h          Interface to the game's resource use, the GPU setting, power plans / mode, and statsBackend()
  stats/DriverModel.*           Pure logic: installed driver per card, NVIDIA's version number, NVIDIA lookup ids / answer
  stats/DriverCheck.*           Asks NVIDIA's driver lookup for the newest driver (Qt Network)
  platform/win32/Win32Stats.cpp StatsBackend on Windows: process times / memory, PDH GPU counters, DXGI adapters,
                                an ETW real-time session for present events, power status and plan
  platform/unsupported/StatsUnsupported.cpp    StatsBackend stand-in (Linux): supported() == false, no bar
  helix/HelixRelease.*          Pure logic: versions, GitHub release parsing, checksums, update decision
  helix/HelixGame.*             Pure logic: where a game's FSR 3.1 DLLs are, installed / replaced state
  helix/HelixPaths.*            Pure logic: on-disk layout, built-package marker, post-build leftovers
  helix/HelixIni.*              Pure logic: read / edit helixsr.ini (Sharpener, Sharpness, WaveSize), keeping the rest of the file
  helix/HelixSystem.*           Pure logic: GPU family from the name / SIMD width, can this PC run HelixSR (Verdict)
  helix/SystemProbe.*           DXGI + Direct3D 12 probe of the adapters, free disk space, tar / PowerShell
  helix/HelixSetup.*            Download, verify, unpack, build (HelixSR's own setup), clean up; per-game install
  ui/Theme.*                    Dark palette + stylesheet; button roles and "active" state
  ui/GameBar.*                  Game dropdown + Add / Start / Restart / Kill
  ui/WindowTable.*              Table of the target's windows (+ empty-state message)
  ui/ControlPanels.*            Tab pages: StatePanel, PositionPanel, PresetPanel, CapturePanel
  ui/SnapshotCapture.*          Screenshot / burst capture
  ui/GameRegistry.*             Saved-game helpers (names, entries, snapshot folders) and syncSteamGames (Steam libraries -> saved games)
  ui/FitDialog.*                "Fit window to monitor" popup (measurements + correction)
  ui/ScaledView.*               Downscaled view: overlay window + DWM thumbnail of the game
  ui/InfoDialogs.*              About dialog
  ui/HelpDialog.*               Help > Help topics: topic list + search over docs/help/*.md (embedded via the .qrc);
                                web links and Help > Online wiki open the wiki (wikiUrl)
  ui/LogViewerDialog.*          File > View logs...: app and per-game log viewer
  ui/HelixPanel.*               HelixSR tab: set-up / update with a timer, list of confirmed games, run
  ui/DisplayPanel.*             Display tab: this PC's display settings, and the selected game's own
  ui/GameConfigPanel.*          Game config tab: render scale, resolution and JConfig window for the selected game
  ui/FocusMonitor.*             Reads the foreground every 250 ms: Focus: game / app / other (FocusSwitch::ownerOf)
  ui/StatsMonitor.*             Samples the game once a second while the stats bar or the floating bar shows;
                                logs a summary; statsUpdated() feeds the views
  ui/StatsBar.*                 Game stats bar (a bottom QToolBar), shows StatsMonitor's samples
  ui/MiniBar.*                  The floating bar the app minimizes to (frameless, topmost, takes no focus)
  ui/GpuPanel.*                 GPU & Power tab: graphics cards and drivers (+ NVIDIA update check), the card in use,
                                Windows' per-exe graphics setting, power plan and power mode
tests/                          QtTest suites (see below)
resources/                      Icon, arrow SVGs used by the stylesheet, Windows .rc, helixsr_driver.py
```

## How the pieces talk

```
 buttons / table ──click──▶ WindowManagerController ──▶ WindowManager
        ▲                          │
        └────── refresh() ◀── statusChanged()

 WindowManager / MonitorHelper ──▶ PlatformBackend (interface) ──▶ Win32Backend
```

- UI classes never call Win32 window APIs for the managed game. They call a
  `WindowManagerController` slot, which runs the action, re-syncs the stabilizer and
  emits `statusChanged()`.
- `MainWindow::refresh()` is the single place that re-reads state: it updates the
  status bar, the monitor/stability buttons, the table, and calls `refresh()` on each
  panel (which sets the green "active" buttons).
- `WindowTable` only emits `windowChosen` when the *user* selects a row, so refreshing
  the table cannot feed back into another refresh.

## Adding a control

- **A button on an existing tab:** add it in that panel's constructor (`ControlPanels.cpp`)
  with `Theme::button(...)` and connect it to a controller slot. If it shows a toggled
  state, set it in the panel's `refresh()` with `Theme::setActive`.
- **A new tab:** derive from `ControlPanel`, implement `refresh()` if needed, create it
  in `MainWindow::buildUi()` and add it to both `m_tabs` and `m_panels`. The tab area
  sizes itself to the current page, so no height bookkeeping is needed.
- **A new window action:** add a method to `WindowManager` (it calls `platform()`; a new OS primitive goes on `PlatformBackend` and in every backend), a slot on
  `WindowManagerController` using `finish(ok, okMsg, failMsg)`, then connect it.

## Styling

All colours live in `ui/Theme.cpp`. Widgets pick up variants through dynamic
properties rather than inline stylesheets:

- `role="accent"` / `role="danger"` - emphasised buttons (`Theme::button(text, parent, "danger")`)
- `active=true` - a toggle that is currently on (`Theme::setActive(button, on)`)

Spin boxes are styled in the same sheet; their arrows are the SVGs in `resources/`.

## Fit to monitor

```
 Start ──▶ controller.launchTarget() ──▶ armFitCheck()  (Fit::Trigger.arm, 500 ms timer)
                                              │
              every tick:  WindowManager::findPrimaryWindow()  ─▶  Fit::Trigger.update()
                                              │   (same hwnd + same rect for ~1.8 s)
                                              ▼
                            controller emits fitProposed() ──▶ MainWindow::showFitDialog()
                                                                      │
                          FitDialog ◀── measureFit(request) ──────────┤  recomputed on every option change
                                │
                       Apply ──▶ controller.applyFit(request) ──▶ WindowManager::applyFit()
```

- `Fit::compute` takes the window rectangle, the client rectangle (screen coordinates),
  and the target rectangle, and returns the insets, the desired window rectangle and
  the correction `dx, dy, dw, dh`. With `stripFrame` the insets are zero. It has no
  Win32 dependency and is unit-tested (`tst_fit`).
- `Fit::Trigger` decides *when* to propose: the window must exist, be usable and keep
  the same rectangle for the settle time; it fires once per `arm()` and times out
  after two minutes. Also unit-tested.
- `WindowManager::findPrimaryWindow` picks "the game": visible, not minimized, at least
  10,000 px^2, not a console/IME helper window; ready windows beat everything, then
  titled ones, then the largest.
- `WindowManager::measureFit` reads `GetWindowRect`, the client rectangle via
  `GetClientRect` + `ClientToScreen`, and the DWM extended frame bounds. `applyFit` removes
  the frame (if requested), then measures and corrects up to three times, because some
  games move their window again right after a move, and finally re-measures to report
  whether the fit is exact.

## Downscaled view

```
 DownscalePanel ── plan() ──▶ Supersample::renderSize()   how big the game window must be
        │ Start
        ▼
 ScaledView::start()
   1. overlay: borderless, topmost, WS_EX_NOACTIVATE QWidget placed on the target monitor and pinned
      there (its WM_WINDOWPOSCHANGING forces the monitor's rectangle: a monitor with another display
      scale made Windows / Qt resize it, once to Qt's default 640x480)
   2. game window: restore, popup style (no frame), size = render size,
      parked at Supersample::parkingOrigin()  (behind the overlay, or beyond the desktop edge)
   3. DwmRegisterThumbnail(overlay, game) with the destination rectangle from
      Supersample::letterbox(); source = client area only
   4. SetForegroundWindow(game): keyboard / gamepad stay with the game
        │ every 400 ms
        ▼
 tick(): game gone -> stop();  restore if minimized;  put the size back (Supersample::ReassertPolicy:
         every time, but at most 5 times within 10 s, then follow the game until it is quiet);
         update the thumbnail rectangle (the game's shape may change)
 stop(): unregister thumbnail, delete overlay, restore the game's original style + rectangle
```

- The game window is switched to a **popup** style because Windows limits a normal window
  to about the virtual desktop size; a popup is not clamped, so it can be 2x-4x the monitor.
- The mirror is a DWM thumbnail, so there is no copy through this app's memory.
  GPU-rendered (flip-model) windows are mirrored in full even beyond the monitor edge; GDI
  windows are only painted inside the desktop, so only that part shows.
- **Stop** is available three ways because the overlay covers the screen: the panel button,
  the global hotkey Ctrl+Alt+D (`RegisterHotKey` + `MainWindow::nativeEvent`), and
  automatically when the game window disappears or the app closes.
- **Mouse forwarding.** The overlay is a small `QWidget` subclass whose `nativeEvent` hands
  raw mouse messages (exact physical pixels, no DPI rounding) to `ScaledView::handleMouse`.
  It maps the point with `Supersample::mapPoint` (overlay pixel -> game client pixel, using
  pixel centres: a 2x picture maps overlay pixel n to game pixel 2n+1) and `PostMessage`s the
  same message to the game window with the mapped client coordinates. Wheel messages carry
  *screen* coordinates, so they are converted to the overlay's client space first and sent
  back as the game window's screen origin + mapped point. Points over the letterbox bars are
  swallowed; while a button is held the overlay has mouse capture and positions are clamped to
  the picture edge so a drag can leave the picture. The game keeps keyboard focus throughout.
  Games that read the mouse via raw input, DirectInput or `GetCursorPos` cannot see posted messages.

## Display settings

```
 DisplayPanel -- Apply / combos --> displayBackend() --> Win32Display
      |                                                  (DisplayUnsupported on Linux)
      | "While the game runs" --> AppConfig GameEntry::display (DisplayModel::GameSettings)
      v
 MainWindow::launchWhenFree --> DisplaySession::gameStarting(exe) --> controller.launchTarget()
                                   1. DisplayModel::plan(mode only)  -> setMode(temporary)   (CDS_FULLSCREEN)
                                   2. re-read, plan(the rest)    -> setHdr / setScale / setBrightness
                                   3. DisplayModel::undoFor + merge   -> Restore, written to windowmanager.ini
                                   every 2 s: WindowManager::isExeRunning
                                      seen, then gone 3 ticks / never seen in 3 min -> restore()
 restore(): resetMode (back to the stored mode), HDR, scale, brightness; also on a failed launch,
            "Restore now", another game starting, the app closing, and recoverFromCrash() at start-up
```

- `DisplayBackend` is separate from `PlatformBackend`: that one is about the game's windows, this one
  about the PC's monitors. Monitors are named by `DisplayModel::Monitor::id`, the monitor's device path from
  `DISPLAYCONFIG_TARGET_DEVICE_NAME`, which survives restarts (the `\\.\DISPLAYn` name does not).
- **Modes.** `Monitor::modes` is kept in landscape; `orientedModes` turns it into what is shown.
  The panel applies a mode with `CDS_UPDATEREGISTRY` and asks to keep it (15 s, then the old mode is set
  again); a game's mode uses `CDS_FULLSCREEN` (not stored) and is undone with `ChangeDisplaySettingsEx(NULL)`.
- **HDR.** `DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2` / `SET_HDR_STATE` (Windows 11 24H2), else
  the older advanced-colour calls. The structs are declared locally because the SDK only declares them for
  a newer `NTDDI_VERSION`. HDR is only switched on a display that reports HDR support: on 24H2 the old call
  would toggle Auto Color Management instead.
- **Scale.** Undocumented `DisplayConfigGetDeviceInfo` / `SetDeviceInfo` types -3 / -4 on the source:
  steps relative to the recommended scale (`DisplayModel::scaleFromRelative`, `kScaleSteps`). The steps depend
  on the resolution, so a game's scale is applied after its mode, and restored after the mode is back.
- **Brightness.** A built-in panel: WMI `root\WMI` `WmiMonitorBrightness` / `WmiSetBrightness`, matched to
  the monitor by `DisplayModel::wmiInstanceMatches`. Otherwise DDC/CI (`dxva2`: `GetMonitorBrightness`); it is
  slow (tens of ms per command), so the slider sends at most every 250 ms.
- **Colour profiles.** `ColorProfileGetDisplayList` / `GetDisplayDefault` / `SetDisplayDefaultAssociation`
  (mscms, Windows 10 1809+), looked up with `GetProcAddress`; the SDR or HDR default by the HDR state.
- **Arrangement.** `QueryDisplayConfig(QDC_DATABASE_CURRENT)` gives the topology,
  `SetDisplayConfig(SDC_APPLY | SDC_TOPOLOGY_*)` sets it. Night light has no API: the button opens
  `ms-settings:nightlight`.
- The panel reads the monitors when the tab is shown, after each change and on **Re-read**, not in
  `refresh()` (DDC/CI reads are slow).

## Game stats bar

```
 StatsMonitor (QTimer 1 s, while the stats bar or the floating bar shows)
     -- pid --> thread pool: statsBackend().sample(pid) --> RawSample
     pid: controller.activeWindowPid(), else WindowManager::runningPid(target) every 5 s
     <-- queued -- Stats::Sampler::update(pid, raw, adapters) --> GameStats --> game log
                                                               statsUpdated --> StatsBar, MiniBar, GpuPanel
```

- **CPU**: `GetProcessTimes` and `GetSystemTimes` deltas between two samples (`Stats::cpuPercent`, 100 % = all cores).
- **GPU**: the PDH wildcard counters `\GPU Engine(*)\Utilization Percentage` and `\GPU Process Memory(*)\Dedicated
  Usage`. Instances are named `pid_<pid>_luid_0x<high>_0x<low>_phys_0[_eng_<n>_engtype_<type>]`
  (`Stats::parseGpuInstance`); the use is the busiest engine type summed per adapter, as in Task Manager, and the LUID
  names the adapter through DXGI. One collection costs tens of ms, hence the thread pool.
- **FPS**: a real-time ETW session (`WindowManager-Frames`, QPC timestamps, 1 s flush) with the DXGI provider
  (`Present_Start` 42, `PresentMultiplaneOverlay_Start` 55) and the D3D9 provider (`Present_Start` 1), filtered to those
  ids. The callback keeps the target pid's timestamps; `Stats::frameRate` measures up to the newest present, because events
  arrive up to a second late. Starting the session needs administrator rights (or Performance Log Users):
  `FpsSource::NoPermission` otherwise. A session left behind by a crash is stopped and restarted; `MainWindow::closeEvent`
  stops it.
- **Power**: `GetSystemPowerStatus`, `PowerGetActiveScheme` / `PowerReadFriendlyName`, and for the Balanced plan the power
  mode from `PowerGetEffectiveOverlayScheme` (looked up with `GetProcAddress`).
- **Graphics setting** (GPU tab): the `REG_SZ` value named after the full exe path under
  `HKCU\Software\Microsoft\DirectX\UserGpuPreferences`, e.g. `AppStatus=4096;GpuPreference=2;`
  (`Stats::gpuPreferenceFrom` / `withGpuPreference` change only the `GpuPreference` entry). The card each choice
  means comes from `IDXGIFactory6::EnumAdapterByGpuPreference`. DirectX reads it when the game starts.
- **Drivers**: DXGI gives each adapter's PCI vendor / device id; the display class key
  `HKLM\SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}\NNNN` gives
  `MatchingDeviceId`, `DriverVersion` and `DriverDate` (`Driver::matchesDevice`; re-read on every `adapters()` call).
  NVIDIA's number is the last five digits (`32.0.16.1742` -> `617.42`). `DriverCheck` fetches
  `lookupValueSearch.aspx?TypeID=2` (series) and `TypeID=3` (products) once per run, picks the product's notebook
  or desktop series (a battery means notebook; `Driver::findNvidiaIds`), then `AjaxDriverService.php?func=
  DriverManualLookup` (osID 57, DCH, WHQL) for the newest driver.
- **Power plans**: `PowerEnumerate(ACCESS_SCHEME)` lists every plan, `PowerSetActiveScheme` switches. The power mode
  is `PowerGetEffectiveOverlayScheme` / `PowerSetActiveOverlayScheme` (by `GetProcAddress`; Balanced plan only).
- **Floating bar**: `MainWindow::changeEvent` sees the minimise and calls `showMiniBar()` (hide this window, show
  the `MiniBar`); `restoreFromMiniBar()` reverses it. `MiniBar` is a top-level `Qt::Tool | FramelessWindowHint |
  WindowStaysOnTopHint | WindowDoesNotAcceptFocus` window with `WA_ShowWithoutActivating`, so it never takes the
  keyboard from the game; dragging uses `QWindow::startSystemMove`. `StatsMonitor` runs while either bar shows.
- **Window list refresh**: `WindowManager::refreshWindows` re-reads the target's windows without logging, keeps the
  active window while it exists (`bestWindow` otherwise) and says whether anything changed; the controller's
  `refreshWindows` slot emits `statusChanged` only then. Called on `GameWatch::windowsChanged`,
  `StatsMonitor::gameChanged`, and every 3 s while this window is shown. `scanWindowsForTarget` (Scan) still picks
  the best window. The table marks the active row (bold, Status *Active*; green selection in `Theme.cpp`).
- **One instance** (`main.cpp`): a `QLocalServer` named after the user and a hash of the install folder. A second
  start connects, sends `show` (after `AllowSetForegroundWindow`) and exits; the first calls `MainWindow::bringToFront`.
  A copy in another folder (a test build) is a separate instance.
- **Off the game's monitor**: `MainWindow::keepOffGameMonitor` runs 400 ms after a controller status change, a
  `GameWatch::windowsChanged` or the downscaled view starting / stopping; if this window is on the game's monitor
  (`gameMonitorIndex`: the view's monitor while it runs, else the active window's) it moves to another one.
- **Release the mouse**: the global hotkey Ctrl+Alt+M (`RegisterHotKey`, like Ctrl+Alt+D) calls `ClipCursor(nullptr)`
  and `bringToFront`; a game that holds the mouse lets go once it loses the foreground.
- **Game watch** (diagnostics, Windows): `StatsMonitor::gameChanged(pid)` starts `GameWatch::watch(pid)` (0 stops it).
  It installs out-of-context WinEvent hooks: `EVENT_OBJECT_CREATE`..`HIDE` and `EVENT_OBJECT_LOCATIONCHANGE` for the
  game's process (top-level windows only, read 300 ms after the last report), and `EVENT_SYSTEM_FOREGROUND` for all
  processes; `GetClipCursor` is read every second. `Watch::WindowTracker` turns the readings into change lines.
  The downscaled view logs its own layout (`ScaledView layout:`) from `updateThumbnail`, on change only.
- `Stats::warningsFor` decides the warnings; the bar logs each once per game run and a `summaryLine` 10 s after the game is
  first seen and then every minute.

## Game config

```
 GameConfigPanel -- combos --> AppConfig GameEntry::gameConfig (GameConfig::Settings, JSON "GameConfig")
      | Write now / Restore originals
      v
 MainWindow::launchWhenFree --> GameConfig::apply(exeForConfig(exe, childExePath), settings)   (before the display
                                    settings and the launch: the game reads its files at start-up)
 apply / restore, per file of GameConfig::layoutFor(exe):
   <file>.wm-original exists? -> the original;  else the file itself (and nothing to do if the settings don't touch it)
   entriesFor(file, settings, original) -> IniText::apply(current, entries, "=")  -> write if changed (copy kept first)
   monitor_misc.bin: readJConfig -> jconfigFor(settings, original) -> writeJConfig
```

- **Layout.** `layoutFor` expects `<root>/<Project>/Binaries/Win64|Win32/<exe>`: `DefaultEngine.ini` and
  `DefaultGameUserSettings.ini` in `<Project>/Config`, `WindowsEngine.ini` in `<root>/Engine/Config/Windows`, JConfig's
  `monitor_misc.bin` next to the exe. The tab is usable when `DefaultEngine.ini` exists (loose config, as in Tekken 7's
  arcade builds: no `Saved/` folder, nothing packed).
- **Keys.** `[SystemSettings]` outranks the scalability groups and is the only place `r.ScreenPercentage` may exceed
  100 (`sg.ResolutionQuality` is capped at 100 by the engine, so it gets `min(scale, 100)`). `r.Upscale.Quality=3` goes
  with any render scale. The resolution goes to `ResX/ResY`, `ResolutionSizeX/Y`, `r.setres` and JConfig's DX resolution.
- **Edits in place.** Only the managed keys change; a setting left at 0 takes the original's value (or removes the key the
  original did not have), so the user's own edits elsewhere survive, and **Restore originals** is `apply(Settings{})` plus
  deleting the copies. `IniText::apply` returns the text untouched when nothing changes; otherwise trailing blank lines go.
  The encoding (UTF-8, UTF-8 with BOM, UTF-16 LE) and line endings stay.
- **JConfig** (`monitor_misc.bin`, 244 bytes = 61 little-endian int32): offset 60/64 window position, 68/72 window size,
  76/80 DX resolution, read from the user's file and the game's first window (2026-10-20). Other values are kept; a file
  of another size is left alone. JConfig's own dialog reads the same file.
- `.bat` games: `exeForConfig` uses `childExePath` when the script is not inside `Binaries`.

## Focus switch

- `FocusMonitor` polls `platform().foregroundWindow()` (`GetForegroundWindow`; X11 `_NET_ACTIVE_WINDOW`) every 250 ms.
  `FocusSwitch::ownerOf` compares the foreground pid with this app's, the active game window's
  (`controller.activeWindowPid()`) and whether the foreground process runs the target exe (read once per pid).
- `MainWindow::updateFocusSwitch` shows the result at the end of the stats bar (the status bar without one) and registers
  the hotkey (`RegisterHotKey`, id 3, default `F1` from `windowmanager.ini` `focusHotkey`) **only while a game is found**,
  so F1 is not taken from other programs (nor from this app's help) the rest of the time. `WM_HOTKEY` ->
  `switchFocus()`: the game has it -> `takeForeground()`, else `releaseForeground()` + `controller.focusTarget()`.
- `takeForeground()`: `bringToFront()`; if Windows refused, `AttachThreadInput` to the foreground thread for the
  `SetForegroundWindow`; then `LockSetForegroundWindow(LSFW_LOCK)` while any window of this process is in front.
  0.5 s later the foreground is checked and logged. Tekken 7 + JConfig takes it back even so (reproduced with a test
  program that attaches to the foreground thread's input; disabling the game window with `EnableWindow` did not stop
  it either), so then `setMouseOnly(true)`: `WS_EX_NOACTIVATE` on this window, so clicks reach the buttons without
  activating it and there is no tug of war. Off when the owner becomes `None` (the game closed).
- **Window opacity** (*View > Window opacity*, `opacity` / `opacityGameOnly` in `windowmanager.ini`):
  `MainWindow::applyOpacity` -> `QWidget::setWindowOpacity` (a layered window on Windows; needs a compositor on X11).
  With *Only while a game runs* it follows `FocusMonitor` (100 % while the owner is `None`); called from
  `updateFocusSwitch`.
- **Terminate key** (id 4, default `F2`, `killHotkey`): `FocusSwitch::DoublePress` (2 s); the first press beeps
  (`MessageBeep`) and logs, the second calls `controller.killTarget()` (Shutdown without the confirmation box, which
  cannot be answered while the game has the focus).

## HelixSR

```
 HelixPanel -- Set up / Update --> HelixSetup::installOrUpdate()
   Checking     GET api.github.com/.../releases -> Helix::latestRelease()   (newest non-prerelease 1.x zip, kSupportedMajor)
   Downloading  zip -> SHA-256 against the asset digest (or SHA256SUMS.txt); mismatch = deleted
   Extracting   tar.exe -xf  -> helixsr/HelixSR-<version>/
   Building     powershell helixsr-setup.ps1 -Yes   (Python, dxc, DLSS DLL, network; ~5-6 min)
   Cleaning     Helix::buildLeftovers() + older packages deleted, markBuilt()
 HelixPanel -- Install / Run --> HelixSetup::installIntoGame()
   python helixsr_driver.py <package> install|remove <game root>
```

- `HelixPanel` runs `probeSystem()` + `Helix::evaluate()` at start-up, on **Check this PC** and before every
  set-up: `Unsupported` stops it, `Experimental` adds a warning to the confirmation. The rules live in
  `HelixSystem.cpp` (`familyOf` reads the name, or the SIMD width for generic names like "AMD Radeon(TM) Graphics":
  32 lanes = RDNA, 64 = GCN / Vega).
- The decisions are in the pure `helix/` units (`tst_helix`); `HelixSetup` only runs the steps and
  reports them (`stateChanged`, `buildFinished`, `gameJobFinished`). One job at a time.
- A package counts as built only when `.windowmanager-built` exists in it, so a failed or cancelled
  build is never used and is removed. The set-up prompt (the DLSS download) is answered with `-Yes`
  after the user agrees in `HelixPanel`.
- `resources/helixsr_driver.py` (embedded, written to `helixsr/` before each job) imports HelixSR's
  `helixsr_install` module and installs the FSR drop-in for one folder, because the stock script
  works on a numbered list of every game it finds. HelixSR's files are never edited.
- After an install or removal the result is judged from the files (`Helix::scanGame`), not from the
  exit code. `scanGame` looks for `amd_fidelityfx_upscaler_dx12.dll` / `amd_fidelityfx_dx12.dll`
  (not `*.original.dll`) under the game root (`gameRootFor`: the exe folder, or the game folder when the
  exe is in bin / x64 / Win64 / Binaries). "Installed" means the DLL contains `HelixSR x.y.z`;
  "replaced" means HelixSR's record and backup exist but the DLL is the game's again.
- The HelixSR list shows every saved game; a game whose scan finds no FSR 3.1 DLL is disabled (greyed out, not
  selectable). `helixConfirmed` in `windowmanager.json` caches the last result, so usable games sort first.
- **Scans are asynchronous.** `Helix::scanGameAsync(context, exe, done)` runs `scanGame` on the global thread
  pool and calls `done` on the context's thread (dropped if the context is gone). `HelixPanel::rebuildGames`
  shows the list at once ("checking...") and fills each row as its scan returns; a generation counter drops the
  results of an older list. `HelixSetup::runGameJob` marks itself busy first, so nothing can overlap.
- **Installer self-test.** `HelixSetup::testDriver` builds a throw-away game under `helixsr/selftest`, runs the
  helper to install and remove, and checks the DLL changed and was restored byte for byte. On success the
  fingerprint of `helixsr_driver.py` (`Helix::driverFingerprint`) is stored in the package's
  `.windowmanager-built` marker, so it is not repeated until the helper script or the package changes. The
  helper exits with code 2 (`driver-incompatible: ...`) when HelixSR's functions no longer match
  (ImportError / AttributeError / TypeError / NameError); a failed test sets `driverOk() == false`, which
  `canInstall()` and the panel's buttons respect.
- `HelixIni::applyIni` edits the existing text line by line (section and key lookup is case-insensitive), so
  comments, unknown keys and the file's line endings survive.

## Process termination

`WindowManager::killTarget()` snapshots running processes and asks
`KillPlanner::plan()` which to terminate. The plan starts from the pids this app
launched or detected, the active window's pid, and processes whose image path is the
selected target; adds all their descendants; and adds loader processes only if they
started after the launch and are in that tree or orphaned. The ignore list and the
app's own pid are never included. Because `KillPlanner` has no Win32 dependency it is
unit-tested directly.

## Tests

| Suite | Covers |
|---|---|
| `tst_utilities` | name sanitising, unique ids, string/path helpers, size formatting |
| `tst_killplanner` | session scoping: unrelated loaders spared, descendants, orphans, pid reuse, ignore list |
| `tst_gamewatch` | the watch's lines: first sight of a window (hidden helpers left out until shown), size / place / frame / visibility / title changes, unchanged and class-only changes not logged, closed windows, other programs' titles kept out of the foreground line, mouse clip vs. free |
| `tst_fit` | fit maths (framed, borderless, negative-origin monitor, overscan, applied plan is stable) and the settle trigger (fires once, restarts on move/new window, ignores unusable windows, times out) |
| `tst_supersample` | letterbox (side bars, top/bottom bars, never exceeds the area), render size per factor and shape, parking position (behind the overlay vs beyond the desktop, with a monitor above/next to it), mouse mapping (pixel centres, black bars ignored, clamping while dragging, never past the last pixel), putting the game window back (every focus change; a fast loop stopped, then tried again after 10 s) |
| `tst_steammatch` | AppID from `steam_app_<id>` classes and pasted text (store URL, run URL, bare number), AppID beats a Wine loader image, another Steam game never matches, path / exe-name fallback |
| `tst_windowmanager` | `WindowManager` and `MonitorHelper` against a fake `PlatformBackend`: which windows a scan keeps and their order, the quiet refresh (keeps a chosen window, follows a resize, a closed splash screen replaced by the game window), borderless fullscreen, centring on another monitor, missing monitors, toggle messages, kill scope |
| `tst_steamlibrary` | `libraryfolders.vdf` (current and old format, Windows paths), appmanifest fields / install state / escapes, tool filter, main-exe choice (redistributables, crash reporters, Unreal shipping exe), a fake Steam folder with two libraries, and the sync into the saved games (add, keep the user's name, AppID for a game added by hand, uninstalled vs. drive not connected, moved exe) |
| `tst_x11backend` | Linux only: the real X11 backend against an xterm (list, title, pid, move/resize, frame strip/restore, process info, terminate); skips itself without a display |
| `tst_appconfig` | save/load round-trip, clearing profiles, corrupt/missing files, legacy keys, the HelixSR flag |
| `tst_display` | mode lists (sizes, refresh rates, best mode, portrait), scale steps relative to the recommended one, brightness percent, WMI instance matching, a game's display settings (JSON, sanitising, description), choosing the monitor, the plan (only what differs and the display supports) and the undo record (first value wins, JSON); on Windows a read-only probe of this PC's displays is printed |
| `tst_stats` | GPU counter instance names, busiest engine and video memory per process, CPU percent, frame rate and 1 % low (late and out-of-order presents, stale), dedicated adapter, the warnings, short GPU names, reading / editing the graphics-setting value, the floating bar's line, driver matching / dates / NVIDIA versions,
NVIDIA lookup ids (notebook vs desktop) and its answer, the log line, the sampler across samples and processes; on Windows a live sample of the test process is printed |
| `tst_gameconfig` | layout from the exe path (Win64 / Win32, native separators, not Unreal), the `.bat` game's exe, the keys per file (render scale above / below 100, resolution, left settings take the original or are removed), JConfig's fields (read / write / other bytes kept / wrong size), JSON and sanitising; against a fake Tekken folder: write (CRLF kept, copies only of changed files, nothing on a second run), a user's edit surviving, restore (originals back, copies gone), UTF-8 BOM kept, a game without config |
| `tst_focusswitch` | who has the focus (game, a second game process, app, other, no game), the toggle, labels, the press-twice rule of the terminate key, hotkey parsing (F-keys, letters, digits, named keys, modifiers, invalid) and text round trip |
| `tst_helix` | version parsing/ordering, release choice (no prereleases, the right zip), checksums, update decision, DLL version, game root, FSR DLL detection and installed / replaced state, async scan delivery, built-package marker and driver fingerprint, post-build leftovers, helixsr.ini parsing and in-place editing, GPU family and the can-this-PC-run-it verdict (a live probe of the test machine is printed too) |

The UI and the real OS backend (`platform/win32/`) are not unit-tested; they are checked by running the
app. New logic that can be written without Win32 or widgets (like `KillPlanner`) should
go in a Win32-free unit with a test.

## Documentation

```
README.md                       What the app is for, getting started, link to the wiki
docs/index.md                   Wiki home page (MkDocs site, https://robertotorino.github.io/WindowManager/)
docs/wiki/*.md                  The user documentation in full
docs/development/*.md           Building and testing, the docs site
docs/ARCHITECTURE.md, LINUX_PORT.md, HELIXSR_2.md   Development notes (also in the wiki)
docs/help/*.md                  In-app help: the highlights only, each page links to its wiki page;
                                embedded via resources/resources.qrc, excluded from the site
docs/assets/                    Logo, favicon, extra.css (the blue theme), screenshots/ (placeholders)
overrides/                      Theme overrides: the header (partials/header.html), no tabs on the home page and
                                the footer's link columns (main.html)
mkdocs.yml                      Site settings and navigation
.github/workflows/docs.yml      Builds the site (--strict) and publishes it to GitHub Pages
```

See [The docs site](development/docs-site.md).
