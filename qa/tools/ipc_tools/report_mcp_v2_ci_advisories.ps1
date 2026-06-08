param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$SummaryPath,

    [string]$OutDir
)

# R180 — Read-only advisory interpreter for G4/G5/G6 from qa_mcp_v2_ci_baseline_summary.txt.
# Policy: MCP_DECISIONS.md R168 (G4/G5), R179 (G6). Does not modify CI summary or gates.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$commonModule = Join-Path $ScriptDir "ipc_qa_common.ps1"
if (!(Test-Path -LiteralPath $commonModule)) {
    Write-Error "Missing common IPC QA module at $commonModule"
    exit 1
}
. $commonModule

$BuildDir = (Resolve-Path $BuildDir).Path

function Find-LatestCiBaselineSummary {
    param([string]$Root)
    $artifactRoot = Join-Path $Root "ipc-v1-artifacts"
    if (!(Test-Path -LiteralPath $artifactRoot)) { return $null }
    $files = @(Get-ChildItem -Path $artifactRoot -Recurse -Filter "qa_mcp_v2_ci_baseline_summary.txt" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
    if ($files.Count -eq 0) { return $null }
    return $files[0].FullName
}

function Read-SummaryKv {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )
    $map = @{}
    foreach ($line in Get-Content -LiteralPath $Path) {
        if ($line -match '^([^=]+)=(.*)$') {
            $map[$Matches[1]] = $Matches[2]
        }
    }
    return $map
}

function Get-MapValue {
    param(
        [hashtable]$Map,
        [string]$Key,
        [string]$Default = "absent"
    )
    if ($Map.ContainsKey($Key)) { return $Map[$Key] }
    return $Default
}

function Test-IsNotApplicable {
    param([string]$Value)
    return ($Value -eq "N/A" -or $Value -eq "absent" -or [string]::IsNullOrWhiteSpace($Value))
}

function Test-IsSkipped {
    param([string]$Value)
    return ($Value -eq "skipped")
}

function Get-G4Status {
    param([hashtable]$Map)
    $ok = Get-MapValue $Map "runtime_footprint_workflow_ok"
    $green = Get-MapValue $Map "runtime_footprint_workflow_green"
    $mutation = Get-MapValue $Map "runtime_footprint_workflow_mutation_performed"

    if (Test-IsSkipped $ok -or Test-IsSkipped $green) { return "skipped" }
    if (Test-IsNotApplicable $ok -and Test-IsNotApplicable $green) { return "not_applicable" }
    if ($ok -eq "absent" -or $green -eq "absent") { return "unknown" }
    if ($mutation -eq "true") { return "incident_qa" }
    if ($ok -eq "false" -or $green -eq "false") { return "release_hold" }
    if ($ok -eq "true" -and $green -eq "true") { return "green" }
    return "unknown"
}

function Get-G5Status {
    param([hashtable]$Map)
    $ok = Get-MapValue $Map "footprint_workflow_discovery_consistency_ok"
    $green = Get-MapValue $Map "footprint_workflow_discovery_consistency_green"
    $scope = Get-MapValue $Map "footprint_workflow_discovery_consistency_scope_match_ok"

    if (Test-IsSkipped $ok -or Test-IsSkipped $green) { return "skipped" }
    if (Test-IsNotApplicable $ok -and Test-IsNotApplicable $green) { return "not_applicable" }
    if ($ok -eq "absent" -or $green -eq "absent") { return "unknown" }
    if ($scope -eq "false") { return "release_hold" }
    if ($ok -eq "false" -or $green -eq "false") { return "release_hold" }
    if ($ok -eq "true" -and $green -eq "true") { return "green" }
    return "unknown"
}

function Get-G6Status {
    param([hashtable]$Map)
    $ok = Get-MapValue $Map "pcb_apply_drc_save_workflow_ok"
    $green = Get-MapValue $Map "pcb_apply_drc_save_workflow_green"
    $applyConfirm = Get-MapValue $Map "pcb_apply_drc_save_workflow_apply_confirm_required_ok"
    $saveConfirm = Get-MapValue $Map "pcb_apply_drc_save_workflow_save_confirm_required_ok"
    $postSaveVerify = Get-MapValue $Map "pcb_apply_drc_save_workflow_post_save_verify_ok"

    if (Test-IsSkipped $ok -or Test-IsSkipped $green) { return "skipped" }
    if (Test-IsNotApplicable $ok -and Test-IsNotApplicable $green) { return "not_applicable" }
    if ($ok -eq "absent" -or $green -eq "absent") { return "unknown" }
    if ($applyConfirm -eq "false" -or $saveConfirm -eq "false" -or $postSaveVerify -eq "false") {
        return "incident_qa"
    }
    if ($ok -eq "false" -or $green -eq "false") { return "release_hold" }
    if ($ok -eq "true" -and $green -eq "true") { return "green" }
    return "unknown"
}

function Get-SeverityRank {
    param([string]$Status)
    switch ($Status) {
        "incident_qa"       { return 5 }
        "release_hold"      { return 4 }
        "warning"           { return 3 }
        "unknown"           { return 2 }
        "green"             { return 1 }
        "skipped"           { return 0 }
        "not_applicable"    { return 0 }
        default             { return 2 }
    }
}

function Get-OverallFromStatuses {
    param(
        [string]$G4,
        [string]$G5,
        [string]$G6
    )
    $ranks = @(
        (Get-SeverityRank $G4),
        (Get-SeverityRank $G5),
        (Get-SeverityRank $G6)
    )
    $max = ($ranks | Measure-Object -Maximum).Maximum
    switch ($max) {
        5 { return "incident_qa" }
        4 { return "release_hold" }
        3 { return "warning" }
        2 { return "unknown" }
        default {
            if ($G4 -eq "not_applicable" -and $G5 -eq "not_applicable" -and $G6 -eq "not_applicable") {
                return "green"
            }
            if ($G4 -eq "skipped" -and $G5 -eq "skipped" -and $G6 -eq "skipped") {
                return "green"
            }
            return "green"
        }
    }
}

function Get-ActionForOverall {
    param([string]$Overall)
    switch ($Overall) {
        "incident_qa"       { return "block_release_until_triaged" }
        "release_hold"      { return "manual_review_before_release" }
        "warning"           { return "rerun" }
        "unknown"           { return "manual_review_before_release" }
        default             { return "none" }
    }
}

# --- Resolve summary path ---
$resolvedSummary = $SummaryPath
if ([string]::IsNullOrWhiteSpace($resolvedSummary)) {
    $resolvedSummary = Find-LatestCiBaselineSummary -Root $BuildDir
}
if ([string]::IsNullOrWhiteSpace($resolvedSummary) -or !(Test-Path -LiteralPath $resolvedSummary)) {
    Write-Error "CI baseline summary not found under $BuildDir\ipc-v1-artifacts (qa_mcp_v2_ci_baseline_summary.txt)"
    exit 1
}

try {
    $kv = Read-SummaryKv -Path $resolvedSummary
}
catch {
    Write-Error "Unable to read CI baseline summary: $resolvedSummary ($($_.Exception.Message))"
    exit 1
}

if ($kv.Count -eq 0) {
    Write-Error "CI baseline summary is empty or unreadable: $resolvedSummary"
    exit 1
}

$outDirectory = $OutDir
if ([string]::IsNullOrWhiteSpace($outDirectory)) {
    $outDirectory = Split-Path -Parent $resolvedSummary
}
New-Item -ItemType Directory -Force -Path $outDirectory | Out-Null
$outDirectory = (Resolve-Path $outDirectory).Path

$g4Status = Get-G4Status -Map $kv
$g5Status = Get-G5Status -Map $kv
$g6Status = Get-G6Status -Map $kv
$overallStatus = Get-OverallFromStatuses -G4 $g4Status -G5 $g5Status -G6 $g6Status
$overallAction = Get-ActionForOverall -Overall $overallStatus

$ciBaselineGreen = Get-MapValue $kv "ci_baseline_green"
$mcpCheckpointGreen = Get-MapValue $kv "mcp_v2_checkpoint_green"
$verifyOnly = Get-MapValue $kv "verify_only"

$releaseManualReview = "false"
if ($overallStatus -in @("release_hold", "incident_qa", "unknown")) {
    $releaseManualReview = "true"
}
foreach ($key in @(
    "runtime_footprint_workflow_ok", "runtime_footprint_workflow_green",
    "footprint_workflow_discovery_consistency_ok", "footprint_workflow_discovery_consistency_green",
    "pcb_apply_drc_save_workflow_ok", "pcb_apply_drc_save_workflow_green"
)) {
    if ((Get-MapValue $kv $key) -eq "absent") {
        $releaseManualReview = "true"
        if ($overallStatus -eq "green") { $overallStatus = "unknown" }
        $overallAction = Get-ActionForOverall -Overall $overallStatus
    }
}

$reportGreen = if ($overallStatus -eq "green") { "true" } else { "false" }

$blocker = Get-MapValue $kv "exact_blocker_if_any" "N/A"
if ($blocker -eq "absent") { $blocker = "N/A" }

$g4Ok = Get-MapValue $kv "runtime_footprint_workflow_ok"
$g5Ok = Get-MapValue $kv "footprint_workflow_discovery_consistency_ok"
$g6Ok = Get-MapValue $kv "pcb_apply_drc_save_workflow_ok"

$summaryLines = @(
    "ci_advisory_report_green=$reportGreen"
    "ci_baseline_green=$ciBaselineGreen"
    "mcp_v2_checkpoint_green=$mcpCheckpointGreen"
    "verify_only=$verifyOnly"
    "advisory_overall_status=$overallStatus"
    "advisory_overall_action=$overallAction"
    "g4_status=$g4Status"
    "g5_status=$g5Status"
    "g6_status=$g6Status"
    "g4_runtime_footprint_workflow_ok=$g4Ok"
    "g5_footprint_workflow_discovery_consistency_ok=$g5Ok"
    "g6_pcb_apply_drc_save_workflow_ok=$g6Ok"
    "release_requires_manual_review=$releaseManualReview"
    "exact_blocker_if_any=$blocker"
    "source_ci_baseline_summary=$resolvedSummary"
    "report_generated_at=$(Get-Date -Format o)"
)

$txtPath = Join-Path $outDirectory "qa_mcp_v2_ci_advisory_report.txt"
$summaryLines | Set-Content -Path $txtPath -Encoding UTF8

$mdPath = Join-Path $outDirectory "qa_mcp_v2_ci_advisory_report.md"
@(
    "# MCP v2 CI advisory report (G4/G5/G6)"
    ""
    "Policy: R168 (G4/G5), R179 (G6). Read-only; does not modify CI gates."
    ""
    "| Field | Value |"
    "|-------|-------|"
    "| Source summary | ``$resolvedSummary`` |"
    "| Advisory overall | **$overallStatus** |"
    "| Recommended action | **$overallAction** |"
    "| CI baseline green | $ciBaselineGreen |"
    "| Checkpoint green | $mcpCheckpointGreen |"
    "| Verify only run | $verifyOnly |"
    "| Release manual review | $releaseManualReview |"
    ""
    "## Group status"
    ""
    "| Group | Status | ok |"
    "|-------|--------|-----|"
    "| G4 runtime footprint workflow | $g4Status | $g4Ok |"
    "| G5 footprint discovery consistency | $g5Status | $g5Ok |"
    "| G6 PCB apply DRC save workflow | $g6Status | $g6Ok |"
    ""
    "## G4 detail"
    ""
    "- mutation_performed: $(Get-MapValue $kv 'runtime_footprint_workflow_mutation_performed')"
    "- blocker: $(Get-MapValue $kv 'runtime_footprint_workflow_blocker')"
    "- target_run_dir: $(Get-MapValue $kv 'runtime_footprint_workflow_target_run_dir')"
    ""
    "## G5 detail"
    ""
    "- scope_match_ok: $(Get-MapValue $kv 'footprint_workflow_discovery_consistency_scope_match_ok')"
    "- blocker: $(Get-MapValue $kv 'footprint_workflow_discovery_consistency_blocker')"
    ""
    "## G6 detail"
    ""
    "- preview_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_preview_ok')"
    "- apply_confirm_required_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_apply_confirm_required_ok')"
    "- apply_with_drc_advisory_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok')"
    "- post_apply_drc_summary_present_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok')"
    "- post_apply_sync_readiness_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok')"
    "- save_confirm_required_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_save_confirm_required_ok')"
    "- save_confirmed_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_save_confirmed_ok')"
    "- post_save_verify_ok: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_post_save_verify_ok')"
    "- blocker: $(Get-MapValue $kv 'pcb_apply_drc_save_workflow_blocker')"
    ""
    "> ci_baseline_green=true does not override a red advisory (R179)."
) | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$outDirectory"
Write-Output "REPORT_FILE=$txtPath"
Write-Output "REPORT_MD=$mdPath"
$summaryLines | ForEach-Object { Write-Host $_ }
exit 0
