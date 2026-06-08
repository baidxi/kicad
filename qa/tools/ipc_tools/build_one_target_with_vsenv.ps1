param(
    [Parameter(Mandatory = $true)]
    [string]$Target,

    [Parameter(Mandatory = $false)]
    [string]$Preset = "win64-release"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# Usage: call from repo root, or let this script set it.
$repoRoot = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
Push-Location (Resolve-Path $repoRoot)
try {
    # Wrap build in VS dev environment when VsDevCmd.bat exists.
    $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    foreach( $edition in @("Professional", "Community", "Enterprise") ) {
        if( -not (Test-Path $vsDevCmd) ) {
            $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\$edition\Common7\Tools\VsDevCmd.bat"
        }
    }

    if( -not (Test-Path $vsDevCmd) ) {
        throw "VsDevCmd.bat not found. Install Visual Studio 2022 Build Tools or full VS."
    }

    $cmd = "`"$vsDevCmd`" -arch=amd64 && cmake --build --preset $Preset --target $Target"
    cmd /c $cmd
} finally {
    Pop-Location
}

