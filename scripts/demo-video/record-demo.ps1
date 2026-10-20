<#
Records the demo video (wiki home page, YouTube): a one-minute tour of WindowManager with a stand-in game.

  pwsh scripts/demo-video/record-demo.ps1                # record, edit, write build/demo-video/
  pwsh scripts/demo-video/record-demo.ps1 -UpdateDocs    # also replace docs/assets/video/demo.mp4 + poster
  pwsh scripts/demo-video/record-demo.ps1 -EditOnly      # only redo captions / cards from the last recording

The tour (what is clicked, and each step's caption) is in the "Record" section; the title and end cards in "Edit".

It copies the built app to a separate folder (-DemoDir, default C:\WMDemo: a path without your user name,
because the app shows the game's path) with its own windowmanager.json, so your own setup is not touched.
It records a second monitor (by default the first one that is not the main one) and moves the real mouse
there for about a minute: keep your hands off the mouse and keyboard until it says it is done.
Needs ffmpeg (winget install Gyan.FFmpeg) and the .NET Framework C# compiler (part of Windows).
#>
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\..\build\windows-clang-debug'),
    [int]$Monitor = 0,   # 1-based, in the order Windows lists them; 0 = the first monitor that is not the main one
    [string]$DemoDir = 'C:\WMDemo',
    [string]$OutDir = (Join-Path $PSScriptRoot '..\..\build\demo-video'),
    [switch]$UpdateDocs,
    [switch]$KeepDemoDir,
    [switch]$EditOnly    # only redo the editing (captions, cards) from the last raw.mp4 and steps.csv
)
$ErrorActionPreference = 'Stop'
$BuildDir = [IO.Path]::GetFullPath($BuildDir)
$OutDir = [IO.Path]::GetFullPath($OutDir)
$docsVideo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..\docs\assets\video'))
$raw = "$OutDir\raw.mp4"

# --- Tools ----------------------------------------------------------------------------------------------

if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {   # installed in this session: PATH not reloaded
    $env:Path = [Environment]::GetEnvironmentVariable('Path', 'Machine') + ';' + [Environment]::GetEnvironmentVariable('Path', 'User')
}
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) { throw 'ffmpeg not found: winget install Gyan.FFmpeg' }
$csc = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if (-not (Test-Path $csc)) { throw "C# compiler not found: $csc" }
if (-not (Test-Path "$BuildDir\WindowManager.exe")) { throw "No WindowManager.exe in ${BuildDir}: build first" }

Add-Type -AssemblyName UIAutomationClient, UIAutomationTypes
Add-Type @"
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class Demo {
    [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, int x, int y, uint d, UIntPtr e);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint f, UIntPtr e);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint f);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    public struct POINT { public int X, Y; }
    struct RECT { public int L, T, R, B; }
    [StructLayout(LayoutKind.Sequential)] struct MONITORINFO { public int cbSize; public RECT rcMonitor, rcWork; public uint dwFlags; }
    delegate bool MonitorEnum(IntPtr m, IntPtr dc, IntPtr r, IntPtr d);
    [DllImport("user32.dll")] static extern bool EnumDisplayMonitors(IntPtr dc, IntPtr clip, MonitorEnum cb, IntPtr d);
    [DllImport("user32.dll")] static extern bool GetMonitorInfo(IntPtr m, ref MONITORINFO mi);

    // Physical pixels from here on, whatever the monitors' scale.
    public static void PerMonitor() { SetThreadDpiAwarenessContext(new IntPtr(-4)); }

    // { left, top, width, height, primary }
    public static List<int[]> Monitors() {
        PerMonitor();
        var list = new List<int[]>();
        EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (m, dc, r, d) => {
            var mi = new MONITORINFO(); mi.cbSize = Marshal.SizeOf(mi);
            GetMonitorInfo(m, ref mi);
            list.Add(new[] { mi.rcMonitor.L, mi.rcMonitor.T, mi.rcMonitor.R - mi.rcMonitor.L,
                             mi.rcMonitor.B - mi.rcMonitor.T, (int)(mi.dwFlags & 1) });
            return true;
        }, IntPtr.Zero);
        return list;
    }
}
"@
[Demo]::PerMonitor()
$UIA = [System.Windows.Automation.AutomationElement]

if (-not $EditOnly) {   # -EditOnly: re-edit the last recording (raw.mp4, steps.csv)

    # --- The monitor to record ------------------------------------------------------------------------------

    $monitors = [Demo]::Monitors()
    if ($Monitor -gt 0) {
        if ($Monitor -gt $monitors.Count) { throw "There are only $($monitors.Count) monitor(s)" }
        $mon = $monitors[$Monitor - 1]
    } else {
        $mon = $monitors | Where-Object { $_[4] -eq 0 } | Select-Object -First 1
        if (-not $mon) { throw 'Only one monitor: pass -Monitor 1 to record the main one (everything on it shows in the video)' }
    }
    $mx, $my, $mw, $mh = $mon[0], $mon[1], $mon[2], $mon[3]
    Write-Host "Recording the monitor at $mx,$my (${mw}x$mh)"
    if ($mw -ne 1920 -or $mh -ne 1080) {
        Write-Warning 'The tour is laid out for a 1920x1080 monitor at 100 % scale; the video is scaled to 1920x1080.'
    }

    # --- Demo copy of the app -------------------------------------------------------------------------------

    function Get-DemoApp { Get-Process WindowManager -ErrorAction SilentlyContinue | Where-Object Path -like "$DemoDir\*" | Select-Object -First 1 }
    function Stop-Demo {
        Get-Process DemoGame -ErrorAction SilentlyContinue | Where-Object Path -like "$DemoDir\*" | Stop-Process -Force
        $p = Get-DemoApp
        if ($p) { [void]$p.CloseMainWindow(); if (-not $p.WaitForExit(5000)) { $p | Stop-Process -Force } }
    }

    Stop-Demo
    if (Test-Path $DemoDir) {
        if (-not (Test-Path "$DemoDir\.wm-demo")) { throw "$DemoDir exists and was not made by this script: pass another -DemoDir" }
        Remove-Item -Recurse -Force $DemoDir
    }
    $gameDir = "$DemoDir\Games\Demo Game"
    New-Item -ItemType Directory -Force "$DemoDir\app", $gameDir, $OutDir | Out-Null
    Set-Content "$DemoDir\.wm-demo" 'Made by scripts/demo-video/record-demo.ps1; removed after recording.'

    & $csc /nologo /target:winexe "/out:$gameDir\DemoGame.exe" /r:System.Windows.Forms.dll /r:System.Drawing.dll "$PSScriptRoot\DemoGame.cs"
    if ($LASTEXITCODE) { throw 'DemoGame.cs did not compile' }

    Copy-Item "$BuildDir\WindowManager.exe", "$BuildDir\*.dll" "$DemoDir\app"
    foreach ($d in 'generic', 'iconengines', 'imageformats', 'networkinformation', 'platforms', 'styles', 'tls', 'helixsr') {
        if (Test-Path "$BuildDir\$d") { Copy-Item -Recurse "$BuildDir\$d" "$DemoDir\app" }
    }
    $gameExe = "$gameDir\DemoGame.exe" -replace '\\', '/'
    @{
        games = @(@{
            ApplicationPath = $gameExe; CaptureDir = 'captures/GAME_DEMO_GAME'; ChildExePath = ''; EbootIsoPath = ''
            GameApplication = 'DemoGame.exe'; HelixConfirmed = $false; Id = 'GAME_DEMO_GAME'; IsPatchable = 'false'
            LauncherType = 'STANDARD'; SavedName = 'DEMO_GAME'; SnapshotDir = 'snapshots/GAME_DEMO_GAME'
        })
        lastExePath = $gameExe
        profiles = @()
    } | ConvertTo-Json -Depth 5 | Set-Content "$DemoDir\app\windowmanager.json"
    # Everything on one monitor: not on top, no moving off the game's monitor, no fit box after Start.
    @(
        '[General]', 'alwaysOnTop=false', 'opacity=100', 'opacityGameOnly=true', 'confirmKill=false', 'fitCheck=false',
        'statsBar=true', 'miniBar=true', 'avoidGameMonitor=false', 'focusHotkey=F1', 'killHotkey=F2'
    ) | Set-Content "$DemoDir\app\windowmanager.ini"

    # --- Input helpers --------------------------------------------------------------------------------------

    function App-Window {
        $p = Get-DemoApp
        if (-not $p) { throw 'The demo copy of WindowManager is not running' }
        $cond = New-Object System.Windows.Automation.PropertyCondition($UIA::ProcessIdProperty, $p.Id)
        $UIA::RootElement.FindAll('Children', $cond) | Where-Object { $_.Current.Name -like 'WindowManager*' } | Select-Object -First 1
    }
    function Find-El([string]$name, [string]$type) {
        $cond = New-Object System.Windows.Automation.PropertyCondition($UIA::NameProperty, $name)
        foreach ($e in (App-Window).FindAll('Descendants', $cond)) {
            if ($e.Current.ControlType.ProgrammaticName -eq "ControlType.$type") { return $e }
        }
        throw "Not found in the app: $type '$name' (did the UI change? update the tour in record-demo.ps1)"
    }
    function Move-Mouse([int]$x, [int]$y, [int]$ms = 600) {
        $p = New-Object Demo+POINT; [void][Demo]::GetCursorPos([ref]$p)
        $steps = [Math]::Max(1, [int]($ms / 15))
        for ($i = 1; $i -le $steps; $i++) {
            $t = $i / $steps; $e = $t * $t * (3 - 2 * $t)   # smoothstep
            [void][Demo]::SetCursorPos([int]($p.X + ($x - $p.X) * $e), [int]($p.Y + ($y - $p.Y) * $e))
            Start-Sleep -Milliseconds 15
        }
    }
    function Click-At([int]$x, [int]$y) {
        if ($x -lt $mx -or $x -ge $mx + $mw -or $y -lt $my -or $y -ge $my + $mh) { throw "Click at $x,$y is off the recorded monitor" }
        Move-Mouse $x $y; Start-Sleep -Milliseconds 250
        [Demo]::mouse_event(0x2, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 80
        [Demo]::mouse_event(0x4, 0, 0, 0, [UIntPtr]::Zero)
    }
    function Click-El([string]$name, [string]$type = 'Button') {
        $r = (Find-El $name $type).Current.BoundingRectangle
        Click-At ([int]($r.X + $r.Width / 2)) ([int]($r.Y + $r.Height / 2))
    }
    function Select-Tab($name) { Click-El $name 'TabItem'; Start-Sleep -Milliseconds 700 }
    function Key-Down([byte]$vk) { [Demo]::keybd_event($vk, 0, 0, [UIntPtr]::Zero) }
    function Key-Up([byte]$vk) { [Demo]::keybd_event($vk, 0, 2, [UIntPtr]::Zero) }
    function Send-Key([byte]$vk) { Key-Down $vk; Start-Sleep -Milliseconds 60; Key-Up $vk }
    function Assert-AppFront {
        # keystrokes go to whatever has the focus: never type into another program
        [uint32]$fpid = 0; [void][Demo]::GetWindowThreadProcessId([Demo]::GetForegroundWindow(), [ref]$fpid)
        if ($fpid -ne (Get-DemoApp).Id) { throw 'The demo copy of WindowManager lost the focus: stopped before typing' }
    }
    function Set-Spin($label, [string]$value) {
        $r = (Find-El $label 'Text').Current.BoundingRectangle   # the spin box is right of its label
        Click-At ([int]($r.Right + 60)) ([int]($r.Y + $r.Height / 2)); Start-Sleep -Milliseconds 300
        Assert-AppFront
        Key-Down 0x11; Send-Key 0x41; Key-Up 0x11   # Ctrl+A
        Start-Sleep -Milliseconds 200
        foreach ($ch in $value.ToCharArray()) { Send-Key ([byte]$ch); Start-Sleep -Milliseconds 90 }
        Start-Sleep -Milliseconds 400
    }
    function Click-Game {
        $g = Get-Process DemoGame | Select-Object -First 1
        $cond = New-Object System.Windows.Automation.PropertyCondition($UIA::ProcessIdProperty, $g.Id)
        $r = $UIA::RootElement.FindFirst('Children', $cond).Current.BoundingRectangle
        Click-At ([int]($r.X + $r.Width * 0.3)) ([int]($r.Y + $r.Height * 0.6))
    }

    # --- Record ---------------------------------------------------------------------------------------------

    $env:WM_DEMO_X = $mx + 40; $env:WM_DEMO_Y = $my + 120   # the game inherits them through the app
    Start-Process "$DemoDir\app\WindowManager.exe" -WorkingDirectory "$DemoDir\app"
    for ($i = 0; $i -lt 40; $i++) {
        $p = Get-DemoApp
        if ($p -and $p.MainWindowHandle -ne 0) { break }
        Start-Sleep -Milliseconds 250
    }
    Start-Sleep 2
    [void][Demo]::SetWindowPos((Get-DemoApp).MainWindowHandle, [IntPtr]::Zero, $mx + 900, $my + 20, 0, 0, 0x1 -bor 0x4)
    Start-Sleep 3   # start-up: the Steam scan, the move to the other monitor's scale

    Write-Host 'Recording: hands off the mouse and keyboard for about a minute...'
    Move-Mouse ($mx + 1500) ($my + 780) 10
    $psi = New-Object Diagnostics.ProcessStartInfo 'ffmpeg', ("-hide_banner -loglevel error -y -f gdigrab -framerate 30 -draw_mouse 1 " +
        "-offset_x $mx -offset_y $my -video_size ${mw}x$mh -i desktop -c:v libx264 -preset veryfast -crf 16 -pix_fmt yuv420p `"$raw`"")
    $psi.UseShellExecute = $false; $psi.RedirectStandardInput = $true; $psi.CreateNoWindow = $true
    $ff = [Diagnostics.Process]::Start($psi)
    Start-Sleep -Milliseconds 700   # ffmpeg's first frame
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $steps = New-Object System.Collections.Generic.List[object]
    function Step([string]$caption, [scriptblock]$body) {
        $a = $clock.Elapsed.TotalSeconds
        & $body
        $steps.Add([pscustomobject]@{ start = $a; end = $clock.Elapsed.TotalSeconds; text = $caption })
    }

    # The tour. Each step's caption is shown while it runs.
    try {
        Step 'intro' { Start-Sleep 5 }
        Step 'Start launches the game. Its window shows up in the list.' {
            Select-Tab 'Window'; Click-El 'Start'; Start-Sleep 4 }
        Step 'Fit to monitor measures the frame and fits the picture to the monitor.' {
            Click-El 'Fit to monitor...'; Start-Sleep 3.5
            Click-El 'Apply correction'; Start-Sleep 1.5; Click-Game; Start-Sleep 3.5 }
        Step 'F1 switches between the game and WindowManager.' {
            Send-Key 0x70; Start-Sleep 3 }
        Step 'Windowed puts the title bar and frame back.' {
            Click-El 'Windowed'; Start-Sleep 3 }
        Step 'Exact sizes, down to the pixel.' {
            Select-Tab 'Position & Size'
            Set-Spin 'Width' '1280'; Set-Spin 'Height' '720'
            Click-El 'Set size'; Start-Sleep 3.5 }
        Step 'Snapshots of the game window, saved as PNG.' {
            Select-Tab 'Capture'; Click-El 'Capture'; Start-Sleep 3 }
        Step 'Kill ends the game.' {
            Select-Tab 'Window'; Click-El 'Kill'; Start-Sleep 3 }
        Step 'outro' { Move-Mouse ($mx + 1500) ($my + 780) 600; Start-Sleep 5 }
    }
    finally {
        $ff.StandardInput.Write('q'); $ff.StandardInput.Flush()
        [void]$ff.WaitForExit(15000)
        Stop-Demo
        Write-Host 'Recording done: mouse and keyboard are yours again.'
    }
    $steps | Export-Csv "$OutDir\steps.csv" -NoTypeInformation
} else {
    $steps = Import-Csv "$OutDir\steps.csv" | ForEach-Object {
        [pscustomobject]@{ start = [double]$_.start; end = [double]$_.end; text = $_.text } }
}

# --- Edit: title card, a caption per step, end card -----------------------------------------------------

$edit = "$OutDir\edit"
New-Item -ItemType Directory -Force $edit | Out-Null
Copy-Item "$env:WINDIR\Fonts\seguisb.ttf", "$env:WINDIR\Fonts\segoeui.ttf" $edit   # relative: no C: to escape
$dur = [double](ffprobe -v error -show_entries format=duration -of csv=p=0 $raw)
$utf8 = New-Object System.Text.UTF8Encoding $false
function TextFile($name, $text) { [IO.File]::WriteAllText("$edit\$name", $text, $utf8); $name }
function Fade($a, $b) { "alpha='if(lt(t,$a+0.35),(t-$a)/0.35,if(gt(t,$b-0.35),($b-t)/0.35,1))'" }
function Text($file, $font, $size, $color, $y, $a, $b) {
    "drawtext=fontfile=$font`:textfile=$file`:fontsize=$size`:fontcolor=$color`:x=(w-text_w)/2:y=$y`:$(Fade $a $b)"
}
$introEnd = $steps[0].end; $outroStart = $steps[-1].start; $end = $dur + 1
$f = @("scale=1920:1080", "fade=t=in:st=0:d=0.6",
       "drawbox=x=0:y=ih-96:w=iw:h=96:color=0x0b0f1a@0.82:t=fill")   # caption bar (also hides desktop watermarks)
$f += "drawbox=x=0:y=0:w=iw:h=ih-96:color=0x0b0f1a@0.72:t=fill:enable='lt(t,$introEnd)'"
$f += Text (TextFile t1.txt 'WindowManager') seguisb.ttf 120 white 330 0 $introEnd
$f += Text (TextFile t2.txt "Take control of a game's window on Windows") segoeui.ttf 46 '0xcfe3ff' 490 0.3 $introEnd
$f += Text (TextFile t3.txt 'Borderless full screen  ·  exact sizes  ·  fit to monitor  ·  snapshots') segoeui.ttf 34 '0x9fb3cc' 575 0.6 $introEnd
$f += Text (TextFile t4.txt 'Shown with a stand-in game window') segoeui.ttf 34 '0xcfe3ff' 'h-66' 0.6 $introEnd
$i = 0
foreach ($st in $steps | Where-Object { $_.text -notin 'intro', 'outro' }) {
    $i++
    $f += (Text (TextFile "c$i.txt" $st.text) seguisb.ttf 40 white 'h-70' $st.start $st.end) + ":enable='between(t,$($st.start),$($st.end))'"
}
$f += "drawbox=x=0:y=0:w=iw:h=ih-96:color=0x0b0f1a@0.78:t=fill:enable='gte(t,$outroStart)'"
$f += Text t1.txt seguisb.ttf 110 white 330 $outroStart $end
$f += Text (TextFile e2.txt 'Free and open source') segoeui.ttf 46 '0xcfe3ff' 480 ($outroStart + 0.3) $end
$f += Text (TextFile e3.txt 'Docs: robertotorino.github.io/WindowManager') segoeui.ttf 38 '0x9fd0ff' 570 ($outroStart + 0.6) $end
$f += Text (TextFile e4.txt 'Code: github.com/RobertoTorino/WindowManager') segoeui.ttf 38 '0x9fd0ff' 630 ($outroStart + 0.6) $end
$f += "fade=t=out:st=$([Math]::Round($dur - 0.8, 2)):d=0.8"
[IO.File]::WriteAllText("$edit\filter.txt", ($f -join ",`n"), $utf8)

$video1080 = "$OutDir\windowmanager-demo-1080p.mp4"
$video720 = "$OutDir\windowmanager-demo-720p.mp4"
Push-Location $edit
try {
    ffmpeg -hide_banner -loglevel error -y -i $raw -/vf filter.txt -c:v libx264 -preset slow -crf 18 -pix_fmt yuv420p -r 30 -movflags +faststart $video1080
    if ($LASTEXITCODE) { throw 'ffmpeg could not edit the recording' }
} finally { Pop-Location }
ffmpeg -hide_banner -loglevel error -y -i $video1080 -vf scale=1280:-2 -c:v libx264 -preset slow -crf 26 -pix_fmt yuv420p -movflags +faststart $video720
# poster: the middle of the F1 step (the game full screen with the app over it)
$f1 = $steps | Where-Object text -like 'F1*'
ffmpeg -hide_banner -loglevel error -y -ss ([Math]::Round(($f1.start + $f1.end) / 2, 2)) -i $video720 -frames:v 1 -q:v 4 "$OutDir\demo-poster.jpg"
if ($UpdateDocs) {
    New-Item -ItemType Directory -Force $docsVideo | Out-Null
    Copy-Item $video720 "$docsVideo\demo.mp4"
    Copy-Item "$OutDir\demo-poster.jpg" "$docsVideo\demo-poster.jpg"
    Write-Host "Updated $docsVideo"
}
if (-not $EditOnly -and -not $KeepDemoDir) { Remove-Item -Recurse -Force $DemoDir }

Write-Host "YouTube (1080p): $video1080"
Write-Host "Wiki (720p):     $video720"
