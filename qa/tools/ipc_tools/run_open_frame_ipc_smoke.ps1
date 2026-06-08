param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $true)]
    [ValidateSet('pcb', 'sch')]
    [string]$Frame,

    [Parameter(Mandatory = $true)]
    [string]$FilePathsCsv,

    [Parameter(Mandatory = $false)]
    [bool]$SoftwareRendering = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-LastApiUrl([string]$apiLog) {
    if (!(Test-Path $apiLog)) { return $null }
    $lines = Get-Content $apiLog -Tail 400
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match 'started at (ipc://.+) ---') { return $Matches[1] }
    }
    return $null
}

$BuildDir = (Resolve-Path $BuildDir).Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$apiLog = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\api.log"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
${FilePaths} = $FilePathsCsv -split ';' | ForEach-Object { $_.Trim() } | Where-Object { $_ -and $_.Length -gt 0 }

# OpenProjectFiles() implementations insist on absolute paths.
$FilePaths = $FilePaths | ForEach-Object { [System.IO.Path]::GetFullPath($_) }

foreach( $fp in $FilePaths ) {
    if( !(Test-Path $fp) ) { throw "Missing file at $fp" }
}

$tempKiCad = Join-Path $env:TEMP "kicad"

if (Test-Path $apiLog) { $beforeLen = (Get-Item $apiLog).Length } else { $beforeLen = 0 }

Stop-Process -Name kicad -Force -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $tempKiCad -ErrorAction SilentlyContinue

# Runtime env (mirrors qa/tools/ipc_tools/run_ipc_v1_windows.ps1)
$env:KICAD_CONFIG_HOME = $cfgRoot
$env:KICAD_RUN_FROM_BUILD_DIR = "1"
$env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
$env:PYTHONHOME = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"

$pathPrefix = @(
    (Join-Path $BuildDir "kicad"),
    (Join-Path $BuildDir "eeschema"),
    (Join-Path $BuildDir "pcbnew"),
    (Join-Path $BuildDir "common"),
    (Join-Path $BuildDir "common\gal"),
    (Join-Path $BuildDir "api"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3")
) -join ";"

$env:PATH = $pathPrefix + ";" + $env:PATH

Write-Host ("Starting KiCad: frame={0} files={1} software-rendering={2}" -f $Frame, ($FilePaths -join ','), $SoftwareRendering)
$argsList = @()
if( $SoftwareRendering ) { $argsList += "--software-rendering" }
$argsList += @("--frame", $Frame) + $FilePaths
$p = Start-Process -FilePath $kicadExe -WorkingDirectory $repoRoot -ArgumentList $argsList -PassThru

$url = $null
$exitCode = $null
for ($i = 0; $i -lt 60; $i++) {
    Start-Sleep -Milliseconds 500
    if (!(Get-Process -Id $p.Id -ErrorAction SilentlyContinue)) {
        $exitCode = $p.ExitCode
        break
    }

    if (Test-Path $apiLog) {
        $len = (Get-Item $apiLog).Length
        if ($len -gt $beforeLen) {
            $url = Get-LastApiUrl $apiLog
            if ($url) { break }
        }
    }
}

if (!$url) {
    Write-Host "KiCad did not reach API server. exitCode=$exitCode"
    $latestDump = Get-ChildItem -Path "$env:LOCALAPPDATA\\CrashDumps\\kicad.exe*.dmp" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if( $latestDump ) {
        Write-Host "Latest crash dump: $($latestDump.FullName)"
    }
    throw "No API url found in $apiLog"
}
Write-Host "URL=$url"

# Quick reachability check
& $smokeExe $url ping | Out-Null

# Exact output requested for workspace overview
& $smokeExe $url mcp_call_tool get_workspace_overview

