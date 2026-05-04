Contains logic to launch applications:


```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description GUI to configure, run, and kill emulator executables.
; * @class EmulatorConfigGui
; * @location lib/ui/EmulatorConfigGui.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\config\ConfigManager.ahk
#Include DialogsGui.ahk
#Include ..\window\WindowManager.ahk
#Include ..\emulator\tools\RomScanner.ahk

class EmulatorConfigGui {
    static MainGui := ""

    ; Added 'RomExts' to define supported file types
    static Emulators := [{ Name: "DOLPHIN", Section: "DOLPHIN_PATH", Key: "DolphinPath", RomExts: ["gcm", "iso", "rvz", "wbfs"] }, { Name: "DUCKSTATION", Section: "DUCKSTATION_PATH", Key: "DuckStationPath", RomExts: ["bin", "chd", "cue", "iso"] }, { Name: "PCSX2", Section: "PCSX2_PATH", Key: "Pcsx2Path", RomExts: ["bin", "chd", "gz", "iso"] }, { Name: "PPSSPP", Section: "PPSSPP_PATH", Key: "PpssppPath", RomExts: ["cso", "elf", "iso", "pbp"] }, { Name: "REDREAM", Section: "REDREAM_PATH", Key: "RedreamPath", RomExts: ["gdi", "cdi", "chd"] }, { Name: "RPCS3", Section: "RPCS3_PATH", Key: "Rpcs3Path", RomExts: ["EBOOT.BIN"] }, { Name: "RPCS3_FIGHTER", Section: "RPCS3_FIGHTER_PATH", Key: "Rpcs3FighterPath", RomExts: ["EBOOT.BIN"] }, { Name: "RPCS3_SHOOTER", Section: "RPCS3_SHOOTER_PATH", Key: "Rpcs3ShooterPath", RomExts: ["EBOOT.BIN"] }, { Name: "RPCS3_TCRS", Section: "RPCS3_TCRS_PATH", Key: "Rpcs3TcrsPath", RomExts: ["EBOOT.BIN"] }, { Name: "SHADPS4", Section: "SHADPS4_PATH", Key: "ShadPs4Path", RomExts: ["bin"] }, { Name: "SHADPS4_GUI", Section: "SHADPS4_GUI_PATH", Key: "ShadPs4GuiPath" }, { Name: "TEKNO", Section: "TEKNO_PATH", Key: "TeknoPath" }, { Name: "VITA3K", Section: "VITA3K_PATH", Key: "Vita3kPath" }, { Name: "VITA3K_3830", Section: "VITA3K_3830_PATH", Key: "Vita3k3830Path" }, { Name: "VIVANONNO", Section: "VIVANONNO_PATH", Key: "VivaNonnoPath", RomExts: ["zip"] }, { Name: "YUZU", Section: "YUZU_PATH", Key: "YuzuPath", RomExts: ["nsp", "xci"] },
    ]

    static Show() {
        if (this.MainGui)
            this.MainGui.Destroy()
        this.MainGui := Gui("-Caption +Border +ToolWindow +AlwaysOnTop", "Nexus :: Configure Emulators")

        ; ---- Snap Gui ----
        WindowManagerGui.RegisterForSnapping(this.MainGui.Hwnd)

        this.MainGui.BackColor := "2A2A2A"
        this.MainGui.SetFont("s12 cWhite", "Segoe UI")

        guiW := 805

        title := this.MainGui.Add("Text", "x0 y0 w" (guiW - 30) " h30 +0x200 Background2A2A2A", "  Nexus :: Configure Emulators")
        title.OnEvent("Click", (*) => PostMessage(0xA1, 2, 0, this.MainGui.Hwnd))
        this.MainGui.Add("Text", "x+0 yp w30 h30 +0x200 +Center Background2A2A2A cRed", "✕").OnEvent("Click", (*) => this.MainGui.Destroy())

        y := 45
        for index, emu in this.Emulators {
            this.MainGui.Add("Button", "x-100 y-100 w0 h0 Default", "")
            currentPath := IniRead(ConfigManager.IniPath, emu.Section, emu.Key, "")

            this.MainGui.Add("Text", "x10 y" y " w125 h26 Right", emu.Name ":")
            edt := this.MainGui.Add("Edit", "x+10 yp h26 w510 +0x200 ReadOnly Background2A2A2A", currentPath)

            this.BtnAddTheme(" 📂 ", this.OnBrowse.Bind(this, emu, edt), "x+5 yp +0x200 Background2B3B45")
            this.BtnAddTheme(" ▶️ ", this.OnRun.Bind(this, edt), "x+5 yp Background0C660C")
            this.BtnAddTheme(" ❌ ", this.OnKill.Bind(this, edt), "x+5 yp Background6E0000")

            ; [NEW] Render Scan Button if supported
            if (emu.HasOwnProp("RomExts")) {
                this.BtnAddTheme(" 💿 ", this.OnScan.Bind(this, emu), "x+5 yp Background4A2A5A")
            }

            y += 35
        }
        this.MainGui.Show("w" guiW " h" (y + 10))
    }

    static BtnAddTheme(label, callback, options) {
        btn := this.MainGui.Add("Text", options " h26 +0x200 +Center +Border", label)
        btn.OnEvent("Click", callback)
        return btn
    }

    static OnScan(emu, *) {
        if (!emu.HasOwnProp("RomExts"))
            return

        RomScanner.Scan(emu.Name, emu.RomExts)
    }

    static OnBrowse(emu, editCtrl, *) {
        newPath := FileSelect(3, editCtrl.Value, "Select " emu.Name " Executable", "Applications (*.exe)")
        if (newPath != "") {
            editCtrl.Value := newPath
            IniWrite(newPath, ConfigManager.IniPath, emu.Section, emu.Key)
        }
    }

    static OnRun(editCtrl, *) {
        exePath := editCtrl.Value
        if (exePath == "")
            return

        SplitPath(exePath, &exeName, &dir)

        if (exeName = "TeknoParrotUi.exe") {
            WindowManager.ForceKillAll()
            Sleep(200)
        }

        try Run(exePath, dir)
    }

    static OnKill(editCtrl, *) {
        if (editCtrl.Value == "")
            return
        SplitPath(editCtrl.Value, &exeName)

        if (exeName = "TeknoParrotUi.exe") {
            WindowManager.ForceKillAll()
            DialogsGui.CustomTrayTip("TeknoParrot (All Processes) Killed", 1)
            return
        }

        if ProcessExist(exeName)
            ProcessClose(exeName)
        DialogsGui.CustomTrayTip("Terminated: " exeName, 1)
    }
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description Utilities Module (v2) - Common helper functions for Nexus.
; * @class Utilities
; * @location lib/core/Utilities.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include Logger.ahk ; [RESTORED] Required for GetCommandOutput logging

class Utilities {

    ; SanitizeName(str)
    ; Removes invalid characters, ensures single underscores.
    ; [UPDATED] Now supports Unicode (Japanese, Chinese, etc.)
    static SanitizeName(str) {
        if (str == "")
            return ""

        ; 1. Replace common separators (Space, Dash, Dot, Colon, Brackets) with Underscore
        Clean := RegExReplace(str, "[ \-\.\:\[\]\(\)]", "_")

        ; 2. Remove anything that is NOT Alphanumeric, Underscore, or Unicode (\x{0080}-\x{FFFF})
        ; This ensures we don't delete Japanese characters
        Clean := RegExReplace(Clean, "[^a-zA-Z0-9_\x{0080}-\x{FFFF}]", "")

        ; 3. Collapse multiple underscores into one
        Clean := RegExReplace(Clean, "_+", "_")

        ; 4. Trim leading and trailing underscores (Crucial for file paths)
        Clean := Trim(Clean, "_")

        return StrUpper(Clean)
    }

    ; JoinArray(arr, sep)
    ; Joins array elements with separator
    static JoinArray(arr, sep := ",") {
        str := ""
        for index, val in arr {
            if (str != "")
                str .= sep
            str .= val
        }
        return str
    }

    ; GetCommandOutput(cmd)
    ; Executes a command via ComSpec and captures stdout
    static GetCommandOutput(cmd) {
        tmpFile := A_Temp "\cmd_output.txt"
        ; Wrap the entire cmd in double-quotes to preserve quoted paths inside
        fullCmd := A_ComSpec . " /c `"" . cmd . " > `"" . tmpFile . "`" 2>&1`""

        ; [RESTORED] Logging logic
        if IsSet(Logger)
            Logger.Debug("Utilities: Running command: " . fullCmd)

        try {
            RunWait(fullCmd, , "Hide")

            if FileExist(tmpFile) {
                output := FileRead(tmpFile)
                FileDelete(tmpFile)

                if IsSet(Logger)
                    Logger.Debug("Utilities: Command output: " . output)

                return Trim(output)
            }
        } catch as err {
            if IsSet(Logger)
                Logger.Error("Utilities: GetCommandOutput failed: " . err.Message)
        }
        return ""
    }

    ; TrimQuotesAndSpaces(str)
    ; Removes leading/trailing quotes and spaces
    static TrimQuotesAndSpaces(str) {
        str := Trim(str)  ; trim spaces first

        ; Remove leading double quote
        while (SubStr(str, 1, 1) = '"')
            str := SubStr(str, 2)

        ; Remove trailing double quote (V2 SubStr: -1 is last char)
        while (SubStr(str, -1) = '"')
            str := SubStr(str, 1, StrLen(str) - 1)

        return str
    }

    ; FileNameNoExt(filePath)
    ; Extracts filename without extension
    static FileNameNoExt(filePath) {
        SplitPath(filePath, , , , &fileNoExt)
        return fileNoExt
    }

    ; GetFileExtension(filePath)
    ; Extracts file extension (including dot)
    static GetFileExtension(filePath) {
        SplitPath(filePath, , , &ext)
        return ext != "" ? "." ext : ""
    }

    ; IsValidExePath(path)
    ; Validates that path points to a real executable file
    static IsValidExePath(path) {
        if (path = "" || !FileExist(path))
            return false

        ; Optimized check
        return (SubStr(path, -4) = ".exe")
    }

    ; IsValidIsoPath(path)
    ; Validates that path points to an ISO/CSO/RVZ etc
    static IsValidIsoPath(path) {
        if (path = "" || !FileExist(path))
            return false

        ext := SubStr(path, -3)
        ; V2 string comparison is case-insensitive by default
        return (ext = "iso" || ext = "cso" || ext = "rvz" || SubStr(path, -4) = ".iso" || SubStr(path, -4) = ".cso")
    }

    ; FormatFileSize(bytes)
    ; Converts bytes to human-readable format (KB, MB, GB)
    static FormatFileSize(bytes) {
        if (bytes < 1024)
            return bytes . " B"
        else if (bytes < 1024 * 1024)
            return Round(bytes / 1024, 2) . " KB"
        else if (bytes < 1024 * 1024 * 1024)
            return Round(bytes / (1024 * 1024), 2) . " MB"
        else
            return Round(bytes / (1024 * 1024 * 1024), 2) . " GB"
    }

    ; GetCurrentTimestamp()
    static GetCurrentTimestamp() {
        return FormatTime(, "yyyy-MM-dd HH:mm:ss")
    }

    ; GetDateTimestampShort()
    static GetDateTimestampShort() {
        return FormatTime(, "yyyy-MM-dd_HH-mm-ss")
    }

    ; CustomTrayTip(text, iconType)
    ; Wrapper for TrayTip with consistent titling
    ; iconType: 1=Info, 2=Warning, 3=Error
    static CustomTrayTip(text, iconType := 1) {
        title := "Nexus"
        options := (iconType == 2) ? 2 : (iconType == 3) ? 3 : 1
        try {
            TrayTip(text, title, options)
            SetTimer(() => TrayTip(), -3000)
        }
    }

    ; IsInternetAvailable()
    ; Checks local network adapter status via WinAPI
    static IsInternetAvailable() {
        return DllCall("Wininet.dll\InternetGetConnectedState", "UInt*", 0, "UInt", 0)
    }

    ; GenerateUniqueId(friendlyName, existingGamesMap)
    ; Generates a strictly unique ID
    static GenerateUniqueId(friendlyName, existingGamesMap) {
        ; 1. Get the clean base (Uses the new Unicode-Safe logic)
        clean := this.SanitizeName(friendlyName)

        if (clean == "")
            clean := "GAME"

        ; 2. Form the base ID
        baseId := "GAME_" . clean

        ; 3. Check for collisions and increment if necessary
        finalId := baseId
        counter := 1

        ; Loop until we find an ID that isn't in the map
        while (existingGamesMap.Has(finalId)) {
            counter++
            finalId := baseId . "_" . counter
        }

        return finalId
    }

    ; --- MONITOR & GEOMETRY HELPERS ---

        static GetMonitorCount() {
            return MonitorGetCount()
        }

        static GetMonitorGeometry(index) {
            if (index > MonitorGetCount())
                return false

            try {
                MonitorGet(index, &L, &T, &R, &B)
                width := R - L
                height := B - T

                ; Safety check
                if (width <= 0 || height <= 0)
                    return false

                return {Left: L, Top: T, Right: R, Bottom: B, Width: width, Height: height}
            } catch {
                return false
            }
        }

        static GetMonitorIndexFromPoint(x, y) {
            count := MonitorGetCount()
            Loop count {
                try {
                    MonitorGet(A_Index, &L, &T, &R, &B)
                    if (x >= L && x < R && y >= T && y < B)
                        return A_Index
                }
            }
            return 1 ; Default to Primary
        }

        ; --- LOGGING ROUTINE ---
        ; Call this method from Nexus.ahk at startup
        static LogMonitorStats() {
            if !IsSet(Logger)
                return

            count := this.GetMonitorCount()
            Logger.Debug("--------------------------------------------------")
            Logger.Debug("SYSTEM MONITOR INFO")
            Logger.Debug("Monitor Count: " . count)

            Loop count {
                m := this.GetMonitorGeometry(A_Index)
                if (m) {
                    msg := "Monitor " . A_Index . ": "
                         . "L=" . m.Left . " T=" . m.Top . " "
                         . "W=" . m.Width . " H=" . m.Height
                    Logger.Debug(msg)
                } else {
                    Logger.Warn("Monitor " . A_Index . " geometry could not be read.")
                }
            }

            ; Specific check for Monitor 2 (as per your legacy code)
            if (count < 2) {
                Logger.Info("Monitor 2 not available (Single Monitor Setup).")
            }

            Logger.Debug("--------------------------------------------------")
        }
    }
```

---

```
#Requir**es AutoHotkey v2.0
; ==============================================================================
; * @description Contains the correct arguments for launching games in fullscreen without the Gui.
; * @class Pcsx2Launcher
; * @location lib/emulator/types/Pcsx2Launcher.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\EmulatorBase.ahk
#Include ..\..\window\WindowManager.ahk
#Include ..\..\capture\CaptureManager.ahk

class Pcsx2Launcher extends EmulatorBase {

    Launch(gameObj) {
        this.GameId := gameObj.Id

        ; 1. Get Emulator Path
        emuPath := this.GetEmulatorPath("PCSX2_PATH", "Pcsx2Path")
        if !emuPath
            return false

        SplitPath(emuPath, &exeName, &emuDir)

        ; 2. Path Validation
        rawPath := gameObj.HasProp("ApplicationPath") ? gameObj.ApplicationPath : ""
        if (rawPath == "" && gameObj.HasProp("EbootIsoPath"))
            rawPath := gameObj.EbootIsoPath

        ; --- FIX 1: TRACK UI SESSION ---
        if (rawPath == "") {
            Logger.Info("PCSX2: No ISO selected, launching UI.")
            try {
                Run(emuPath, emuDir, , &guiPid)
                if (guiPid > 0)
                    this.TrackProcess(guiPid, emuPath, "PCSX2_UI")
                return true
            } catch {
                return false
            }
        }

        ; 3. Normalization (Force Backslashes for PCSX2 CLI)
        isoPath := StrReplace(rawPath, "/", "\")

        ; 4. Prep
        this.KillProcess(exeName)
        CaptureManager.CurrentProcessName := exeName

        ; 5. Launch Arguments
        ; -batch (Exit on close) -fullscreen -- (File separator)
        runCmd := Format('"{1}" -batch -fullscreen -- "{2}"', emuPath, isoPath)
        Logger.Info("Launching PCSX2: " runCmd)

        try {
            ; --- FIX 2: REMOVE LEGACY "UseErrorLevel" ---
            Run(runCmd, emuDir, , &newPid)

            if (newPid > 0) {
                Logger.Info("PCSX2Launcher: Process started successfully. PID: " . newPid, "PCSX2Launcher")

                ; Hook into Process Manager
                this.TrackProcess(newPid, emuPath, gameObj.Id)

                ; 6. Window Management ("Ninja Move")
                ; Wait up to 3 seconds for the window to appear so we can snap it
                if WinWait("ahk_pid " newPid, , 3) {
                    WindowManager.SetGameContext("ahk_pid " newPid, 1)
                    Logger.Info("PCSX2 Launched & Moved (PID: " newPid ")")
                    return true
                }

                Logger.Warn("PCSX2 launched, but window not found within timeout.")
                return true
            }
            return false
        } catch as err {
            Logger.Error("PCSX2 Launch Failed: " err.Message)
            return false
        }
    }
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description Contains the correct arguments for launching games in fullscreen without the Gui.
; * @class PpssppLauncher
; * @location lib/emulator/types/PpssppLauncher.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\EmulatorBase.ahk
#Include ..\..\window\WindowManager.ahk

class PpssppLauncher extends EmulatorBase {

    Launch(gameObj) {
        ; 1. Safe Property Access (Handles Map vs Object distinction)
        ; We try to get 'EbootIsoPath', but fallback to 'ApplicationPath' which always exists.
        isoPath := ""
        gameId := ""

        if (gameObj is Map) {
            gameId := gameObj.Has("Id") ? gameObj["Id"] : "Unknown"
            if gameObj.Has("EbootIsoPath")
                isoPath := gameObj["EbootIsoPath"]
            else if gameObj.Has("ApplicationPath")
                isoPath := gameObj["ApplicationPath"]
        }
        else {
            ; It's a standard Object
            gameId := gameObj.HasOwnProp("Id") ? gameObj.Id : "Unknown"
            if gameObj.HasOwnProp("EbootIsoPath")
                isoPath := gameObj.EbootIsoPath
            else if gameObj.HasOwnProp("ApplicationPath")
                isoPath := gameObj.ApplicationPath
        }

        this.GameId := gameId

        ; 2. Get Emulator Path
        emuPath := this.GetEmulatorPath("PPSSPP_PATH", "PpssppPath")
        if !emuPath
            return false

        SplitPath(emuPath, &exeName, &emuDir)

        ; 3. Handle "Launch UI Only" (No ROM provided)
        if (isoPath == "") {
            try {
                Run(emuPath, emuDir, , &newPid)
                this.TrackProcess(newPid, emuPath, gameId)
                return true
            } catch {
                return false
            }
        }

        ; 4. Cleanup & Launch
        this.KillProcess(exeName)

        ; Construct command: "path/to/emu.exe" --fullscreen "path/to/rom.iso"
        runCmd := Format('"{1}" --fullscreen "{2}"', emuPath, isoPath)
        Logger.Info("Launching PPSSPP: " runCmd)

        try {
            Run(runCmd, emuDir, , &newPid)

            if (newPid > 0) {
                Logger.Info("PPSSPPLauncher: Process started successfully. PID: " . newPid, "PpssppLauncher")
                this.TrackProcess(newPid, emuPath, gameId)
                WindowManager.SetGameContext("ahk_pid " newPid)
                return true
            }
            return false
        } catch as err {
            ; THIS LINE IS THE KEY:
            Logger.Error(Format("LAUNCH CRASH: {1} | File: {2} | Line: {3}", err.Message, err.File, err.Line))
            return false
        }
    }
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description Universal Launcher that coordinates Build + Game Patches
; * @class Rpcs3UniversalLauncher
; * @location lib/emulator/types/Rpcs3Launcher.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\EmulatorBase.ahk
#Include ..\..\window\WindowManager.ahk
#Include ..\..\capture\CaptureManager.ahk
#Include ..\..\ui\DialogsGui.ahk
#Include ..\..\config\ConfigManager.ahk
#Include ..\..\core\Logger.ahk

; --- INHERIT FROM EMULATORBASE ---
class Rpcs3UniversalLauncher extends EmulatorBase {

    Launch(gameMap) {
        Logger.Info("RPCS3 Launcher: Starting launch sequence...", "Rpcs3UniversalLauncher")

        ; Universal map adapter
        game := {}
        if (Type(gameMap) == "Map") {
            for k, v in gameMap
                game.%k% := v
        } else {
            game := gameMap
        }

        this.GameId := game.HasProp("Id") ? game.Id : ""

        ; 1. Path validation
        rawPath := game.HasProp("ApplicationPath") ? game.ApplicationPath : ""
        if (rawPath == "" && game.HasProp("EbootIsoPath"))
            rawPath := game.EbootIsoPath

        Logger.Debug("RPCS3 Launcher: Raw Path from Config: " . rawPath)

        if (rawPath == "") {
            Logger.Error("RPCS3 Launcher: Path is empty.")
            DialogsGui.CustomMsgBox("Launch Error", "Game file (EBOOT/ISO) path is missing.", 0x10)
            return false
        }

        ; FIX 1: Normalize Path
        gamePath := StrReplace(rawPath, "/", "\")

        ; 2. Config Selection Logic
        iniKey := "Rpcs3Path"
        iniSec := "RPCS3_PATH"
        currentType := "Standard"

        if (game.HasProp("LauncherType")) {
            currentType := game.LauncherType
            Logger.Debug("RPCS3 Launcher: Detected Type: " . currentType)

            switch StrUpper(game.LauncherType) {
                case "FIGHTER", "RPCS3_FIGHTER":
                    iniSec := "RPCS3_FIGHTER_PATH"
                    iniKey := "Rpcs3FighterPath"

                case "SHOOTER", "RPCS3_SHOOTER":
                    iniSec := "RPCS3_SHOOTER_PATH"
                    iniKey := "Rpcs3ShooterPath"

                case "TCRS", "RPCS3_TCRS":
                    iniSec := "RPCS3_TCRS_PATH"
                    iniKey := "Rpcs3TcrsPath"
            }
        }

        Logger.Debug("RPCS3 Launcher: Looking for Emulator in INI [" . iniSec . "] Key: " . iniKey)

        ; Note: We use IniRead directly here instead of GetEmulatorPath because of the dynamic section logic
        emuPath := IniRead(ConfigManager.IniPath, iniSec, iniKey, "")

        if (emuPath == "" || !FileExist(emuPath)) {
            err := "RPCS3 Launcher: Emulator EXE not found at '" . emuPath . "' using section [" . iniSec . "]"
            Logger.Error(err)
            DialogsGui.CustomMsgBox("Emulator Error", "RPCS3 Executable not found.`nCheck logs/config.", 0x10)
            return false
        }

        Logger.Info("RPCS3 Launcher: Emulator found: " . emuPath, "Rpcs3UniversalLauncher")

        SplitPath(emuPath, &emuExe, &emuDir)

        ; Tell CaptureManager what to watch
        CaptureManager.CurrentProcessName := emuExe

        ; 3. Run Command
        runCmd := Format('"{1}" --no-gui --fullscreen "{2}"', emuPath, gamePath)
        Logger.Debug("RPCS3 Launcher: Command Line: " . runCmd)

        try {
            Run(runCmd, emuDir, , &outPid)
            this.Pid := outPid
            Logger.Info("RPCS3 Launcher: Process started successfully. PID: " . this.Pid, "Rpcs3UniversalLauncher")

            if (this.Pid > 0) {
                ; --- THE FIX: TRACK PROCESS ---
                ; Connects to ProcessManager/ConfigManager
                this.TrackProcess(this.Pid, emuPath, this.GameId)

                WindowManager.SetGameContext("ahk_pid " this.Pid, 1)

                if (WinWait("ahk_pid " this.Pid, , 10)) {
                    WinActivate("ahk_pid " this.Pid)
                    WindowManager.SetGameContext("ahk_pid " this.Pid, 1)
                    Logger.Info("RPCS3 Launcher: Window activated and context set.", "Rpcs3UniversalLauncher")
                } else {
                    Logger.Warn("RPCS3 Launcher: Process started, but window did not appear within 10s.")
                }
            }
            return true

        } catch as err {
            Logger.Error("RPCS3 Launcher: Run() Exception: " . err.Message)
            DialogsGui.CustomMsgBox("Launch Failed", "RPCS3 Error: " . err.Message, 0x10)
            return false
        }
    }

    ; Removed manual Stop() to use EmulatorBase.Stop() instead
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description ShadPs4Launcher PS4 emulator
; * @class ShadPs4Launcher
; * @location lib/emulator/types/ShadPs4Launcher.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\EmulatorBase.ahk
#Include ..\..\window\WindowManager.ahk
#Include ..\..\capture\CaptureManager.ahk
#Include ..\..\ui\DialogsGui.ahk
#Include ..\..\config\ConfigManager.ahk
#Include ..\..\core\Logger.ahk

class ShadPs4Launcher extends EmulatorBase {

    Launch(gameMap) {
        Logger.Info("ShadPS4 Launcher: Starting launch sequence...", "ShadPs4Launcher")

        ; Universal map adapter
        game := {}
        if (Type(gameMap) == "Map") {
            for k, v in gameMap
                game.%k% := v
        } else {
            game := gameMap
        }

        this.GameId := game.HasProp("Id") ? game.Id : ""

        ; 1. Path validation
        rawPath := game.HasProp("ApplicationPath") ? game.ApplicationPath : ""
        Logger.Debug("ShadPS4 Launcher: Raw Path from Config: " . rawPath)

        if (rawPath == "") {
            Logger.Error("ShadPS4 Launcher: ApplicationPath is empty.")
            DialogsGui.CustomMsgBox("Launch Error", "Game file (eboot.bin) path is missing.", 0x10)
            return false
        }

        ; Normalize slashes
        gamePath := StrReplace(rawPath, "/", "\")

        ; 2. Locate emulator
        emuPath := IniRead(ConfigManager.IniPath, "SHADPS4_PATH", "ShadPs4Path", "")
        if (emuPath == "" || !FileExist(emuPath)) {
            Logger.Error("ShadPS4 Launcher: Emulator EXE not found at '" . emuPath . "'")
            DialogsGui.CustomMsgBox("Emulator Error", "shadPS4 executable not found.`nCheck logs/config.", 0x10)
            return false
        }

        SplitPath(emuPath, &emuExe, &emuDir)
        Logger.Info("ShadPS4 Launcher: Using exe: " . emuExe, "ShadPs4Launcher")

        ; Tell CaptureManager what to watch
        CaptureManager.CurrentProcessName := emuExe
        this.KillProcess(emuExe)

        ; shadPS4 CLI:  -f true     = fullscreen (requires explicit true/false value)
        ;               -g "path"   = game to launch
        runCmd := Format('"{1}" -f true -g "{2}"', emuPath, gamePath)
        Logger.Debug("ShadPS4 Launcher: Command Line: " . runCmd)

        try {
            Run(runCmd, emuDir, , &pid)
            this.Pid := pid
            Logger.Info("ShadPS4 Launcher: Process started. PID: " . pid, "ShadPs4Launcher")

            if (pid > 0) {
                this.TrackProcess(pid, emuPath, this.GameId)

                if (WinWait("ahk_pid " pid, , 20)) {
                    WinActivate("ahk_pid " pid)
                    WindowManager.SetGameContext("ahk_pid " pid, 1)
                    Logger.Info("ShadPS4 Launcher: Window activated, context set.", "ShadPs4Launcher")
                } else {
                    Logger.Warn("ShadPS4 Launcher: Process started but window did not appear within 20s.")
                }
                return true
            }
        } catch as err {
            Logger.Error("ShadPS4 Launch Failed: " . err.Message)
        }
        return false
    }
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; * @description Scans TP UserProfiles & GameProfiles (XML + JSON Metadata)
; * @class TeknoParrotManager
; * @location lib/config/TeknoParrotManager.ahk
; * @author Philip
; * @date 2026/01/25
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\core\Utilities.ahk
#Include ..\ui\DialogsGui.ahk
#Include ..\core\Logger.ahk
#Include ConfigManager.ahk

class TeknoParrotManager {
    static PickerGui := ""
    static ListView := ""
    static IconCtrl := ""
    static ProfileMap := Map()
    static TpRootDir := ""

    ; Header Buttons
    static BtnAdd := ""
    static BtnView := ""
    static BtnClose := ""

    static UserCount := 0
    static SystemCount := 0

    static EmulatorMap := Map(
        "Play", "Play.exe", "ElfLdr2", "elfldr2.exe", "Sdaemon", "sdaemon.exe",
        "TeknoParrot", "TeknoParrot.exe", "ParrotLoader", "parrotloader.exe",
        "OpenParrot", "OpenParrotLoader.exe", "OpenParrot64", "OpenParrotLoader64.exe",
        "Lindbergh", "BudgieLoader.exe", "SegaTools", "BudgieLoader.exe",
        "RingEdge", "BudgieLoader.exe", "TypeX", "game.exe", "Nesica", "game.exe",
        "RPCS3", "rpcs3.exe", "CrediarDolphin", "DolphinNoGUI.exe", "Dolphin", "Dolphin.exe"
    )

    static GetPath() {
        return IniRead(ConfigManager.IniPath, "TEKNO_PATH", "TeknoPath", "")
    }

    static ShowPicker() {
        tpPath := this.GetPath()

        if (tpPath == "" || !FileExist(tpPath)) {
            tpPath := FileSelect(3 + 4096, , "Select TeknoParrotUi.exe", "TeknoParrotUi.exe")
            if (tpPath == "")
                return
            IniWrite(tpPath, ConfigManager.IniPath, "TEKNO_PATH", "TeknoPath")
        }

        Logger.Info("TP Manager: Opening Picker. TP Path: " tpPath)
        SplitPath(tpPath, , &tpDir)
        this.TpRootDir := tpDir

        userDir := tpDir "\UserProfiles"
        systemDir := tpDir "\GameProfiles"

        if !DirExist(userDir) {
            Logger.Error("TP Manager: UserProfiles folder missing at " userDir)
            DialogsGui.CustomTrayTip("UserProfiles folder not found", 3)
            return
        }

        this.ProfileMap.Clear()
        this.UserCount := 0
        this.SystemCount := 0

        Logger.Info("TP Manager: Scanning User Profiles...")
        this.ScanProfiles(userDir, "User")

        if DirExist(systemDir) {
            Logger.Info("TP Manager: Scanning System Profiles...")
            this.ScanProfiles(systemDir, "System")
        }

        Logger.Info("TP Manager: Scan Complete. Found " this.UserCount " User, " this.SystemCount " System.")

        if (this.ProfileMap.Count == 0) {
            DialogsGui.CustomTrayTip("No XML profiles found", 2)
            return
        }
        this.CreateGui()
    }

    static ScanProfiles(dir, type) {
        Loop Files, dir "\*.xml" {
            try {
                xmlContent := FileRead(A_LoopFileFullPath)
                title := A_LoopFileName

                if RegExMatch(xmlContent, "<GameNameInternal>(.*?)</GameNameInternal>", &match)
                    title := match[1]
                else if RegExMatch(xmlContent, "<GameName>(.*?)</GameName>", &match)
                    title := match[1]

                gameExe := ""
                if RegExMatch(xmlContent, "<ExecutableName>(.*?)</ExecutableName>", &match)
                    gameExe := match[1]

                emuType := ""
                if RegExMatch(xmlContent, "<EmulatorType>(.*?)</EmulatorType>", &match)
                    emuType := match[1]

                iconRelPath := ""

                if (type == "System") {
                    jsonName := StrReplace(A_LoopFileName, ".xml", ".json")
                    jsonPath := this.TpRootDir . "\Metadata\" . jsonName
                    if FileExist(jsonPath) {
                        try {
                            jsonContent := FileRead(jsonPath)
                            if RegExMatch(jsonContent, 'i)"icon_name"\s*:\s*"(.*?)"', &match) {
                                iconRelPath := "Icons\" . match[1]
                            }
                        }
                    }
                }

                if (iconRelPath == "") {
                    if RegExMatch(xmlContent, "i)<IconName>\s*(.*?)\s*</IconName>", &match)
                        iconRelPath := Trim(match[1])
                }

                if (SubStr(gameExe, -4) = ".zip" || SubStr(gameExe, -4) = ".rar") {
                    if (this.EmulatorMap.Has(emuType))
                        gameExe := this.EmulatorMap[emuType]
                    else if (emuType = "Play")
                        gameExe := "Play.exe"
                }

                this.ProfileMap[A_LoopFileFullPath] := {
                    Title: title,
                    Exe: gameExe,
                    File: A_LoopFileName,
                    FullPath: A_LoopFileFullPath,
                    Type: type,
                    IconPath: iconRelPath
                }

                if (type == "User")
                    this.UserCount++
                else
                    this.SystemCount++
            }
        }
    }

    static CreateGui() {
        if (this.PickerGui)
            this.PickerGui.Destroy()

        this.PickerGui := Gui("-Caption +Border +AlwaysOnTop +ToolWindow", "TeknoParrot Profile Explorer")
        this.PickerGui.BackColor := "2A2A2A"
        this.PickerGui.SetFont("s10 cWhite", "Segoe UI")

        ; ---- [FIX] Snap Logic ----
        this.InitSnapping()

        headerText := "   NEXUS :: TeknoParrot Profiles  (User: " this.UserCount " / System: " this.SystemCount ")"

        ; HEADER BAR (850w)
        this.PickerGui.Add("Text", "x0 y0 w715 h30 +0x200 Background2A2A2A", headerText).OnEvent("Click", (*) => PostMessage(0xA1, 2, 0, this.PickerGui.Hwnd))

        ; Header Icons [Add] [View] [Close]
        this.BtnAdd := this.AddNavBtn(" ➕ ", this.OnProfileSelected.Bind(this), "x+0 yp ") ; Starts Gray
        this.BtnView := this.AddNavBtn(" 📄 ", this.OnViewXml.Bind(this), "x+0 yp ")
        this.BtnClose := this.AddNavBtn(" ❌ ", (*) => this.PickerGui.Destroy(), "x+0 yp  cRed")

        ; ListView
        this.ListView := this.PickerGui.Add("ListView", "x10 y+5 w600 h450 -Hdr Background202020 cWhite", ["Game Title", "XML File", "Type"])
        this.ListView.OnEvent("DoubleClick", this.OnProfileSelected.Bind(this))
        this.ListView.OnEvent("ItemSelect", this.OnSelectionChanged.Bind(this))

        ; Icon Preview
        this.IconCtrl := this.PickerGui.Add("Picture", "x615 y50 w180 h180 +BackgroundTrans -Border vIconPreview +0x40", "")

        for path, data in this.ProfileMap {
            this.ListView.Add(, data.Title, data.File, data.Type)
        }

        this.ListView.ModifyCol(1, 300)
        this.ListView.ModifyCol(2, 220)
        this.ListView.ModifyCol(3, 75)

        this.PickerGui.Show("w805")
    }

    ; --- [NEW] SNAPPING LOGIC ---
    static InitSnapping() {
        ; Register the Windows Message Hook for Moving (0x216 = WM_MOVING)
        OnMessage(0x0216, this.OnWindowMove.Bind(this))
    }

static OnWindowMove(wParam, lParam, msg, hwnd) {
        ; --- CRASH FIX START ---
        ; safely get the Picker HWND. If it fails (window destroyed), set it to 0.
        pickerHwnd := 0
        try {
            if (this.PickerGui)
                pickerHwnd := this.PickerGui.Hwnd
        }

        ; If the Picker window doesn't exist, OR if the window moving isn't the Picker...
        ; (This stops the Main GUI movement from crashing the script)
        if (!pickerHwnd || hwnd != pickerHwnd)
            return
        ; --- CRASH FIX END ---

        ; Ensure Main Window exists to snap TO
        if (!IsSet(GuiBuilder) || !GuiBuilder.MainGui)
            return

        ; --- SNAPPING LOGIC ---
        try {
            WinGetPos(&mX, &mY, &mW, &mH, "ahk_id " GuiBuilder.MainGui.Hwnd)
        } catch {
            return
        }

        curX := NumGet(lParam, 0, "Int")
        curY := NumGet(lParam, 4, "Int")
        curR := NumGet(lParam, 8, "Int")
        curB := NumGet(lParam, 12, "Int")

        width := curR - curX
        height := curB - curY
        snapDist := 20

        ; Snap X
        if (Abs(curX - (mX + mW)) < snapDist)
            curX := mX + mW
        else if (Abs((curX + width) - mX) < snapDist)
            curX := mX - width
        else if (Abs(curX - mX) < snapDist)
            curX := mX

        ; Snap Y
        if (Abs(curY - (mY + mH)) < snapDist)
            curY := mY + mH
        else if (Abs((curY + height) - mY) < snapDist)
            curY := mY - height
        else if (Abs(curY - mY) < snapDist)
            curY := mY

        NumPut("Int", curX, lParam, 0)
        NumPut("Int", curY, lParam, 4)
        NumPut("Int", curX + width, lParam, 8)
        NumPut("Int", curY + height, lParam, 12)
    }

    static AddNavBtn(label, callback, options) {
        btn := this.PickerGui.Add("Text", options " w30 h30 +0x200 +Center", label)
        btn.OnEvent("Click", callback)
        return btn
    }

    static OnSelectionChanged(*) {
        row := this.ListView.GetNext(0, "F")
        if (row == 0) {
            this.BtnAdd.Opt("Background333333")
            this.IconCtrl.Value := ""
            return
        }

        targetFile := this.ListView.GetText(row, 2)
        type := this.ListView.GetText(row, 3)

        ; Update Add Button State
        if (type == "User") {
            this.BtnAdd.Opt("Background006600") ; Green for Go
        } else {
            this.BtnAdd.Opt("Background333333") ; Gray for No-Go
        }

        selectedData := ""
        for path, data in this.ProfileMap {
            if (data.File == targetFile && data.Type == type) {
                selectedData := data
                break
            }
        }

        if (selectedData && selectedData.IconPath != "") {
            cleanRel := StrReplace(selectedData.IconPath, "/", "\")
            fullIconPath := this.TpRootDir . "\" . cleanRel

            if !FileExist(fullIconPath) {
                SplitPath(cleanRel, &fName)
                fullIconPath := this.TpRootDir . "\Icons\" . fName
            }

            try {
                if FileExist(fullIconPath) {
                    this.IconCtrl.Value := fullIconPath
                    Logger.Debug("TP Manager: Previewing Icon -> " fullIconPath)
                } else {
                    this.IconCtrl.Value := ""
                    Logger.Debug("TP Manager: Icon missing on disk -> " fullIconPath)
                }
            } catch {
                this.IconCtrl.Value := ""
            }
        } else {
            this.IconCtrl.Value := ""
        }
    }

    static OnViewXml(*) {
        row := this.ListView.GetNext(0, "F")
        if (row == 0)
            return

        targetFile := this.ListView.GetText(row, 2)
        targetType := this.ListView.GetText(row, 3)

        selectedData := ""
        for path, data in this.ProfileMap {
            if (data.File == targetFile && data.Type == targetType) {
                selectedData := data
                break
            }
        }

        if (selectedData) {
            try {
                content := FileRead(selectedData.FullPath)
                DialogsGui.ShowTextViewer(selectedData.Title, content, 600, 500)
            } catch as err {
                Logger.Error("TP Manager: Failed to read XML " err.Message)
                DialogsGui.CustomStatusPop("Error reading file")
            }
        }
    }

    static OnProfileSelected(*) {
        row := this.ListView.GetNext(0, "F")
        if (row == 0)
            return

        type := this.ListView.GetText(row, 3)

        ; Block action if it's a System profile
        if (type != "User") {
            DialogsGui.CustomStatusPop("Cannot run System Templates")
            return
        }

        targetFile := this.ListView.GetText(row, 2)

        for path, data in this.ProfileMap {
            if (data.File == targetFile && data.Type == "User") {
                this.RegisterTeknoGame(data)
                this.PickerGui.Destroy()
                return
            }
        }
    }

static RegisterTeknoGame(data) {
        Logger.Info("TP Manager: Starting registration for " . data.Title)

        ; --- STEP 1: CALCULATE ID FIRST (Critical Fix) ---
        ; We must create safeId BEFORE we try to use it in RegisterGame
        safeTitle := Utilities.SanitizeName(data.Title)

        ; Sanitize filename to create a safe ID (e.g., "GAME_TP_MarioKart_xml")
        safeFile := RegExReplace(data.File, "[^A-Za-z0-9]", "_")
        safeId := "GAME_TP_" . safeFile

        ; --- STEP 2: GET USER INPUT ---
        userInput := DialogsGui.AskForString("Add TeknoParrot Game", "Display Name:", safeTitle)
        friendlyName := (userInput != "") ? Utilities.SanitizeName(userInput) : safeTitle

        ; --- STEP 3: BUILD THE OBJECT ---
        tpPath := this.GetPath()
        newGame := {
            ApplicationPath: tpPath,
            GameApplication: "TeknoParrotUi.exe",
            ExeName: data.Exe,
            SavedName: friendlyName,
            LauncherType: "TEKNO",
            ProfileFile: data.File,
            Id: safeId
        }

        ; --- STEP 4: REGISTER TO DATABASE ---
        ; Now safeId and newGame exist, so this will work
        ConfigManager.RegisterGame(safeId, newGame)

        ; Update State
        ConfigManager.CurrentGameId := safeId
        ConfigManager.UpdateLastPlayed(safeId)

        Logger.Info("TP Manager: Successfully registered " . safeId)

        ; --- STEP 5: REFRESH UI ---
        if IsSet(GuiBuilder) {
            GuiBuilder.RefreshDropdown()
            DialogsGui.CustomTrayTip("Added: " . friendlyName, 1)
        }
    }
}
```

---

```
#Requires AutoHotkey v2.0
; ==============================================================================
; Description: Handles TeknoParrot launching.
; Class: TeknoParrotLauncher
; Location: lib/emulator/types/TeknoParrotLauncher.ahk
; * @author Philip
; * @date 2026/01/17
; * @version 1.0.00
; ==============================================================================

; --- DEPENDENCY IMPORTS ---
#Include ..\EmulatorBase.ahk
#Include ..\..\config\TeknoParrotManager.ahk
#Include ..\..\window\WindowManager.ahk
#Include ..\..\core\Logger.ahk

; --- INHERIT FROM EMULATORBASE ---
class TeknoParrotLauncher extends EmulatorBase {
    TpPid := 0

    static EmulatorMap := Map(
        "Play", "Play.exe", "ElfLdr2", "elfldr2.exe", "Sdaemon", "sdaemon.exe",
        "TeknoParrot", "TeknoParrot.exe", "ParrotLoader", "parrotloader.exe",
        "OpenParrot", "OpenParrotLoader.exe", "Lindbergh", "BudgieLoader.exe",
        "SegaTools", "BudgieLoader.exe", "RingEdge", "BudgieLoader.exe",
        "TypeX", "game.exe", "Nesica", "game.exe", "Dolphin", "Dolphin.exe"
    )

    Launch(gameObj) {
        this.GameId := gameObj.Id
        Logger.Info("TP Launcher: Starting sequence for " gameObj.Id, "TeknoParrotLauncher")

        Logger.Info("TP Launcher: Cleaning up old processes...", "TeknoParrotLauncher")
        WindowManager.ForceKillAll()
        Sleep(100)

        tpPath := TeknoParrotManager.GetPath()
        profileName := gameObj.HasProp("ProfileFile") ? gameObj.ProfileFile : ""

        if (!tpPath || !profileName) {
            Logger.Error("TP Launcher: Missing Path or Profile.")
            DialogsGui.CustomMsgBox("Error", "TeknoParrot path missing.")
            return false
        }

        SplitPath(tpPath, , &tpDir)
        profilePath := (InStr(profileName, "\")) ? profileName : tpDir "\UserProfiles\" profileName

        if !FileExist(profilePath) {
            Logger.Error("TP Launcher: XML Profile not found at " profilePath)
            DialogsGui.CustomMsgBox("Error", "XML not found:`n" profilePath)
            return false
        }

        gameInfo := this.ParseProfileXml(profilePath)

        ; Stop launch if the game executable doesn't exist
        if (gameInfo.Path != "" && !FileExist(gameInfo.Path)) {
            Logger.Error("TP Launcher: Game file missing at " gameInfo.Path)
            DialogsGui.CustomMsgBox("Launch Error", "The game file was not found:`n`n" gameInfo.Path)
            return false
        }

        expectedExe := gameInfo.Exe

        ; Fallback for Tekken if Regex failed
        if (expectedExe == "" && InStr(profileName, "tekken")) {
            Logger.Warn("TP Launcher: Applying Tekken fallback")
            expectedExe := "Play.exe"
        }

        Logger.Info("TP Launcher: Target Exe Resolved -> [" expectedExe "] (EmuType: " gameInfo.EmuType ")", "TeknoParrotLauncher")

        if (expectedExe != "") {
            WindowManager.SetGameContext("ahk_exe " . expectedExe, 1)
        }

        runCmd := Format('"{1}" --profile="{2}"', tpPath, profilePath)
        Logger.Debug("TP Launcher: Executing -> " runCmd)

        try {
            Run(runCmd, tpDir, "Min", &tpPid)
            this.TpPid := tpPid
            Logger.Info("TP Launcher: TeknoParrotUI started with PID " tpPid, "TeknoParrotLauncher")

            ; Start the popup killer
            SetTimer(this.NagScreenKiller.Bind(this), 500)
            SetTimer(() => SetTimer(this.NagScreenKiller.Bind(this), 0), -20000)

            Logger.Info("TP Launcher: Waiting for game process...", "TeknoParrotLauncher")

            ; This waits up to 45 seconds for the REAL game (e.g. BudgieLoader.exe) to appear
            realPid := this.WaitForGameProcess(expectedExe, tpPid, 45000)

            if (realPid) {
                this.Pid := realPid
                Logger.Info("TP Launcher: Game Process FOUND! PID: " realPid, "TeknoParrotLauncher")

                ; --- THE FIX: TRACK THE REAL PROCESS ---
                ; We pass the PID of the game (BudgieLoader), not the UI.
                ; This ensures ProcessManager watches the correct RAM usage.
                this.TrackProcess(realPid, gameInfo.Path, this.GameId)

                Sleep(2000) ; Give the game a moment to render

                ; Force Context
                WindowManager.SetGameContext("ahk_pid " realPid, 1)

                GuiBuilder.SetRecordingStatus(true)
                return true
            } else {
                Logger.Error("TP Launcher: Timeout waiting for [" expectedExe "]")
                DialogsGui.CustomTrayTip("Game process timed out.", 3)
                if ProcessExist(tpPid)
                    ProcessClose(tpPid)
                return false
            }
        } catch as err {
            Logger.Error("TP Launcher: Crash -> " err.Message)
            DialogsGui.CustomMsgBox("Launch Failure", err.Message)
            return false
        }
    }

    ; ... (Keep ParseProfileXml, WaitForGameProcess, and NagScreenKiller exactly as they were) ...
    ParseProfileXml(xmlPath) {
        try {
            content := FileRead(xmlPath)
            info := { Exe: "", Path: "", EmuType: "" }
            if RegExMatch(content, "i)<GamePath>\s*(.*?)\s*</GamePath>", &match)
                info.Path := Trim(match[1])
            if RegExMatch(content, "i)<ExecutableName>\s*(.*?)\s*</ExecutableName>", &match)
                info.Exe := Trim(match[1])
            if RegExMatch(content, "i)<EmulatorType>\s*(.*?)\s*</EmulatorType>", &match)
                info.EmuType := Trim(match[1])

            needsMapping := (SubStr(info.Exe, -4) = ".zip" || SubStr(info.Exe, -4) = ".rar" || info.Exe == "")
            if (needsMapping && TeknoParrotLauncher.EmulatorMap.Has(info.EmuType))
                info.Exe := TeknoParrotLauncher.EmulatorMap[info.EmuType]
            else if (info.EmuType = "Play")
                info.Exe := "Play.exe"

            return info
        } catch {
            return { Exe: "", Path: "", EmuType: "" }
        }
    }

    WaitForGameProcess(expectedExe, tpLauncherPid, timeoutMs) {
        startTime := A_TickCount
        while ((A_TickCount - startTime) < timeoutMs) {
            if (expectedExe != "" && ProcessExist(expectedExe))
                return ProcessExist(expectedExe)
            foundHwnd := WindowManager.CheckForTeknoWindow()
            if (foundHwnd) {
                return WinGetPID("ahk_id " foundHwnd)
            }
            Sleep(500)
        }
        return 0
    }

    NagScreenKiller() {
        if (!this.TpPid || !ProcessExist(this.TpPid))
            return
        try {
            for this_id in WinGetList("ahk_pid " this.TpPid) {
                title := WinGetTitle(this_id)
                txt := WinGetText(this_id)
                if (InStr(txt, "already be running") || title = "Question" || title = "Error") {
                    WinActivate(this_id), Sleep(50), Send("{Enter}")
                    continue
                }
                if (WinGetStyle(this_id) & 0x10000000) {
                    WinGetPos(, , &w, &h, this_id)
                    (w < 600 && h < 450) ? (WinActivate(this_id), Send("{Enter}")) : WinHide(this_id)
                }
            }
        }
    }

Stop() {
        ; 1. Nuke the processes first
        Logger.Info("TP Launcher: Stopping PID " this.Pid, "TeknoParrotLauncher")
        WindowManager.ForceKillAll()

        ; 2. Let the MonitorProcess timer handle the EndSession automatically,
        ; but we clear our local references.
        this.Pid := 0
        this.TpPid := 0
    }
}
```


