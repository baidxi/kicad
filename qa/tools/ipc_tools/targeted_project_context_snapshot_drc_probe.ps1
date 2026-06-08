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

function Sync-IpcDrcV1Fixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )
    $src = Join-Path $RepoRoot "qa\data\pcbnew\drc\ipc_drc_v1"
    $dst = Join-Path $BuildDir "ipc-drc-v1-proj"
    if (!(Test-Path -LiteralPath $src)) { throw "Missing ipc_drc_v1 fixture at $src" }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcDrcV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project" }
if (!(Test-Path $pcb)) { throw "Missing board" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-project-context-snapshot-drc-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$script:ProbeIpcRetryExtraTotal = 0

function Parse-SnapshotOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                          = ($Text -match 'status=1')
        project_info_present               = (& $parseLine "project_info_present")
        snapshot_summary_present           = (& $parseLine "snapshot_summary_present")
        drc_summary_present                = (& $parseLine "drc_summary_present")
        drc_summary_available              = (& $parseLine "drc_summary_available")
        drc_summary_requested              = (& $parseLine "drc_summary_requested")
        drc_summary_run_mode               = (& $parseLine "drc_summary_run_mode")
        drc_summary_is_official_drc        = (& $parseLine "drc_summary_is_official_drc")
        drc_summary_drc_was_run            = (& $parseLine "drc_summary_drc_was_run")
        drc_summary_drc_source             = (& $parseLine "drc_summary_drc_source")
        drc_summary_violations_count       = (& $parseLine "drc_summary_violations_count")
        drc_summary_has_clearance_violations = (& $parseLine "drc_summary_has_clearance_violations")
        drc_summary_exact_blocker_if_any   = (& $parseLine "drc_summary_exact_blocker_if_any")
        drc_summary_types                  = (& $parseLine "drc_summary_types")
        raw                                = $Text
    }
}

function Invoke-ProjectContextSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_project_context_snapshot", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return @{
        Parsed = Parse-SnapshotOutput -Text $out
        Status = if ($out -match 'status=1') { "ok" } else { "error" }
        Raw    = $out
    }
}

$defaultNoDrcOk = "false"
$existingMarkersOk = "false"
$engineRunOk = "false"
$engineRunClearanceOk = "false"
$invalidRunModeOk = "false"
$invalidMaxViolationsOk = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }
    Start-Sleep -Seconds 3

    $tDefault = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{} -LogBaseName "snapshot_default"
    if ($tDefault.Status -eq "ok" `
            -and $tDefault.Parsed.project_info_present -eq "1" `
            -and $tDefault.Parsed.snapshot_summary_present -eq "1" `
            -and ($tDefault.Parsed.drc_summary_present -eq "0" -or $tDefault.Parsed.drc_summary_present -eq "absent")) {
        $defaultNoDrcOk = "true"
    }

    $tExisting = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "existing_markers"
        drc_board_filename    = $pcbAbs
    } -LogBaseName "snapshot_existing_markers"
    if ($tExisting.Status -eq "ok" `
            -and $tExisting.Parsed.drc_summary_present -eq "1" `
            -and $tExisting.Parsed.drc_summary_requested -eq "true" `
            -and $tExisting.Parsed.drc_summary_run_mode -eq "existing_markers" `
            -and $tExisting.Parsed.drc_summary_drc_was_run -eq "false" `
            -and $tExisting.Parsed.drc_summary_exact_blocker_if_any -eq "N/A") {
        $existingMarkersOk = "true"
    }

    $tEngine = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "engine_run"
        drc_board_filename    = $pcbAbs
        drc_max_violations    = 10
    } -LogBaseName "snapshot_engine_run"
    $violCount = 0
    if ($tEngine.Parsed.drc_summary_violations_count -match '^[0-9]+$') {
        $violCount = [int]$tEngine.Parsed.drc_summary_violations_count
    }
    $hasClearanceType = ($tEngine.Parsed.drc_summary_types -match 'clearance')
    if ($tEngine.Status -eq "ok" `
            -and $tEngine.Parsed.drc_summary_present -eq "1" `
            -and $tEngine.Parsed.drc_summary_requested -eq "true" `
            -and $tEngine.Parsed.drc_summary_run_mode -eq "engine_run" `
            -and $tEngine.Parsed.drc_summary_is_official_drc -eq "true" `
            -and $tEngine.Parsed.drc_summary_drc_was_run -eq "true" `
            -and $tEngine.Parsed.drc_summary_drc_source -eq "engine_run" `
            -and $violCount -ge 1 `
            -and $tEngine.Parsed.drc_summary_exact_blocker_if_any -eq "N/A") {
        $engineRunOk = "true"
    }
    if ($engineRunOk -eq "true" -and $hasClearanceType `
            -and $tEngine.Parsed.drc_summary_has_clearance_violations -eq "true") {
        $engineRunClearanceOk = "true"
    }

    $tBadMode = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "bad_value"
    } -LogBaseName "snapshot_invalid_run_mode"
    if ($tBadMode.Status -eq "error" -or $tBadMode.Raw -match 'AS_BAD_REQUEST|bad request|invalid_drc_run_mode|drc_run_mode must be') {
        $invalidRunModeOk = "true"
    }

    $tBadMax = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "engine_run"
        drc_board_filename    = $pcbAbs
        drc_max_violations    = 0
    } -LogBaseName "snapshot_invalid_max_violations"
    if ($tBadMax.Status -eq "error" -or $tBadMax.Raw -match 'AS_BAD_REQUEST|bad request|invalid_drc_max_violations') {
        $invalidMaxViolationsOk = "true"
    }

    $green = "true"
    if ($defaultNoDrcOk -ne "true") { $green = "false"; $finalBlocker = "default_no_drc_failed" }
    elseif ($existingMarkersOk -ne "true") { $green = "false"; $finalBlocker = "existing_markers_mode_failed" }
    elseif ($engineRunOk -ne "true") { $green = "false"; $finalBlocker = "engine_run_mode_failed" }
    elseif ($engineRunClearanceOk -ne "true") { $green = "false"; $finalBlocker = "engine_run_clearance_summary_failed" }
    elseif ($invalidRunModeOk -ne "true") { $green = "false"; $finalBlocker = "invalid_drc_run_mode_failed" }
    elseif ($invalidMaxViolationsOk -ne "true") { $green = "false"; $finalBlocker = "invalid_drc_max_violations_failed" }

    @(
        "project_context_snapshot_drc_green=$green"
        "default_no_drc_ok=$defaultNoDrcOk"
        "existing_markers_mode_ok=$existingMarkersOk"
        "engine_run_mode_ok=$engineRunOk"
        "engine_run_clearance_summary_ok=$engineRunClearanceOk"
        "invalid_drc_run_mode_ok=$invalidRunModeOk"
        "invalid_drc_max_violations_ok=$invalidMaxViolationsOk"
        "exact_blocker_if_any=$finalBlocker"
        "ipc_smoke_ipc_retry_extra_total=$script:ProbeIpcRetryExtraTotal"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_project_context_snapshot_drc_summary.txt") -Encoding UTF8

    if ($green -ne "true") {
        Write-Host "TARGET_RUN_DIR=$runDir"
        exit 1
    }
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

Write-Host "TARGET_RUN_DIR=$runDir"
exit 0
