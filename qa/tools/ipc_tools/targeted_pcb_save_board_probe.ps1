param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R176 — save_board_session controlled save probe.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$cfg10 = Join-Path $cfgRoot "10.0"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-save-board-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library for save board"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-SaveBoardFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$DestSuffix
    )
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $dst = Join-Path $BuildDir ("ipc-pcb-save-v1-" + $DestSuffix + "-proj")
    if (!(Test-Path -LiteralPath $baseSrc)) { throw "Missing base fixture at $baseSrc" }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $baseSrc "*") -Destination $dst -Recurse -Force
    $projectFpLibTable = Join-Path $dst "fp-lib-table"
    @(
        "(fp_lib_table",
        "  (version 7)",
        ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library"))'),
        ")"
    ) | Set-Content -Path $projectFpLibTable -Encoding ASCII
    return @{
        ProjDir = $dst
        Proj    = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pro"
        Sch     = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_sch"
        Pcb     = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb"
    }
}

function Parse-SaveBoardOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                      = ($Text -match 'status=1')
        save_board_ok                  = (& $parseLine "save_board_ok")
        save_board_saved               = (& $parseLine "save_board_saved")
        save_board_confirm_save        = (& $parseLine "save_board_confirm_save")
        save_board_file_mtime_before   = (& $parseLine "save_board_file_mtime_before")
        save_board_file_mtime_after    = (& $parseLine "save_board_file_mtime_after")
        save_board_file_mtime_changed  = (& $parseLine "save_board_file_mtime_changed")
        exact_blocker_if_any           = (& $parseLine "save_board_exact_blocker_if_any")
    }
}

function Parse-PcbSyncReadinessOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        pcb_sync_readiness_ok       = (& $parseLine "pcb_sync_readiness_ok")
        missing_footprints_count    = (& $parseLine "missing_footprints_count")
        needs_update_from_schematic = (& $parseLine "needs_update_from_schematic")
    }
}

function Open-EditorsForFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel
    )
    $schAbs = (Resolve-Path $Fixture.Sch).Path
    $pcbAbs = (Resolve-Path $Fixture.Pcb).Path
    foreach ($pair in @(
            @{ type = "sch"; filename = $schAbs; log = "open_sch" }
            @{ type = "pcb"; filename = $pcbAbs; log = "open_pcb" }
        )) {
        $openInput = Join-Path $runDir ("qa_open_editor_frame_" + $CaseLabel + "_" + $pair.log + ".json")
        (@{ type = $pair.type; filename = $pair.filename } | ConvertTo-Json -Compress) | Set-Content $openInput -Encoding UTF8
        $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $openSm.Text | Set-Content (Join-Path $runDir ("qa_open_editor_frame_" + $CaseLabel + "_" + $pair.log + ".log")) -Encoding UTF8
        if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame $($pair.type) failed for $CaseLabel" }
        Start-Sleep -Seconds 3
    }
}

function Invoke-SaveBoardCase {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel,
        [Parameter(Mandatory = $true)]
        [hashtable]$ToolArgs,
        [string]$BoardFilenameOverride = $null,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Assert
    )
    Stop-KiCadProcesses
    Start-Sleep -Seconds 5
    $null = Clear-IpcSockets

    $schAbs = (Resolve-Path $Fixture.Sch).Path
    $pcbAbs = (Resolve-Path $Fixture.Pcb).Path
    $projAbs = (Resolve-Path $Fixture.Proj).Path
    $pcbBefore = Get-Item -LiteralPath $pcbAbs

    $kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $projAbs -WorkingDirectory $RepoRoot -PassThru
    try {
        $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
            -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
        if (-not $resolved.Resolved) { throw "IPC URL unresolved for $CaseLabel" }
        $url = $resolved.Url
        if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
            throw "project_not_loaded for $CaseLabel"
        }
        Start-Sleep -Seconds 5

        Open-EditorsForFixture -Url $url -Fixture $Fixture -CaseLabel $CaseLabel

        $boardArg = if ($BoardFilenameOverride) { $BoardFilenameOverride } else { $pcbAbs }
        $argsPath = Join-Path $runDir ("save_board_" + $CaseLabel + ".args.json")
        $payload = @{
            board_filename = $boardArg
            confirm_save   = $false
        }
        foreach ($key in $ToolArgs.Keys) { $payload[$key] = $ToolArgs[$key] }
        ($payload | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8

        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "save_board_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("save_board_" + $CaseLabel + ".log")) -Encoding UTF8
        $parsed = Parse-SaveBoardOutput -Text $sm.Text
        $pcbAfter = Get-Item -LiteralPath $pcbAbs
        & $Assert $parsed @{ Url = $url; Fixture = $Fixture; CaseLabel = $CaseLabel; PcbBefore = $pcbBefore; PcbAfter = $pcbAfter }
        return @{ Parsed = $parsed; Url = $url; Fixture = $Fixture }
    }
    finally {
        if ($kicadProc -and !$kicadProc.HasExited) {
            Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
    }
}

function Invoke-PcbUpdateApplyOnOpenSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel
    )
    $schAbs = (Resolve-Path $Fixture.Sch).Path
    $pcbAbs = (Resolve-Path $Fixture.Pcb).Path
    $argsPath = Join-Path $runDir ("pcb_update_apply_" + $CaseLabel + ".args.json")
    (@{
        board_filename         = $pcbAbs
        schematic_filename     = $schAbs
        confirm_apply          = $true
        require_preview_green  = $true
        delete_orphans         = $false
        replace_footprints     = $true
        match_mode             = "reference_or_path"
        include_result_items   = $false
        max_items              = 50
    } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "pcb_update_from_schematic_session", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $sm.Text | Set-Content (Join-Path $runDir ("pcb_update_apply_" + $CaseLabel + ".log")) -Encoding UTF8
    if ($sm.Text -notmatch 'pcb_update_from_schematic_ok=true') { throw "apply failed for $CaseLabel" }
}

$confirmFx = Sync-SaveBoardFixture -DestSuffix "confirm"
$saveFx = Sync-SaveBoardFixture -DestSuffix "save"
$mismatchFx = Sync-SaveBoardFixture -DestSuffix "mismatch"
$regressionFx = Sync-SaveBoardFixture -DestSuffix "regression"

$confirmResult = Invoke-SaveBoardCase -Fixture $confirmFx -CaseLabel "confirm_required" -ToolArgs @{
    confirm_save = $false
} -Assert {
    param($p, $ctx)
    if ($p.exact_blocker_if_any -ne "save_confirmation_required") {
        throw "confirm: blocker expected save_confirmation_required got $($p.exact_blocker_if_any)"
    }
    if ($p.save_board_saved -ne "false") { throw "confirm: saved expected false" }
    if ($p.save_board_file_mtime_changed -ne "false") { throw "confirm: file mtime must not change" }
    if ($ctx.PcbBefore.LastWriteTimeUtc -ne $ctx.PcbAfter.LastWriteTimeUtc) {
        throw "confirm: disk file changed without confirm_save"
    }
}

Stop-KiCadProcesses
Start-Sleep -Seconds 5
$null = Clear-IpcSockets
$projAbsSave = (Resolve-Path $saveFx.Proj).Path
$pcbAbsSave = (Resolve-Path $saveFx.Pcb).Path
$pcbBeforeSave = Get-Item -LiteralPath $pcbAbsSave
$kicadProcSave = Start-Process -FilePath $kicadExe -ArgumentList $projAbsSave -WorkingDirectory $RepoRoot -PassThru
try {
    $resolvedSave = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcSave.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolvedSave.Resolved) { throw "IPC URL unresolved for save_confirmed" }
    $saveUrl = $resolvedSave.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $saveUrl -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded for save_confirmed"
    }
    Start-Sleep -Seconds 5
    Open-EditorsForFixture -Url $saveUrl -Fixture $saveFx -CaseLabel "save_confirmed"
    Invoke-PcbUpdateApplyOnOpenSession -Url $saveUrl -Fixture $saveFx -CaseLabel "save_confirmed_pre"

    $saveArgsPath = Join-Path $runDir "save_board_save_confirmed.args.json"
    (@{
        board_filename = $pcbAbsSave
        confirm_save   = $true
    } | ConvertTo-Json -Compress) | Set-Content $saveArgsPath -Encoding UTF8
    $saveSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($saveUrl, "mcp_call_tool_args", "save_board_session", ("@" + $saveArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $saveSm.Text | Set-Content (Join-Path $runDir "save_board_save_confirmed.log") -Encoding UTF8
    $saveParsed = Parse-SaveBoardOutput -Text $saveSm.Text
    $pcbAfterSave = Get-Item -LiteralPath $pcbAbsSave

    if ($saveParsed.save_board_ok -ne "true") { throw "save_confirmed: ok expected true" }
    if ($saveParsed.save_board_saved -ne "true") { throw "save_confirmed: saved expected true" }
    if ($saveParsed.save_board_confirm_save -ne "true") { throw "save_confirmed: confirm_save expected true" }
    if ($saveParsed.exact_blocker_if_any -ne "N/A") { throw "save_confirmed: blocker expected N/A" }
    if ($saveParsed.save_board_file_mtime_changed -ne "true") { throw "save_confirmed: mtime must change" }
}
finally {
    if ($kicadProcSave -and !$kicadProcSave.HasExited) {
        Stop-Process -Id $kicadProcSave.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

$mismatchResult = Invoke-SaveBoardCase -Fixture $mismatchFx -CaseLabel "board_mismatch" -ToolArgs @{
    confirm_save = $true
} -BoardFilenameOverride (Join-Path $BuildDir "nonexistent_board.kicad_pcb") -Assert {
    param($p, $ctx)
    if ($p.exact_blocker_if_any -ne "board_filename_mismatch") {
        throw "mismatch: blocker expected board_filename_mismatch got $($p.exact_blocker_if_any)"
    }
    if ($p.save_board_saved -ne "false") { throw "mismatch: saved expected false" }
}

Stop-KiCadProcesses
Start-Sleep -Seconds 5
$null = Clear-IpcSockets
$projAbsReg = (Resolve-Path $regressionFx.Proj).Path
$schAbsReg = (Resolve-Path $regressionFx.Sch).Path
$pcbAbsReg = (Resolve-Path $regressionFx.Pcb).Path
$kicadProcReg = Start-Process -FilePath $kicadExe -ArgumentList $projAbsReg -WorkingDirectory $RepoRoot -PassThru
try {
    $resolvedReg = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcReg.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolvedReg.Resolved) { throw "IPC URL unresolved for regression" }
    $regUrl = $resolvedReg.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $regUrl -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded for regression"
    }
    Start-Sleep -Seconds 5
    Open-EditorsForFixture -Url $regUrl -Fixture $regressionFx -CaseLabel "regression"
    $syncArgs = Join-Path $runDir "pcb_sync_readiness_regression.args.json"
    (@{
        board_filename     = $pcbAbsReg
        schematic_filename = $schAbsReg
        match_mode         = "reference_or_path"
        include_items      = $false
        max_items          = 50
    } | ConvertTo-Json -Compress) | Set-Content $syncArgs -Encoding UTF8
    $syncSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($regUrl, "mcp_call_tool_args", "get_pcb_sync_readiness_session", ("@" + $syncArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $syncSm.Text | Set-Content (Join-Path $runDir "pcb_sync_readiness_regression.log") -Encoding UTF8
    $syncParsed = Parse-PcbSyncReadinessOutput -Text $syncSm.Text
    if ($syncParsed.pcb_sync_readiness_ok -ne "true") { throw "regression: sync readiness ok expected true" }
}
finally {
    if ($kicadProcReg -and !$kicadProcReg.HasExited) {
        Stop-Process -Id $kicadProcReg.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

$confirmRequiredOk = ($confirmResult.Parsed.exact_blocker_if_any -eq "save_confirmation_required")
$saveConfirmedOk = ($saveParsed.save_board_saved -eq "true")
$mismatchOk = ($mismatchResult.Parsed.exact_blocker_if_any -eq "board_filename_mismatch")
$regressionOk = ($syncParsed.pcb_sync_readiness_ok -eq "true")

$summaryLines = @(
    "save_board_session_green=true"
    "confirm_required_ok=$($confirmRequiredOk.ToString().ToLower())"
    "save_confirmed_ok=$($saveConfirmedOk.ToString().ToLower())"
    "board_mismatch_ok=$($mismatchOk.ToString().ToLower())"
    "post_save_sync_readiness_ok=$($regressionOk.ToString().ToLower())"
    "exact_blocker_if_any=N/A"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_pcb_save_board_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
$summaryLines | ForEach-Object { Write-Host $_ }
exit 0
