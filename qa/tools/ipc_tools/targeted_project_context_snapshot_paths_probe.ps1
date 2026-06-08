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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-project-context-snapshot-paths-targeted")
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
        status_ok                                   = ($Text -match 'status=1')
        project_info_present                        = (& $parseLine "project_info_present")
        snapshot_summary_present                    = (& $parseLine "snapshot_summary_present")
        snapshot_open_pcb_board_absolute_path_present = (& $parseLine "snapshot_open_pcb_board_absolute_path_present")
        snapshot_open_pcb_board_absolute_path       = (& $parseLine "snapshot_open_pcb_board_absolute_path")
        snapshot_open_pcb_board_absolute_paths      = (& $parseLine "snapshot_open_pcb_board_absolute_paths")
        open_documents_pcb_count                    = (& $parseLine "open_documents_pcb_count")
        open_documents_pcb0_absolute_path           = (& $parseLine "open_documents_pcb0_absolute_path")
        open_documents_pcb0_board_absolute_path     = (& $parseLine "open_documents_pcb0_board_absolute_path")
        drc_summary_present                         = (& $parseLine "drc_summary_present")
        drc_summary_run_mode                        = (& $parseLine "drc_summary_run_mode")
        drc_summary_is_official_drc                 = (& $parseLine "drc_summary_is_official_drc")
        drc_summary_drc_was_run                     = (& $parseLine "drc_summary_drc_was_run")
        drc_summary_violations_count                = (& $parseLine "drc_summary_violations_count")
        drc_summary_has_clearance_violations        = (& $parseLine "drc_summary_has_clearance_violations")
        drc_summary_exact_blocker_if_any            = (& $parseLine "drc_summary_exact_blocker_if_any")
        drc_summary_types                           = (& $parseLine "drc_summary_types")
        raw                                         = $Text
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

function Normalize-PathForCompare {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path) -or $Path -eq "absent") { return "" }
    try { return (Resolve-Path -LiteralPath $Path).Path.ToLowerInvariant() } catch { return $Path.ToLowerInvariant() }
}

$defaultSnapshotOk = "false"
$openPathPresentOk = "false"
$openPathSuffixOk = "false"
$openDocumentsPcbAbsOk = "false"
$openDocumentsMatchesSummaryOk = "false"
$drcEngineStillOk = "false"
$defaultNoDrcStillOk = "false"
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

    $tDefault = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{} -LogBaseName "snapshot_default_paths"
    $expectedAbsNorm = Normalize-PathForCompare $pcbAbs
    $reportedAbsNorm = Normalize-PathForCompare $tDefault.Parsed.snapshot_open_pcb_board_absolute_path

    if ($tDefault.Status -eq "ok" `
            -and $tDefault.Parsed.project_info_present -eq "1" `
            -and $tDefault.Parsed.snapshot_summary_present -eq "1") {
        $defaultSnapshotOk = "true"
    }
    if ($tDefault.Parsed.snapshot_open_pcb_board_absolute_path_present -eq "true" `
            -and $reportedAbsNorm -eq $expectedAbsNorm) {
        $openPathPresentOk = "true"
    }
    if ($tDefault.Parsed.snapshot_open_pcb_board_absolute_path -like "*.kicad_pcb") {
        $openPathSuffixOk = "true"
    }
    $openDocAbsNorm = Normalize-PathForCompare $tDefault.Parsed.open_documents_pcb0_absolute_path
    $openDocBoardAbsNorm = Normalize-PathForCompare $tDefault.Parsed.open_documents_pcb0_board_absolute_path
    if ($tDefault.Parsed.open_documents_pcb_count -match '^[1-9]' `
            -and $openDocAbsNorm -eq $expectedAbsNorm `
            -and $tDefault.Parsed.open_documents_pcb0_absolute_path -like "*.kicad_pcb") {
        $openDocumentsPcbAbsOk = "true"
    }
    if ($openDocumentsPcbAbsOk -eq "true" `
            -and $openDocBoardAbsNorm -eq $expectedAbsNorm `
            -and $reportedAbsNorm -eq $expectedAbsNorm) {
        $openDocumentsMatchesSummaryOk = "true"
    }
    if ($tDefault.Parsed.drc_summary_present -eq "0" -or $tDefault.Parsed.drc_summary_present -eq "absent") {
        $defaultNoDrcStillOk = "true"
    }

    $tEngineNoOverride = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "engine_run"
        drc_max_violations    = 10
    } -LogBaseName "snapshot_engine_run_no_board_override"
    $violCount = 0
    if ($tEngineNoOverride.Parsed.drc_summary_violations_count -match '^[0-9]+$') {
        $violCount = [int]$tEngineNoOverride.Parsed.drc_summary_violations_count
    }
    if ($tEngineNoOverride.Status -eq "ok" `
            -and $tEngineNoOverride.Parsed.drc_summary_run_mode -eq "engine_run" `
            -and $tEngineNoOverride.Parsed.drc_summary_is_official_drc -eq "true" `
            -and $tEngineNoOverride.Parsed.drc_summary_drc_was_run -eq "true" `
            -and $violCount -ge 1 `
            -and $tEngineNoOverride.Parsed.drc_summary_has_clearance_violations -eq "true" `
            -and $tEngineNoOverride.Parsed.drc_summary_exact_blocker_if_any -eq "N/A") {
        $drcEngineStillOk = "true"
    }

    $green = "true"
    if ($defaultSnapshotOk -ne "true") { $green = "false"; $finalBlocker = "default_snapshot_failed" }
    elseif ($openPathPresentOk -ne "true") { $green = "false"; $finalBlocker = "open_pcb_board_absolute_path_present_failed" }
    elseif ($openPathSuffixOk -ne "true") { $green = "false"; $finalBlocker = "open_pcb_board_absolute_path_suffix_failed" }
    elseif ($openDocumentsPcbAbsOk -ne "true") { $green = "false"; $finalBlocker = "open_documents_pcb_absolute_path_failed" }
    elseif ($openDocumentsMatchesSummaryOk -ne "true") { $green = "false"; $finalBlocker = "open_documents_snapshot_summary_path_mismatch" }
    elseif ($defaultNoDrcStillOk -ne "true") { $green = "false"; $finalBlocker = "default_no_drc_regression" }
    elseif ($drcEngineStillOk -ne "true") { $green = "false"; $finalBlocker = "drc_summary_engine_regression" }

    @(
        "project_context_snapshot_paths_green=$green"
        "default_snapshot_ok=$defaultSnapshotOk"
        "open_pcb_board_absolute_path_present_ok=$openPathPresentOk"
        "open_pcb_board_absolute_path_suffix_ok=$openPathSuffixOk"
        "open_documents_pcb_absolute_path_ok=$openDocumentsPcbAbsOk"
        "open_documents_matches_snapshot_summary_ok=$openDocumentsMatchesSummaryOk"
        "drc_summary_engine_still_ok=$drcEngineStillOk"
        "default_no_drc_still_ok=$defaultNoDrcStillOk"
        "exact_blocker_if_any=$finalBlocker"
        "ipc_smoke_ipc_retry_extra_total=$script:ProbeIpcRetryExtraTotal"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        "reported_open_pcb_board_absolute_path=$($tDefault.Parsed.snapshot_open_pcb_board_absolute_path)"
        "reported_open_documents_pcb0_absolute_path=$($tDefault.Parsed.open_documents_pcb0_absolute_path)"
        "reported_open_documents_pcb0_board_absolute_path=$($tDefault.Parsed.open_documents_pcb0_board_absolute_path)"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_project_context_snapshot_paths_summary.txt") -Encoding UTF8

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
