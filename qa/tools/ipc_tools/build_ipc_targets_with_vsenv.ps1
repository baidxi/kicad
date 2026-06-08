# Build IPC-related targets with MSVC environment (VsDevCmd).
# Required because KiCad uses /external:env:INCLUDE - cl.exe needs INCLUDE from VsDevCmd.
# Usage: .\build_ipc_targets_with_vsenv.ps1 [-RepoRoot <path>] [-Preset win64-release]
param(
    [string]$RepoRoot = (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)),
    [string]$Preset = "win64-release"
)

$vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
foreach ($edition in @("Professional", "Community", "Enterprise")) {
    if (-not (Test-Path $vsDevCmd)) {
        $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\$edition\Common7\Tools\VsDevCmd.bat"
    }
}
if (-not (Test-Path $vsDevCmd)) {
    throw "VsDevCmd.bat not found. Install Visual Studio 2022 Build Tools or full VS."
}
Push-Location (Resolve-Path $RepoRoot)
try {
    $targets = "api_schema_build_copy", "schema_build_copy", "kicad", "eeschema", "pcbnew", "qa_ipc_smoke"
    $tlist = $targets -join " "
    cmd /c "`"$vsDevCmd`" -arch=amd64 && cmake --build --preset $Preset --target $tlist"
} finally {
    Pop-Location
}
