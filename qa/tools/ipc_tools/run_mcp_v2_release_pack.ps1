param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [switch]$SkipBuild,

    [switch]$KillRunningKiCad,

    [switch]$AllowWarning,

    [string]$OutDir
)

# R184/R185 — Local release pipeline: CI baseline + R180 + R181 + R182/R183 export.
# R185 — Official MCP v2 release command (see MCP_DECISIONS.md R185).
# Policy: MCP_DECISIONS.md R184/R185. Does not modify dev gates, handlers, or upstream scripts.

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

function Get-OutputValue {
    param(
        [object[]]$Lines,
        [string]$Prefix
    )
    $match = @($Lines | Where-Object { $_ -match "^$([regex]::Escape($Prefix))=" } | Select-Object -Last 1)
    if ($match.Count -eq 0) { return $null }
    if ($match[0] -match "^$([regex]::Escape($Prefix))=(.*)$") {
        return $Matches[1]
    }
    return $null
}

function Write-PipelineFailure {
    param(
        [string]$RunDir,
        [string]$Blocker,
        [string]$Message,
        [hashtable]$ExtraFields = @{}
    )
    if (![string]::IsNullOrWhiteSpace($RunDir)) {
        New-Item -ItemType Directory -Force -Path $RunDir | Out-Null
        $summaryPath = Join-Path $RunDir "qa_mcp_v2_release_pack_pipeline_summary.txt"
        $lines = @(
            "release_pack_pipeline_green=false"
            "release_decision=unknown"
            "exact_blocker_if_any=$Blocker"
            "pipeline_failed_at=$(Get-Date -Format o)"
        )
        foreach ($key in ($ExtraFields.Keys | Sort-Object)) {
            $lines += "$key=$($ExtraFields[$key])"
        }
        $lines | Set-Content -Path $summaryPath -Encoding UTF8
        Write-Output "TARGET_RUN_DIR=$RunDir"
        Write-Output "PIPELINE_SUMMARY_FILE=$summaryPath"
    }
    Write-Error $Message
    Write-Host "release_pack_pipeline_green=false"
    Write-Host "exact_blocker_if_any=$Blocker"
    exit 1
}

# --- Step 1: CI baseline (full run, not VerifyOnly) ---
$ciScript = Join-Path $ScriptDir "run_mcp_v2_ci_baseline.ps1"
if (!(Test-Path -LiteralPath $ciScript)) {
    Write-Error "Missing CI baseline script at $ciScript"
    exit 1
}

$ciArgs = @{
    BuildDir = $BuildDir
}
if ($SkipBuild.IsPresent) { $ciArgs.SkipBuild = $true }
if ($KillRunningKiCad.IsPresent) { $ciArgs.KillRunningKiCad = $true }

$ciOutput = @(& $ciScript @ciArgs 2>&1)
$ciExit = $LASTEXITCODE
$ciSummaryPath = Get-OutputValue -Lines $ciOutput -Prefix "SUMMARY_FILE"
$ciRunDir = Get-OutputValue -Lines $ciOutput -Prefix "TARGET_RUN_DIR"

if ($ciExit -ne 0 -or [string]::IsNullOrWhiteSpace($ciSummaryPath) -or !(Test-Path -LiteralPath $ciSummaryPath)) {
    $ciBlocker = if (![string]::IsNullOrWhiteSpace($ciSummaryPath)) {
        Read-ContractValue -SummaryPath $ciSummaryPath -Key "exact_blocker_if_any" -DefaultValue "ci_baseline_failed"
    } else { "ci_baseline_failed" }
    Write-PipelineFailure -RunDir $ciRunDir -Blocker $ciBlocker -Message "CI baseline failed (exit $ciExit)"
}

$artifactDir = $ciRunDir
if (![string]::IsNullOrWhiteSpace($OutDir)) {
    New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
    $artifactDir = (Resolve-Path $OutDir).Path
}

# --- Step 2: Advisory report (R180) ---
$reportScript = Join-Path $ScriptDir "report_mcp_v2_ci_advisories.ps1"
$reportOutput = @(& $reportScript -BuildDir $BuildDir -SummaryPath $ciSummaryPath -OutDir $artifactDir 2>&1)
if ($LASTEXITCODE -ne 0) {
    Write-PipelineFailure -RunDir $artifactDir -Blocker "advisory_report_failed" -Message "Advisory report generation failed"
}
$advisoryReportPath = Get-OutputValue -Lines $reportOutput -Prefix "REPORT_FILE"
if ([string]::IsNullOrWhiteSpace($advisoryReportPath)) {
    $advisoryReportPath = Join-Path $artifactDir "qa_mcp_v2_ci_advisory_report.txt"
}

# --- Step 3: Release check (R181) ---
$checkScript = Join-Path $ScriptDir "check_mcp_v2_release_advisories.ps1"
$checkArgs = @{
    BuildDir             = $BuildDir
    SummaryPath          = $ciSummaryPath
    AdvisoryReportPath   = $advisoryReportPath
}
if ($AllowWarning.IsPresent) { $checkArgs.AllowWarning = $true }

$checkOutput = @(& $checkScript @checkArgs 2>&1)
$checkExit = $LASTEXITCODE
$releaseCheckPath = Get-OutputValue -Lines $checkOutput -Prefix "RELEASE_CHECK_FILE"
if ([string]::IsNullOrWhiteSpace($releaseCheckPath)) {
    $releaseCheckPath = Join-Path $artifactDir "qa_mcp_v2_release_advisory_check_summary.txt"
}
if (!(Test-Path -LiteralPath $releaseCheckPath)) {
    Write-PipelineFailure -RunDir $artifactDir -Blocker "missing_release_check_summary" -Message "Release advisory check summary not found"
}

# --- Step 4: Export release pack (R182/R183) ---
$exportScript = Join-Path $ScriptDir "export_mcp_v2_release_pack.ps1"
$exportOutput = @(& $exportScript -BuildDir $BuildDir -SummaryPath $ciSummaryPath `
    -AdvisoryReportPath $advisoryReportPath -ReleaseCheckPath $releaseCheckPath `
    -OutDir $artifactDir -IncludeCheckpointSummary 2>&1)
if ($LASTEXITCODE -ne 0) {
    Write-PipelineFailure -RunDir $artifactDir -Blocker "release_pack_export_failed" -Message "Release pack export failed"
}

$releasePackMd = Get-OutputValue -Lines $exportOutput -Prefix "RELEASE_PACK_MD"
$releasePackJson = Get-OutputValue -Lines $exportOutput -Prefix "RELEASE_PACK_JSON"
if ([string]::IsNullOrWhiteSpace($releasePackMd)) {
    $releasePackMd = Join-Path $artifactDir "qa_mcp_v2_release_pack.md"
}
if ([string]::IsNullOrWhiteSpace($releasePackJson)) {
    $releasePackJson = Join-Path $artifactDir "qa_mcp_v2_release_pack.json"
}

# --- Pipeline summary ---
$releaseDecision = Read-ContractValue -SummaryPath $releaseCheckPath -Key "release_decision"
$advisoryOverall = Read-ContractValue -SummaryPath $releaseCheckPath -Key "advisory_overall_status"
$g4Status = Read-ContractValue -SummaryPath $releaseCheckPath -Key "g4_status"
$g5Status = Read-ContractValue -SummaryPath $releaseCheckPath -Key "g5_status"
$g6Status = Read-ContractValue -SummaryPath $releaseCheckPath -Key "g6_status"
$ciBaselineGreen = Read-ContractValue -SummaryPath $ciSummaryPath -Key "ci_baseline_green"
$mcpCheckpointGreen = Read-ContractValue -SummaryPath $ciSummaryPath -Key "mcp_v2_checkpoint_green"
$checkpointFamilyCount = Read-ContractValue -SummaryPath $ciSummaryPath -Key "checkpoint_family_count" -DefaultValue "9"
$blocker = Read-ContractValue -SummaryPath $releaseCheckPath -Key "exact_blocker_if_any" -DefaultValue "N/A"
if ($blocker -eq "absent") { $blocker = "N/A" }

$checkpointIncluded = "false"
if (Test-Path -LiteralPath $releasePackJson) {
    try {
        $packJsonObj = Get-Content -LiteralPath $releasePackJson -Raw | ConvertFrom-Json
        if ($null -ne $packJsonObj.checkpoint_summary_included) {
            $checkpointIncluded = $packJsonObj.checkpoint_summary_included.ToString().ToLower()
        }
    }
    catch {
        $checkpointIncluded = "false"
    }
}

$pipelineGreen = "false"
$pipelineExit = 1
if ($releaseDecision -eq "go") {
    $pipelineGreen = "true"
    $pipelineExit = 0
}
elseif ($releaseDecision -eq "warning" -and $AllowWarning.IsPresent) {
    $pipelineGreen = "true"
    $pipelineExit = 0
}

$pipelineSummaryPath = Join-Path $artifactDir "qa_mcp_v2_release_pack_pipeline_summary.txt"
$pipelineLines = @(
    "release_pack_pipeline_green=$pipelineGreen"
    "release_decision=$releaseDecision"
    "ci_baseline_green=$ciBaselineGreen"
    "mcp_v2_checkpoint_green=$mcpCheckpointGreen"
    "checkpoint_family_count=$checkpointFamilyCount"
    "advisory_overall_status=$advisoryOverall"
    "g4_status=$g4Status"
    "g5_status=$g5Status"
    "g6_status=$g6Status"
    "checkpoint_summary_included=$checkpointIncluded"
    "release_pack_markdown_path=$releasePackMd"
    "release_pack_json_path=$releasePackJson"
    "ci_baseline_summary_path=$ciSummaryPath"
    "advisory_report_path=$advisoryReportPath"
    "release_check_path=$releaseCheckPath"
    "allow_warning=$($AllowWarning.IsPresent.ToString().ToLower())"
    "ci_baseline_exit_code=$ciExit"
    "release_check_exit_code=$checkExit"
    "exact_blocker_if_any=$blocker"
    "pipeline_finished_at=$(Get-Date -Format o)"
)
$pipelineLines | Set-Content -Path $pipelineSummaryPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$artifactDir"
Write-Output "PIPELINE_SUMMARY_FILE=$pipelineSummaryPath"
Write-Output "RELEASE_PACK_MD=$releasePackMd"
Write-Output "RELEASE_PACK_JSON=$releasePackJson"
$pipelineLines | ForEach-Object { Write-Host $_ }
exit $pipelineExit
