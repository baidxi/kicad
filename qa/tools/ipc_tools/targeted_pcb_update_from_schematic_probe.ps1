param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R173 — pcb_update_from_schematic_session controlled apply + restore probe.

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-update-from-schematic-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library for pcb update apply"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-PcbApplyFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseName,
        [string]$DestSuffix = $CaseName
    )
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $pcbSrc = Join-Path $RepoRoot ("qa\data\pcbnew\sync\ipc_pcb_sync_v1_" + $CaseName)
    $dst = Join-Path $BuildDir ("ipc-pcb-apply-v1-" + $DestSuffix + "-proj")
    if (!(Test-Path -LiteralPath $baseSrc)) { throw "Missing base fixture at $baseSrc" }
    if (!(Test-Path -LiteralPath $pcbSrc)) { throw "Missing pcb variant at $pcbSrc" }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $baseSrc "*") -Destination $dst -Recurse -Force
    $pcbVariant = Join-Path $pcbSrc ("ipc_pcb_sync_v1_" + $CaseName + ".kicad_pcb")
    if (!(Test-Path -LiteralPath $pcbVariant)) { throw "Missing pcb variant file $pcbVariant" }
    Copy-Item -Path $pcbVariant -Destination (Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb") -Force
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

function Parse-PcbUpdateApplyOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                         = ($Text -match 'status=1')
        pcb_update_from_schematic_ok      = (& $parseLine "pcb_update_from_schematic_ok")
        applied                           = (& $parseLine "applied")
        dry_run                           = (& $parseLine "dry_run")
        preview_checked                   = (& $parseLine "preview_checked")
        board_mutation_performed          = (& $parseLine "board_mutation_performed")
        footprints_before_count           = (& $parseLine "footprints_before_count")
        footprints_after_count            = (& $parseLine "footprints_after_count")
        added_footprints_count            = (& $parseLine "added_footprints_count")
        removed_orphan_footprints_count   = (& $parseLine "removed_orphan_footprints_count")
        delete_orphans                    = (& $parseLine "delete_orphans")
        recommended_next_action             = (& $parseLine "recommended_next_action")
        exact_blocker_if_any              = (& $parseLine "exact_blocker_if_any")
        post_apply_drc_summary_present    = (& $parseLine "pcb_update_post_apply_drc_summary_present")
        post_apply_drc_available          = (& $parseLine "pcb_update_post_apply_drc_available")
        post_apply_drc_violations_count   = (& $parseLine "pcb_update_post_apply_drc_violations_count")
        post_apply_drc_blocker            = (& $parseLine "pcb_update_post_apply_drc_blocker")
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
        orphan_footprints_count     = (& $parseLine "orphan_footprints_count")
        pcb_sync_likely             = (& $parseLine "pcb_sync_likely")
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

function Invoke-PcbSyncReadinessOnOpenSession {
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
    $argsPath = Join-Path $runDir ("pcb_sync_readiness_" + $CaseLabel + ".args.json")
    (@{
        board_filename     = $pcbAbs
        schematic_filename = $schAbs
        match_mode         = "reference_or_path"
        include_items      = $false
        max_items          = 50
    } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "get_pcb_sync_readiness_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $sm.Text | Set-Content (Join-Path $runDir ("pcb_sync_readiness_" + $CaseLabel + ".log")) -Encoding UTF8
    return Parse-PcbSyncReadinessOutput -Text $sm.Text
}

function Invoke-PcbUpdateApplyCase {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel,
        [Parameter(Mandatory = $true)]
        [hashtable]$ToolArgs,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Assert
    )
    Stop-KiCadProcesses
    Start-Sleep -Seconds 5
    $null = Clear-IpcSockets

    $schAbs = (Resolve-Path $Fixture.Sch).Path
    $pcbAbs = (Resolve-Path $Fixture.Pcb).Path
    $projAbs = (Resolve-Path $Fixture.Proj).Path

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

        $argsPath = Join-Path $runDir ("pcb_update_apply_" + $CaseLabel + ".args.json")
        $payload = @{
            board_filename     = $pcbAbs
            schematic_filename = $schAbs
            match_mode         = "reference_or_path"
            include_result_items = $true
            max_items          = 100
        }
        foreach ($key in $ToolArgs.Keys) { $payload[$key] = $ToolArgs[$key] }
        ($payload | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8

        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_update_from_schematic_session", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_update_apply_" + $CaseLabel + ".log")) -Encoding UTF8
        $parsed = Parse-PcbUpdateApplyOutput -Text $sm.Text
        & $Assert $parsed @{ Url = $url; Fixture = $Fixture; CaseLabel = $CaseLabel }
        return @{ Parsed = $parsed; Url = $url; Fixture = $Fixture }
    }
    finally {
        if ($kicadProc -and !$kicadProc.HasExited) {
            Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
    }
}

$emptyFx = Sync-PcbApplyFixture -CaseName "empty" -DestSuffix "confirm"
$emptyApplyFx = Sync-PcbApplyFixture -CaseName "empty" -DestSuffix "apply"
$emptyDrcFx = Sync-PcbApplyFixture -CaseName "empty" -DestSuffix "apply_drc"
$emptyRestoreFx = Sync-PcbApplyFixture -CaseName "empty" -DestSuffix "restore"
$orphanFx = Sync-PcbApplyFixture -CaseName "orphan" -DestSuffix "orphan-safe"

$goldenRestorePcb = Join-Path $runDir "golden_empty_restore.kicad_pcb"
Copy-Item -Path $emptyRestoreFx.Pcb -Destination $goldenRestorePcb -Force

$confirmParsed = Invoke-PcbUpdateApplyCase -Fixture $emptyFx -CaseLabel "confirm_required" -ToolArgs @{
    confirm_apply = $false
} -Assert {
    param($p, $ctx)
    if ($p.exact_blocker_if_any -ne "apply_confirmation_required") {
        throw "confirm: blocker expected apply_confirmation_required got $($p.exact_blocker_if_any)"
    }
    if ($p.applied -ne "false") { throw "confirm: applied expected false" }
    if ($p.board_mutation_performed -ne "false") { throw "confirm: board_mutation_performed expected false" }
    if ($p.footprints_before_count -ne $p.footprints_after_count) {
        throw "confirm: footprints count changed without apply"
    }
}

$emptyApplyResult = Invoke-PcbUpdateApplyCase -Fixture $emptyApplyFx -CaseLabel "empty_apply" -ToolArgs @{
    confirm_apply = $true
    require_preview_green = $true
    delete_orphans = $false
    replace_footprints = $true
} -Assert {
    param($p, $ctx)
    if ($p.pcb_update_from_schematic_ok -ne "true") { throw "empty_apply: ok expected true" }
    if ($p.applied -ne "true") { throw "empty_apply: applied expected true" }
    if ($p.dry_run -ne "false") { throw "empty_apply: dry_run expected false" }
    if ($p.preview_checked -ne "true") { throw "empty_apply: preview_checked expected true" }
    if ([int]$p.footprints_after_count -lt 2) { throw "empty_apply: footprints_after_count expected >= 2 got $($p.footprints_after_count)" }
    if ([int]$p.added_footprints_count -lt 2) { throw "empty_apply: added_footprints_count expected >= 2 got $($p.added_footprints_count)" }
    if ($p.exact_blocker_if_any -ne "N/A") { throw "empty_apply: blocker expected N/A" }
    if ($p.post_apply_drc_summary_present -ne "false") {
        throw "empty_apply: post_apply_drc_summary_present expected false got $($p.post_apply_drc_summary_present)"
    }
    $sync = Invoke-PcbSyncReadinessOnOpenSession -Url $ctx.Url -Fixture $ctx.Fixture -CaseLabel "empty_apply_post"
    if ($sync.missing_footprints_count -ne "0") {
        throw "empty_apply post sync: missing expected 0 got $($sync.missing_footprints_count)"
    }
}

$drcAdvisoryResult = Invoke-PcbUpdateApplyCase -Fixture $emptyDrcFx -CaseLabel "apply_drc_advisory" -ToolArgs @{
    confirm_apply = $true
    require_preview_green = $true
    delete_orphans = $false
    replace_footprints = $true
    include_post_apply_drc_summary = $true
    post_apply_drc_max_violations = 20
} -Assert {
    param($p, $ctx)
    if ($p.pcb_update_from_schematic_ok -ne "true") { throw "apply_drc_advisory: ok expected true" }
    if ($p.applied -ne "true") { throw "apply_drc_advisory: applied expected true" }
    if ($p.post_apply_drc_summary_present -ne "true") {
        throw "apply_drc_advisory: post_apply_drc_summary_present expected true"
    }
    if ($p.post_apply_drc_available -ne "true") {
        throw "apply_drc_advisory: post_apply_drc_available expected true got $($p.post_apply_drc_available)"
    }
    if ($p.post_apply_drc_blocker -ne "N/A") {
        throw "apply_drc_advisory: post_apply_drc_blocker expected N/A got $($p.post_apply_drc_blocker)"
    }
    if ($p.exact_blocker_if_any -ne "N/A") { throw "apply_drc_advisory: apply blocker expected N/A" }
}

$postApplySyncOk = $true

$restoreApplyResult = Invoke-PcbUpdateApplyCase -Fixture $emptyRestoreFx -CaseLabel "restore_apply" -ToolArgs @{
    confirm_apply = $true
    require_preview_green = $true
    delete_orphans = $false
    replace_footprints = $true
} -Assert {
    param($p, $ctx)
    if ($p.pcb_update_from_schematic_ok -ne "true") { throw "restore_apply: ok expected true" }
    if ([int]$p.footprints_after_count -lt 2) { throw "restore_apply: footprints_after expected >= 2" }
}

Stop-KiCadProcesses
Start-Sleep -Seconds 5
Copy-Item -Path $goldenRestorePcb -Destination $emptyRestoreFx.Pcb -Force

Stop-KiCadProcesses
Start-Sleep -Seconds 5
$null = Clear-IpcSockets
$projAbsRestore = (Resolve-Path $emptyRestoreFx.Proj).Path
$kicadProcRestore = Start-Process -FilePath $kicadExe -ArgumentList $projAbsRestore -WorkingDirectory $RepoRoot -PassThru
try {
    $resolvedRestore = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcRestore.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolvedRestore.Resolved) { throw "IPC URL unresolved for restore_readiness" }
    $restoreUrl = $resolvedRestore.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $restoreUrl -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded for restore_readiness"
    }
    Start-Sleep -Seconds 5
    Open-EditorsForFixture -Url $restoreUrl -Fixture $emptyRestoreFx -CaseLabel "restore_readiness"
    $restoreSync = Invoke-PcbSyncReadinessOnOpenSession -Url $restoreUrl -Fixture $emptyRestoreFx -CaseLabel "restore_post"
    if ($restoreSync.missing_footprints_count -ne "2") {
        throw "restore: missing_footprints_count expected 2 got $($restoreSync.missing_footprints_count)"
    }
    if ($restoreSync.needs_update_from_schematic -ne "true") {
        throw "restore: needs_update expected true"
    }
}
finally {
    if ($kicadProcRestore -and !$kicadProcRestore.HasExited) {
        Stop-Process -Id $kicadProcRestore.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

$orphanApplyResult = Invoke-PcbUpdateApplyCase -Fixture $orphanFx -CaseLabel "orphan_safe" -ToolArgs @{
    confirm_apply = $true
    require_preview_green = $true
    delete_orphans = $false
    replace_footprints = $true
} -Assert {
    param($p, $ctx)
    if ($p.pcb_update_from_schematic_ok -ne "true") { throw "orphan_safe: ok expected true" }
    if ([int]$p.removed_orphan_footprints_count -ne 0) {
        throw "orphan_safe: removed_orphan expected 0 got $($p.removed_orphan_footprints_count)"
    }
    if ($p.recommended_next_action -ne "review_orphans") {
        throw "orphan_safe: recommended_next_action expected review_orphans got $($p.recommended_next_action)"
    }
    $sync = Invoke-PcbSyncReadinessOnOpenSession -Url $ctx.Url -Fixture $ctx.Fixture -CaseLabel "orphan_safe_post"
    if ([int]$sync.orphan_footprints_count -lt 1) {
        throw "orphan_safe post sync: orphan_footprints_count expected >= 1 got $($sync.orphan_footprints_count)"
    }
}

$confirmRequiredOk = ($confirmParsed.Parsed.exact_blocker_if_any -eq "apply_confirmation_required")
$emptyApplyOk = ($emptyApplyResult.Parsed.pcb_update_from_schematic_ok -eq "true")
$postApplyDrcAdvisoryOk = ($drcAdvisoryResult.Parsed.post_apply_drc_summary_present -eq "true")
$postApplySyncReadinessOk = $postApplySyncOk
$restoreOk = $true
$orphanSafeDefaultOk = ($orphanApplyResult.Parsed.recommended_next_action -eq "review_orphans")
$noUnconfirmedMutationOk = ($confirmParsed.Parsed.board_mutation_performed -eq "false")

$summaryLines = @(
    "pcb_update_from_schematic_green=true"
    "confirm_required_ok=$($confirmRequiredOk.ToString().ToLower())"
    "empty_apply_ok=$($emptyApplyOk.ToString().ToLower())"
    "post_apply_drc_advisory_ok=$($postApplyDrcAdvisoryOk.ToString().ToLower())"
    "post_apply_sync_readiness_ok=$($postApplySyncReadinessOk.ToString().ToLower())"
    "restore_ok=$($restoreOk.ToString().ToLower())"
    "orphan_safe_default_ok=$($orphanSafeDefaultOk.ToString().ToLower())"
    "no_unconfirmed_mutation_ok=$($noUnconfirmedMutationOk.ToString().ToLower())"
    "exact_blocker_if_any=N/A"
    "empty_apply_footprints_after_count=$($emptyApplyResult.Parsed.footprints_after_count)"
    "empty_apply_added_footprints_count=$($emptyApplyResult.Parsed.added_footprints_count)"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_pcb_update_from_schematic_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
$summaryLines | ForEach-Object { Write-Host $_ }
exit 0
