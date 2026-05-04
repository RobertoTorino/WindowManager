param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("Debug", "Release")]
    [string]$Configuration
)

$settingsPath = Join-Path $PSScriptRoot "..\.vscode\settings.json"

if (-not (Test-Path -Path $settingsPath)) {
    Write-Error "settings.json not found at $settingsPath"
    exit 1
}

$settings = Get-Content -Path $settingsPath -Raw | ConvertFrom-Json

if ($Configuration -eq "Debug") {
    $settings."cmake.configurePreset" = "windows-clang-debug"
    $settings."cmake.buildPreset" = "build-windows-clang-debug"
}
else {
    $settings."cmake.configurePreset" = "windows-clang-release"
    $settings."cmake.buildPreset" = "build-windows-clang-release"
}

$settings | ConvertTo-Json -Depth 16 | Set-Content -Path $settingsPath -Encoding UTF8

Write-Output "Updated defaults to clang $Configuration presets in .vscode/settings.json"