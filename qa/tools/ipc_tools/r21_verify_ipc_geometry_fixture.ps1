param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-geometry-v1-proj\ipc_geometry_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-geometry-v1-proj\ipc_geometry_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $pcb)) { throw "Missing board $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path
$padKid = "f0010001-0001-4001-8001-000000000001"

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r21-ipc-geometry-fixture")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "IPC URL unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame failed: $($openSm.Text)" }
    Start-Sleep -Seconds 3

    $itemsArgsPath = Join-Path $runDir "pcb_get_board_items.args.json"
    (@{ board_filename = $pcbAbs; max_items = 20; item_types = @("pads") } | ConvertTo-Json -Compress -Depth 6) | Set-Content $itemsArgsPath -Encoding UTF8
    $itemsSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $itemsArgsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $itemsSm.Text | Set-Content (Join-Path $runDir "pcb_get_board_items_session.log") -Encoding UTF8

    $measureArgs = @{
        board_filename = $pcbAbs
        a              = @{ item_id = $padKid; position = "pad_anchor" }
        b              = @{ item_id = $padKid; position = "pad_anchor" }
    } | ConvertTo-Json -Compress -Depth 8
    $measurePath = Join-Path $runDir "pcb_measure_pad_anchor_self.args.json"
    Set-Content $measurePath $measureArgs -Encoding UTF8
    $mSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $measurePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $mSm.Text | Set-Content (Join-Path $runDir "pcb_measure_pad_anchor.log") -Encoding UTF8

    $bboxArgs = @{
        board_filename = $pcbAbs
        a              = @{ item_id = $padKid }
        b              = @{ item_id = $padKid }
    } | ConvertTo-Json -Compress -Depth 8
    $bboxPath = Join-Path $runDir "pcb_measure_bbox_self.args.json"
    Set-Content $bboxPath $bboxArgs -Encoding UTF8
    $bSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $bboxPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $bSm.Text | Set-Content (Join-Path $runDir "pcb_measure_bbox_self.log") -Encoding UTF8

    $mixArgs = @{
        board_filename = $pcbAbs
        a              = @{ item_id = $padKid; position = "pad_anchor" }
        b              = @{ item_id = $padKid }
    } | ConvertTo-Json -Compress -Depth 8
    $mixPath = Join-Path $runDir "pcb_measure_anchor_vs_bbox.args.json"
    Set-Content $mixPath $mixArgs -Encoding UTF8
    $xSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $mixPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $xSm.Text | Set-Content (Join-Path $runDir "pcb_measure_anchor_vs_bbox.log") -Encoding UTF8

    @(
        "r21_ipc_geometry_fixture_ok=true"
        "pad_kiid=$padKid"
        "target_run_dir=$runDir"
    ) | Set-Content (Join-Path $runDir "r21_ipc_geometry_fixture_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 0
}
catch {
    $_.Exception.Message | Set-Content (Join-Path $runDir "r21_error.txt") -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
