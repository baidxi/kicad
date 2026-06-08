param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R177 — Example IA workflow: snapshot → preview → apply (DRC advisory) → sync → save → verify.
# Doc-only mini-release validation; hors checkpoint MCP v2.

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r177-pcb-apply-drc-save-workflow")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R177 apply drc save workflow"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-WorkflowFixture {
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $dst = Join-Path $BuildDir "ipc-r177-workflow-proj"
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
        Proj = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pro"
        Sch  = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_sch"
        Pcb  = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb"
    }
}

function Parse-LineField {
    param([string]$Text, [string]$Key)
    if ($Text -match ($Key + '=(true|false|null|absent|0|1|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
    return "absent"
}

function Invoke-McpTool {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [string]$ToolName,
        [Parameter(Mandatory = $true)]
        [hashtable]$ToolArgs,
        [Parameter(Mandatory = $true)]
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", $ToolName, ("@" + $jsonPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $sm.Text | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    if ($sm.Text -notmatch 'status=1') { throw "MCP tool $ToolName failed for $LogBaseName" }
    return $sm.Text
}

function Open-Editors {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [string]$SchAbs,
        [Parameter(Mandatory = $true)]
        [string]$PcbAbs
    )
    foreach ($pair in @(
            @{ type = "sch"; filename = $SchAbs; log = "open_sch" }
            @{ type = "pcb"; filename = $PcbAbs; log = "open_pcb" }
        )) {
        $openInput = Join-Path $runDir ("qa_open_editor_frame_" + $pair.log + ".json")
        (@{ type = $pair.type; filename = $pair.filename } | ConvertTo-Json -Compress) | Set-Content $openInput -Encoding UTF8
        $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $openSm.Text | Set-Content (Join-Path $runDir ("qa_open_editor_frame_" + $pair.log + ".log")) -Encoding UTF8
        if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame $($pair.type) failed" }
        Start-Sleep -Seconds 3
    }
}

$fixture = Sync-WorkflowFixture
$schAbs = (Resolve-Path $fixture.Sch).Path
$pcbAbs = (Resolve-Path $fixture.Pcb).Path
$projAbs = (Resolve-Path $fixture.Proj).Path

$previewOk = "false"
$applyConfirmRequiredOk = "false"
$applyWithDrcAdvisoryOk = "false"
$postApplyDrcSummaryPresentOk = "false"
$postApplySyncReadinessOk = "false"
$saveConfirmRequiredOk = "false"
$saveConfirmedOk = "false"
$postSaveVerifyOk = "false"
$mutationWithoutConfirmBlockedOk = "false"
$saveWithoutConfirmBlockedOk = "false"
$finalBlocker = "N/A"
$r177Green = "false"
$workflowError = $null

Stop-KiCadProcesses
Start-Sleep -Seconds 5
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $projAbs -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "IPC URL unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }
    Start-Sleep -Seconds 5
    Open-Editors -Url $url -SchAbs $schAbs -PcbAbs $pcbAbs

    $commonBoardArgs = @{
        board_filename     = $pcbAbs
        schematic_filename = $schAbs
        match_mode         = "reference_or_path"
    }

    # Step 1 — snapshot orientation (R172)
    $snapshotText = Invoke-McpTool -Url $url -ToolName "get_project_context_snapshot" -LogBaseName "step1_snapshot" -ToolArgs @{
        include_nets               = $false
        include_pcb_sync_summary   = $true
        pcb_sync_include_preview   = $true
    }
    if ((Parse-LineField $snapshotText "snapshot_pcb_sync_summary_present") -ne "1") {
        throw "step1: snapshot_pcb_sync_summary_present expected 1"
    }
    if ((Parse-LineField $snapshotText "snapshot_needs_update_from_schematic") -ne "true") {
        throw "step1: needs_update expected true"
    }

    # Step 2 — preview (R171)
    $previewText = Invoke-McpTool -Url $url -ToolName "pcb_update_from_schematic_preview_session" -LogBaseName "step2_preview" -ToolArgs (@{
        include_actions = $true
        max_actions     = 100
    } + $commonBoardArgs)
    if ((Parse-LineField $previewText "pcb_update_preview_ok") -ne "true") { throw "step2: preview ok expected true" }
    if ((Parse-LineField $previewText "dry_run") -ne "true") { throw "step2: dry_run expected true" }
    if ((Parse-LineField $previewText "board_mutation_detected") -ne "false") {
        throw "step2: board_mutation_detected expected false"
    }
    if ([int](Parse-LineField $previewText "would_add_footprints_count") -lt 2) {
        throw "step2: would_add_footprints_count expected >= 2"
    }
    $previewOk = "true"

    # Gate — apply without confirm (no mutation)
    $applyBlockedText = Invoke-McpTool -Url $url -ToolName "pcb_update_from_schematic_session" -LogBaseName "gate_apply_no_confirm" -ToolArgs (@{
        confirm_apply         = $false
        require_preview_green = $true
        delete_orphans        = $false
        replace_footprints    = $true
        include_result_items  = $false
        max_items             = 50
    } + $commonBoardArgs)
    if ((Parse-LineField $applyBlockedText "exact_blocker_if_any") -ne "apply_confirmation_required") {
        throw "gate_apply: blocker expected apply_confirmation_required"
    }
    if ((Parse-LineField $applyBlockedText "applied") -ne "false") { throw "gate_apply: applied expected false" }
    if ((Parse-LineField $applyBlockedText "board_mutation_performed") -ne "false") {
        throw "gate_apply: board_mutation_performed expected false"
    }
    $applyConfirmRequiredOk = "true"
    $mutationWithoutConfirmBlockedOk = "true"

    # Gate — save without confirm (before apply; disk must not be saved)
    $saveBlockedPreApplyText = Invoke-McpTool -Url $url -ToolName "save_board_session" -LogBaseName "gate_save_no_confirm_pre_apply" -ToolArgs @{
        board_filename = $pcbAbs
        confirm_save   = $false
    }
    if ((Parse-LineField $saveBlockedPreApplyText "save_board_exact_blocker_if_any") -ne "save_confirmation_required") {
        throw "gate_save_pre: blocker expected save_confirmation_required"
    }
    if ((Parse-LineField $saveBlockedPreApplyText "save_board_saved") -ne "false") { throw "gate_save_pre: saved expected false" }
    if ((Parse-LineField $saveBlockedPreApplyText "save_board_file_mtime_changed") -ne "false") {
        throw "gate_save_pre: file mtime must not change"
    }

    # Step 3 — apply with DRC advisory (R173 + R176)
    $applyText = Invoke-McpTool -Url $url -ToolName "pcb_update_from_schematic_session" -LogBaseName "step3_apply_drc_advisory" -ToolArgs (@{
        confirm_apply                  = $true
        require_preview_green          = $true
        delete_orphans                 = $false
        replace_footprints             = $true
        include_result_items           = $false
        max_items                      = 50
        include_post_apply_drc_summary = $true
        post_apply_drc_max_violations  = 20
    } + $commonBoardArgs)
    if ((Parse-LineField $applyText "pcb_update_from_schematic_ok") -ne "true") { throw "step3: apply ok expected true" }
    if ((Parse-LineField $applyText "applied") -ne "true") { throw "step3: applied expected true" }
    if ((Parse-LineField $applyText "pcb_update_post_apply_drc_summary_present") -ne "true") {
        throw "step3: post_apply_drc_summary_present expected true"
    }
    if ((Parse-LineField $applyText "pcb_update_post_apply_drc_available") -ne "true") {
        throw "step3: post_apply_drc_available expected true"
    }
    if ((Parse-LineField $applyText "pcb_update_post_apply_drc_blocker") -ne "N/A") {
        throw "step3: post_apply_drc_blocker expected N/A (advisory non bloquant)"
    }
    if ((Parse-LineField $applyText "exact_blocker_if_any") -ne "N/A") { throw "step3: apply blocker expected N/A" }
    $applyWithDrcAdvisoryOk = "true"
    $postApplyDrcSummaryPresentOk = "true"

    # Step 4 — post-apply sync readiness (R170)
    $syncPostApplyText = Invoke-McpTool -Url $url -ToolName "get_pcb_sync_readiness_session" -LogBaseName "step4_sync_post_apply" -ToolArgs (@{
        include_items = $false
        max_items     = 50
    } + $commonBoardArgs)
    if ((Parse-LineField $syncPostApplyText "pcb_sync_readiness_ok") -ne "true") { throw "step4: sync ok expected true" }
    if ((Parse-LineField $syncPostApplyText "missing_footprints_count") -ne "0") {
        throw "step4: missing_footprints_count expected 0"
    }
    if ((Parse-LineField $syncPostApplyText "needs_update_from_schematic") -ne "false") {
        throw "step4: needs_update expected false"
    }
    $postApplySyncReadinessOk = "true"

    # Gate — save without confirm after apply (in-memory dirty, no disk write)
    $saveBlockedPostApplyText = Invoke-McpTool -Url $url -ToolName "save_board_session" -LogBaseName "gate_save_no_confirm_post_apply" -ToolArgs @{
        board_filename = $pcbAbs
        confirm_save   = $false
    }
    if ((Parse-LineField $saveBlockedPostApplyText "save_board_exact_blocker_if_any") -ne "save_confirmation_required") {
        throw "gate_save_post: blocker expected save_confirmation_required"
    }
    if ((Parse-LineField $saveBlockedPostApplyText "save_board_saved") -ne "false") { throw "gate_save_post: saved expected false" }
    $saveConfirmRequiredOk = "true"
    $saveWithoutConfirmBlockedOk = "true"

    # Step 5 — save confirmed (R176)
    $saveText = Invoke-McpTool -Url $url -ToolName "save_board_session" -LogBaseName "step5_save_confirmed" -ToolArgs @{
        board_filename = $pcbAbs
        confirm_save   = $true
    }
    if ((Parse-LineField $saveText "save_board_ok") -ne "true") { throw "step5: save ok expected true" }
    if ((Parse-LineField $saveText "save_board_saved") -ne "true") { throw "step5: saved expected true" }
    if ((Parse-LineField $saveText "save_board_confirm_save") -ne "true") { throw "step5: confirm_save expected true" }
    if ((Parse-LineField $saveText "save_board_file_mtime_changed") -ne "true") {
        throw "step5: file mtime must change after save"
    }
    $saveConfirmedOk = "true"

    # Step 6 — optional full DRC detail (v1)
    $drcText = Invoke-McpTool -Url $url -ToolName "get_pcb_drc_violations_context_session" -LogBaseName "step6_drc_optional" -ToolArgs @{
        board_filename  = $pcbAbs
        run_drc         = $true
        max_violations  = 20
        include_items   = $false
    }
    if ((Parse-LineField $drcText "drc_ok") -ne "true") { throw "step6: drc ok expected true" }

    # Post-save verify — snapshot + sync still green
    $snapshotPostText = Invoke-McpTool -Url $url -ToolName "get_project_context_snapshot" -LogBaseName "step7_snapshot_post_save" -ToolArgs @{
        include_nets             = $false
        include_pcb_sync_summary = $true
        pcb_sync_include_preview = $false
    }
    if ((Parse-LineField $snapshotPostText "snapshot_pcb_sync_summary_present") -ne "1") {
        throw "step7: snapshot summary present expected 1"
    }
    if ((Parse-LineField $snapshotPostText "snapshot_pcb_sync_likely") -ne "true") {
        throw "step7: pcb_sync_likely expected true"
    }
    if ((Parse-LineField $snapshotPostText "snapshot_needs_update_from_schematic") -ne "false") {
        throw "step7: needs_update expected false"
    }

    $syncPostSaveText = Invoke-McpTool -Url $url -ToolName "get_pcb_sync_readiness_session" -LogBaseName "step7_sync_post_save" -ToolArgs (@{
        include_items = $false
        max_items     = 50
    } + $commonBoardArgs)
    if ((Parse-LineField $syncPostSaveText "pcb_sync_readiness_ok") -ne "true") { throw "step7: sync ok expected true" }
    if ((Parse-LineField $syncPostSaveText "missing_footprints_count") -ne "0") {
        throw "step7: missing expected 0"
    }
    $postSaveVerifyOk = "true"
    $r177Green = "true"
}
catch {
    $workflowError = $_
    $finalBlocker = $_.Exception.Message
}
finally {
    if ($kicadProc -and !$kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

$summaryLines = @(
    "r177_workflow_green=$r177Green"
    "preview_ok=$previewOk"
    "apply_confirm_required_ok=$applyConfirmRequiredOk"
    "apply_with_drc_advisory_ok=$applyWithDrcAdvisoryOk"
    "post_apply_drc_summary_present_ok=$postApplyDrcSummaryPresentOk"
    "post_apply_sync_readiness_ok=$postApplySyncReadinessOk"
    "save_confirm_required_ok=$saveConfirmRequiredOk"
    "save_confirmed_ok=$saveConfirmedOk"
    "post_save_verify_ok=$postSaveVerifyOk"
    "mutation_without_confirm_blocked_ok=$mutationWithoutConfirmBlockedOk"
    "save_without_confirm_blocked_ok=$saveWithoutConfirmBlockedOk"
    "exact_blocker_if_any=$finalBlocker"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_r177_pcb_apply_drc_save_workflow_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
Write-Output "TARGET_RUN_DIR=$runDir"
Write-Output "SUMMARY_FILE=$summaryPath"
$summaryLines | ForEach-Object { Write-Host $_ }
if ($r177Green -ne "true") {
    if ($workflowError) { throw $workflowError }
    exit 1
}
exit 0
