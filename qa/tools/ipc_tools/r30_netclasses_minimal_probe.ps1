param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [Parameter(Mandatory = $true)]
    [ValidateSet('ipc-v1-proj', 'ipc-geometry-v1-proj', 'ipc-clearance-v1-proj')]
    [string]$FixtureName
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

function Sync-IpcClearanceV1Fixture {
    param([string]$RepoRoot, [string]$BuildDir)
    $src = Join-Path $RepoRoot "qa\data\pcbnew\clearance\ipc_clearance_v1"
    $dst = Join-Path $BuildDir "ipc-clearance-v1-proj"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

function Sync-IpcGeometryV1Fixture {
    param([string]$RepoRoot, [string]$BuildDir)
    $src = Join-Path $RepoRoot "qa\data\pcbnew\geometry\ipc_geometry_v1"
    $dst = Join-Path $BuildDir "ipc-geometry-v1-proj"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

switch ($FixtureName) {
    'ipc-clearance-v1-proj' { Sync-IpcClearanceV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir }
    'ipc-geometry-v1-proj' { Sync-IpcGeometryV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir }
    default { }
}

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = switch ($FixtureName) {
    'ipc-v1-proj'            { Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro" }
    'ipc-geometry-v1-proj'   { Join-Path $BuildDir "ipc-geometry-v1-proj\ipc_geometry_v1.kicad_pro" }
    'ipc-clearance-v1-proj' { Join-Path $BuildDir "ipc-clearance-v1-proj\ipc_clearance_v1.kicad_pro" }
}

$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r30-netclasses-probe-" + $FixtureName)
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$swTotal = [System.Diagnostics.Stopwatch]::StartNew()
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "IPC URL unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $pingPath = Join-Path $runDir "get_project_info.args.json"
    '{}' | Set-Content $pingPath -Encoding UTF8
    $swGpi = [System.Diagnostics.Stopwatch]::StartNew()
    $gpiSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_project_info", ("@" + $pingPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 2 -RetryDelayMs 400
    $swGpi.Stop()
    $gpiSm.Text | Set-Content (Join-Path $runDir "get_project_info.log") -Encoding UTF8

    $loaded = $false
    if ($gpiSm.Text -match 'loaded=true') { $loaded = $true }

    $ncArgs = Join-Path $runDir "get_net_classes.args.json"
    '{}' | Set-Content $ncArgs -Encoding UTF8
    $swNc = [System.Diagnostics.Stopwatch]::StartNew()
    $ncSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_net_classes", ("@" + $ncArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 2 -RetryDelayMs 400
    $swNc.Stop()
    $ncSm.Text | Set-Content (Join-Path $runDir "get_net_classes.log") -Encoding UTF8

    $timeoutNc = ($ncSm.Text -eq "timeout")
    $statusNc = ($ncSm.Text -match 'status=1')

    @(
        "fixture=$FixtureName"
        "elapsed_total_ms=$([int]$swTotal.ElapsedMilliseconds)"
        "get_project_info_elapsed_ms=$([int]$swGpi.ElapsedMilliseconds)"
        "get_project_info_loaded=$loaded"
        "get_net_classes_elapsed_ms=$([int]$swNc.ElapsedMilliseconds)"
        "get_net_classes_timeout=$timeoutNc"
        "get_net_classes_status_ok=$statusNc"
        "QA_NETCLASS_A_found=$( $ncSm.Text -match 'QA_NETCLASS_A' )"
        "QA_NETCLASS_B_found=$( $ncSm.Text -match 'QA_NETCLASS_B' )"
    ) | Set-Content (Join-Path $runDir "probe_summary.txt") -Encoding UTF8

    Write-Output ("PROBE_RUN_DIR=" + $runDir)
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
