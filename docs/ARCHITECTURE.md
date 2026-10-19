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
  window/WindowManager.*        Win32 logic: scan, move, resize, styles, launch, kill
  window/MonitorHelper.*        Monitor enumeration and geometry
  window/WindowStabilizer.*     WinEvent hook that re-applies a window's rectangle
  window/KillPlanner.*          Pure logic: which processes belong to this session
  window/FitCalculator.*        Pure logic: fit-to-monitor maths (Fit::compute) and the
                                launch-settle trigger (Fit::Trigger)
  window/Supersample.*          Pure logic: letterbox, render size, parking position
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
  ui/GameRegistry.*             Saved-game helpers (names, entries, snapshot folders)
  ui/FitDialog.*                "Fit window to monitor" popup (measurements + correction)
  ui/ScaledView.*               Downscaled view: overlay window + DWM thumbnail of the game
  ui/InfoDialogs.*              About dialog
  ui/HelpDialog.*               Help > Help topics: topic list + search over docs/help/*.md (embedded via the .qrc)
  ui/LogViewerDialog.*          File > View logs...: app and per-game log viewer
  ui/HelixPanel.*               HelixSR tab: set-up / update with a timer, list of confirmed games, run
tests/                          QtTest suites (see below)
resources/                      Icon, arrow SVGs used by the stylesheet, Windows .rc, helixsr_driver.py
```

## How the pieces talk

```
 buttons / table ──click──▶ WindowManagerController ──▶ WindowManager (Win32)
        ▲                          │
        └────── refresh() ◀── statusChanged()
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
- **A new window action:** add a method to `WindowManager` (Win32), a slot on
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
   1. overlay: borderless, topmost, WS_EX_NOACTIVATE QWidget placed on the target monitor
   2. game window: restore, popup style (no frame), size = render size,
      parked at Supersample::parkingOrigin()  (behind the overlay, or beyond the desktop edge)
   3. DwmRegisterThumbnail(overlay, game) with the destination rectangle from
      Supersample::letterbox(); source = client area only
   4. SetForegroundWindow(game): keyboard / gamepad stay with the game
        │ every 400 ms
        ▼
 tick(): game gone -> stop();  restore if minimized;  re-assert size (max 5x);
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

## HelixSR

```
 HelixPanel -- Set up / Update --> HelixSetup::installOrUpdate()
   Checking     GET api.github.com/.../releases -> Helix::latestRelease()   (newest non-prerelease zip)
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
- Games on the HelixSR list have `helixConfirmed` set in `windowmanager.json`.
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
| `tst_fit` | fit maths (framed, borderless, negative-origin monitor, overscan, applied plan is stable) and the settle trigger (fires once, restarts on move/new window, ignores unusable windows, times out) |
| `tst_supersample` | letterbox (side bars, top/bottom bars, never exceeds the area), render size per factor and shape, parking position (behind the overlay vs beyond the desktop, with a monitor above/next to it), mouse mapping (pixel centres, black bars ignored, clamping while dragging, never past the last pixel) |
| `tst_appconfig` | save/load round-trip, clearing profiles, corrupt/missing files, legacy keys, the HelixSR flag |
| `tst_helix` | version parsing/ordering, release choice (no prereleases, the right zip), checksums, update decision, DLL version, game root, FSR DLL detection and installed / replaced state, async scan delivery, built-package marker and driver fingerprint, post-build leftovers, helixsr.ini parsing and in-place editing, GPU family and the can-this-PC-run-it verdict (a live probe of the test machine is printed too) |

The UI and the Win32 window code are not unit-tested; they are checked by running the
app. New logic that can be written without Win32 or widgets (like `KillPlanner`) should
go in a Win32-free unit with a test.
