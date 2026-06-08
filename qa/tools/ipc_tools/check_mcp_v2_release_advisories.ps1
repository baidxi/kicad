param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$SummaryPath,

    [string]$AdvisoryReportPath,

    [switch]$AllowWarning,

    [switch]$RequireGreen
)

# R181 — Release go/no-go wrapper over R180 advisory report (read-only).
# Policy: MCP_DECISIONS.md R181. Does not modify CI gates, checkpoint, or handlers.

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

function Find-LatestAdvisoryReport {
    param([string]$Root)
    $artifactRoot = Join-Path $Root "ipc-v1-artifacts"
    if (!(Test-Path -LiteralPath $artifactRoot)) { return $null }
    $files = @(Get-ChildItem -Path $artifactRoot -Recurse -Filter "qa_mcp_v2_ci_advisory_report.txt" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
    if ($files.Count -eq 0) { return $null }
    return $files[0].FullName
}

function Read-ReportKv {
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

function Get-ReportValue {
    param(
        [hashtable]$Map,
        [string]$Key,
        [string]$Default = "absent"
    )
    if ($Map.ContainsKey($Key)) { return $Map[$Key] }
    return $Default
}

function Test-AdvisoriesNotRuntimeValidated {
    param(
        [string]$G4,
        [string]$G5,
        [string]$G6
    )
    $nonRuntime = @("not_applicable", "skipped")
    return (($G4 -in $nonRuntime) -and ($G5 -in $nonRuntime) -and ($G6 -in $nonRuntime))
}

# --- Resolve advisory report ---
$resolvedReport = $AdvisoryReportPath
if ([string]::IsNullOrWhiteSpace($resolvedReport)) {
    $reportScript = Join-Path $ScriptDir "report_mcp_v2_ci_advisories.ps1"
    if (!(Test-Path -LiteralPath $reportScript)) {
        Write-Error "Missing advisory report script at $reportScript"
        exit 1
    }

    $reportArgs = @{ BuildDir = $BuildDir }
    if (![string]::IsNullOrWhiteSpace($SummaryPath)) {
        $reportArgs.SummaryPath = $SummaryPath
    }

    $reportOutput = & $reportScript @reportArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Advisory report generation failed (exit $LASTEXITCODE)"
        exit 1
    }

    $reportFileLine = @($reportOutput | Where-Object { $_ -match '^REPORT_FILE=' } | Select-Object -Last 1)
    if ($reportFileLine.Count -gt 0 -and $reportFileLine[0] -match '^REPORT_FILE=(.+)$') {
        $resolvedReport = $Matches[1]
    }
    else {
        $resolvedReport = Find-LatestAdvisoryReport -Root $BuildDir
    }
}

if ([string]::IsNullOrWhiteSpace($resolvedReport) -or !(Test-Path -LiteralPath $resolvedReport)) {
    Write-Error "Advisory report not found (qa_mcp_v2_ci_advisory_report.txt)"
    exit 1
}

try {
    $kv = Read-ReportKv -Path $resolvedReport
}
catch {
    Write-Error "Unable to read advisory report: $resolvedReport ($($_.Exception.Message))"
    exit 1
}

if ($kv.Count -eq 0) {
    Write-Error "Advisory report is empty or unreadable: $resolvedReport"
    exit 1
}

$overallStatus = Get-ReportValue $kv "advisory_overall_status"
$releaseManualReview = Get-ReportValue $kv "release_requires_manual_review"
$g4Status = Get-ReportValue $kv "g4_status"
$g5Status = Get-ReportValue $kv "g5_status"
$g6Status = Get-ReportValue $kv "g6_status"
$verifyOnly = Get-ReportValue $kv "verify_only"
$blocker = Get-ReportValue $kv "exact_blocker_if_any" "N/A"
if ($blocker -eq "absent") { $blocker = "N/A" }

$releaseDecision = switch ($overallStatus) {
    "green"   { "go" }
    "warning" { "manual_review_required" }
    "release_hold" { "hold_release" }
    "incident_qa"  { "incident_qa" }
    default        { "unknown" }
}

# VerifyOnly / skipped advisories: R180 may report overall=green, but release must not treat as go.
$advisoriesRuntimeValidated = -not (Test-AdvisoriesNotRuntimeValidated -G4 $g4Status -G5 $g5Status -G6 $g6Status)
if ($verifyOnly -eq "true" -or !$advisoriesRuntimeValidated) {
    if ($releaseDecision -eq "go") {
        $releaseDecision = "manual_review_required"
    }
    $releaseManualReview = "true"
}

if ($releaseManualReview -eq "true" -and $releaseDecision -eq "go") {
    $releaseDecision = "manual_review_required"
}

$releaseAdvisoryCheckGreen = "false"
$exitCode = 1

if ($overallStatus -eq "green" -and $releaseDecision -eq "go" -and $releaseManualReview -ne "true") {
    $releaseAdvisoryCheckGreen = "true"
    $exitCode = 0
}
elseif ($overallStatus -eq "warning") {
    if ($AllowWarning -and $releaseManualReview -ne "true") {
        $releaseDecision = "go"
        $releaseAdvisoryCheckGreen = "true"
        $exitCode = 0
    }
    elseif ($RequireGreen) {
        $releaseDecision = "manual_review_required"
        $exitCode = 1
    }
    else {
        $exitCode = 1
    }
}
elseif ($overallStatus -in @("release_hold", "incident_qa", "unknown")) {
    $exitCode = 1
}
elseif ($releaseManualReview -eq "true") {
    $exitCode = 1
}

$outDirectory = Split-Path -Parent $resolvedReport
New-Item -ItemType Directory -Force -Path $outDirectory | Out-Null
$outDirectory = (Resolve-Path $outDirectory).Path

$summaryLines = @(
    "release_advisory_check_green=$releaseAdvisoryCheckGreen"
    "release_decision=$releaseDecision"
    "advisory_overall_status=$overallStatus"
    "release_requires_manual_review=$releaseManualReview"
    "g4_status=$g4Status"
    "g5_status=$g5Status"
    "g6_status=$g6Status"
    "exact_blocker_if_any=$blocker"
    "verify_only=$verifyOnly"
    "advisories_runtime_validated=$($advisoriesRuntimeValidated.ToString().ToLower())"
    "allow_warning=$($AllowWarning.IsPresent.ToString().ToLower())"
    "require_green=$($RequireGreen.IsPresent.ToString().ToLower())"
    "source_advisory_report=$resolvedReport"
    "check_generated_at=$(Get-Date -Format o)"
)

$summaryPath = Join-Path $outDirectory "qa_mcp_v2_release_advisory_check_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$outDirectory"
Write-Output "RELEASE_CHECK_FILE=$summaryPath"
$summaryLines | ForEach-Object { Write-Host $_ }
exit $exitCode
