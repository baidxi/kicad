param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$SummaryPath,

    [string]$AdvisoryReportPath,

    [string]$ReleaseCheckPath,

    [string]$OutDir,

    [string]$CheckpointSummaryPath,

    [switch]$IncludeCheckpointSummary
)

# R182/R183 — Read-only release pack export (Markdown + JSON) from CI / R180 / R181 summaries.
# R183: optional -IncludeCheckpointSummary embeds MCP v2 checkpoint family detail.
# Policy: MCP_DECISIONS.md R182/R183. Does not modify gates, handlers, or upstream scripts.

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

function Find-LatestFile {
    param(
        [string]$Root,
        [string]$Filter
    )
    $artifactRoot = Join-Path $Root "ipc-v1-artifacts"
    if (!(Test-Path -LiteralPath $artifactRoot)) { return $null }
    $files = @(Get-ChildItem -Path $artifactRoot -Recurse -Filter $Filter -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending)
    if ($files.Count -eq 0) { return $null }
    return $files[0].FullName
}

function Read-KvFile {
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

function Get-KvValue {
    param(
        [hashtable]$Map,
        [string]$Key,
        [string]$Default = "absent"
    )
    if ($Map.ContainsKey($Key)) { return $Map[$Key] }
    return $Default
}

function Convert-ToJsonInt {
    param([string]$Value)
    if ($Value -eq "absent" -or $Value -eq "N/A" -or [string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }
    return [int]$Value
}

function Convert-ToJsonScalar {
    param([string]$Value)
    switch ($Value) {
        "true"  { return $true }
        "false" { return $false }
        "N/A"   { return $null }
        "absent" { return $null }
        default { return $Value }
    }
}

function Get-AdvisoryAction {
    param([string]$Status)
    switch ($Status) {
        "green"            { return "none" }
        "warning"          { return "rerun" }
        "release_hold"     { return "manual_review_before_release" }
        "incident_qa"      { return "block_release_until_triaged" }
        "not_applicable"   { return "not_applicable" }
        "skipped"          { return "skipped" }
        default            { return "manual_review_before_release" }
    }
}

function Format-ChecklistLine {
    param(
        [bool]$Checked,
        [string]$Label
    )
    if ($Checked) { return "- [x] $Label" }
    return "- [ ] $Label"
}

function Write-ExportFailure {
    param(
        [string]$Blocker,
        [string]$Message
    )
    Write-Error $Message
    Write-Host "export_blocker_if_any=$Blocker"
    Write-Host "release_pack_green=false"
    exit 1
}

function Get-CheckpointFamilyNames {
    param([hashtable]$Map)
    $names = New-Object System.Collections.Generic.List[string]
    foreach ($key in $Map.Keys) {
        if ($key -match '^(.+)_ok$') {
            $familyName = $Matches[1]
            if ($familyName -in @('mcp_v2_checkpoint', 'kiface_smoke')) { continue }
            if ($Map.ContainsKey("${familyName}_exit_code")) {
                [void]$names.Add($familyName)
            }
        }
    }
    return @($names | Sort-Object)
}

function Build-CheckpointFamilyEntries {
    param([hashtable]$Map)
    $entries = @()
    foreach ($familyName in (Get-CheckpointFamilyNames -Map $Map)) {
        $okValue = Get-KvValue $Map "${familyName}_ok"
        $exitCodeValue = Get-KvValue $Map "${familyName}_exit_code"
        $elapsedValue = Get-KvValue $Map "${familyName}_elapsed_ms"
        $entries += ,[ordered]@{
            name       = $familyName
            ok         = (Convert-ToJsonScalar $okValue)
            exit_code  = (Convert-ToJsonInt $exitCodeValue)
            elapsed_ms = (Convert-ToJsonInt $elapsedValue)
        }
    }
    return $entries
}

function Resolve-CheckpointSummary {
    param(
        [string]$ExplicitPath,
        [hashtable]$CiMap,
        [string]$Root
    )
    if (![string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (Test-Path -LiteralPath $ExplicitPath) {
            return @{
                Path       = (Resolve-Path $ExplicitPath).Path
                Confidence = "explicit"
            }
        }
        return @{
            Path       = $null
            Confidence = "missing"
        }
    }

    $ciLinked = Get-KvValue $CiMap "checkpoint_summary_path"
    if ($ciLinked -ne "absent" -and $ciLinked -ne "N/A" -and ![string]::IsNullOrWhiteSpace($ciLinked) -and (Test-Path -LiteralPath $ciLinked)) {
        return @{
            Path       = (Resolve-Path $ciLinked).Path
            Confidence = "same_run"
        }
    }

    $latest = Find-LatestFile -Root $Root -Filter "qa_mcp_v2_checkpoint_summary.txt"
    if (![string]::IsNullOrWhiteSpace($latest) -and (Test-Path -LiteralPath $latest)) {
        return @{
            Path       = $latest
            Confidence = "latest"
        }
    }

    return @{
        Path       = $null
        Confidence = "missing"
    }
}

function Format-ElapsedDisplay {
    param([string]$Value)
    if ($Value -eq "absent" -or [string]::IsNullOrWhiteSpace($Value)) { return "N/A" }
    return $Value
}

# --- Resolve CI baseline summary ---
$resolvedCiSummary = $SummaryPath
if ([string]::IsNullOrWhiteSpace($resolvedCiSummary)) {
    $resolvedCiSummary = Find-LatestFile -Root $BuildDir -Filter "qa_mcp_v2_ci_baseline_summary.txt"
}
if ([string]::IsNullOrWhiteSpace($resolvedCiSummary) -or !(Test-Path -LiteralPath $resolvedCiSummary)) {
    Write-ExportFailure -Blocker "missing_ci_baseline_summary" -Message "CI baseline summary not found (qa_mcp_v2_ci_baseline_summary.txt)"
}

try {
    $ciKv = Read-KvFile -Path $resolvedCiSummary
}
catch {
    Write-ExportFailure -Blocker "unreadable_ci_baseline_summary" -Message "Unable to read CI baseline summary: $resolvedCiSummary ($($_.Exception.Message))"
}

if ($ciKv.Count -eq 0) {
    Write-ExportFailure -Blocker "unreadable_ci_baseline_summary" -Message "CI baseline summary is empty: $resolvedCiSummary"
}

$ciRunDir = Split-Path -Parent $resolvedCiSummary

# --- Resolve advisory report (same run dir first; R182 does not auto-generate) ---
$resolvedAdvisory = $AdvisoryReportPath
if ([string]::IsNullOrWhiteSpace($resolvedAdvisory)) {
    $sameDirAdvisory = Join-Path $ciRunDir "qa_mcp_v2_ci_advisory_report.txt"
    if (Test-Path -LiteralPath $sameDirAdvisory) {
        $resolvedAdvisory = $sameDirAdvisory
    }
}
if ([string]::IsNullOrWhiteSpace($resolvedAdvisory) -or !(Test-Path -LiteralPath $resolvedAdvisory)) {
    Write-ExportFailure -Blocker "missing_advisory_report" -Message "Advisory report not found for CI run. Run report_mcp_v2_ci_advisories.ps1 first (qa_mcp_v2_ci_advisory_report.txt)"
}

try {
    $advisoryKv = Read-KvFile -Path $resolvedAdvisory
}
catch {
    Write-ExportFailure -Blocker "unreadable_advisory_report" -Message "Unable to read advisory report: $resolvedAdvisory ($($_.Exception.Message))"
}

if ($advisoryKv.Count -eq 0) {
    Write-ExportFailure -Blocker "unreadable_advisory_report" -Message "Advisory report is empty: $resolvedAdvisory"
}

# --- Resolve release check summary (same run dir first) ---
$resolvedReleaseCheck = $ReleaseCheckPath
if ([string]::IsNullOrWhiteSpace($resolvedReleaseCheck)) {
    $sameDirRelease = Join-Path $ciRunDir "qa_mcp_v2_release_advisory_check_summary.txt"
    if (Test-Path -LiteralPath $sameDirRelease) {
        $resolvedReleaseCheck = $sameDirRelease
    }
}
if ([string]::IsNullOrWhiteSpace($resolvedReleaseCheck) -or !(Test-Path -LiteralPath $resolvedReleaseCheck)) {
    Write-ExportFailure -Blocker "missing_release_check_summary" -Message "Release advisory check not found. Run check_mcp_v2_release_advisories.ps1 first (qa_mcp_v2_release_advisory_check_summary.txt)"
}

try {
    $releaseKv = Read-KvFile -Path $resolvedReleaseCheck
}
catch {
    Write-ExportFailure -Blocker "unreadable_release_check_summary" -Message "Unable to read release check summary: $resolvedReleaseCheck ($($_.Exception.Message))"
}

if ($releaseKv.Count -eq 0) {
    Write-ExportFailure -Blocker "unreadable_release_check_summary" -Message "Release check summary is empty: $resolvedReleaseCheck"
}

# --- Aggregate fields (release decision from R181, not R180 overall alone) ---
$releaseDecision = Get-KvValue $releaseKv "release_decision"
$releaseAdvisoryCheckGreen = Get-KvValue $releaseKv "release_advisory_check_green"
$releaseManualReview = Get-KvValue $releaseKv "release_requires_manual_review"
$advisoryOverall = Get-KvValue $releaseKv "advisory_overall_status"
if ($advisoryOverall -eq "absent") {
    $advisoryOverall = Get-KvValue $advisoryKv "advisory_overall_status"
}

$g4Status = Get-KvValue $releaseKv "g4_status"
$g5Status = Get-KvValue $releaseKv "g5_status"
$g6Status = Get-KvValue $releaseKv "g6_status"

$ciBaselineGreen = Get-KvValue $ciKv "ci_baseline_green"
$mcpCheckpointGreen = Get-KvValue $ciKv "mcp_v2_checkpoint_green"
$checkpointFamilyCount = Get-KvValue $ciKv "checkpoint_family_count" "9"
if ($checkpointFamilyCount -eq "absent") { $checkpointFamilyCount = "9" }

$g4Ok = Get-KvValue $advisoryKv "g4_runtime_footprint_workflow_ok"
if ($g4Ok -eq "absent") { $g4Ok = Get-KvValue $ciKv "runtime_footprint_workflow_ok" }

$g5Ok = Get-KvValue $advisoryKv "g5_footprint_workflow_discovery_consistency_ok"
if ($g5Ok -eq "absent") { $g5Ok = Get-KvValue $ciKv "footprint_workflow_discovery_consistency_ok" }

$g6Ok = Get-KvValue $advisoryKv "g6_pcb_apply_drc_save_workflow_ok"
if ($g6Ok -eq "absent") { $g6Ok = Get-KvValue $ciKv "pcb_apply_drc_save_workflow_ok" }

$blocker = Get-KvValue $releaseKv "exact_blocker_if_any" "N/A"
if ($blocker -eq "absent") { $blocker = "N/A" }

$verifyOnly = Get-KvValue $advisoryKv "verify_only"
if ($verifyOnly -eq "absent") { $verifyOnly = Get-KvValue $ciKv "verify_only" }

$outDirectory = $OutDir
if ([string]::IsNullOrWhiteSpace($outDirectory)) {
    $outDirectory = $ciRunDir
}
New-Item -ItemType Directory -Force -Path $outDirectory | Out-Null
$outDirectory = (Resolve-Path $outDirectory).Path

$generatedAt = Get-Date -Format o

$checkpointSummaryIncluded = $false
$checkpointSummaryMatchConfidence = "missing"
$resolvedCheckpointSummary = $null
$checkpointSummaryBlocker = "N/A"
$checkpointDetailObject = $null
$checkpointMdSection = @()

if ($IncludeCheckpointSummary.IsPresent) {
    $checkpointResolution = Resolve-CheckpointSummary -ExplicitPath $CheckpointSummaryPath -CiMap $ciKv -Root $BuildDir
    $checkpointSummaryMatchConfidence = $checkpointResolution.Confidence
    $resolvedCheckpointSummary = $checkpointResolution.Path

    if (![string]::IsNullOrWhiteSpace($resolvedCheckpointSummary) -and (Test-Path -LiteralPath $resolvedCheckpointSummary)) {
        try {
            $checkpointKv = Read-KvFile -Path $resolvedCheckpointSummary
        }
        catch {
            $checkpointKv = @{}
        }

        if ($checkpointKv.Count -gt 0) {
            $checkpointSummaryIncluded = $true
            $checkpointSummaryBlocker = "N/A"
            $checkpointFamilies = Build-CheckpointFamilyEntries -Map $checkpointKv
            $cpGreen = Get-KvValue $checkpointKv "mcp_v2_checkpoint_green"
            $cpFamilyCount = Get-KvValue $checkpointKv "checkpoint_family_count"
            if ($cpFamilyCount -eq "absent") {
                $cpFamilyCount = [string]$checkpointFamilies.Count
            }
            $cpKifaceSmoke = Get-KvValue $checkpointKv "kiface_smoke_ok"

            $checkpointDetailObject = [ordered]@{
                mcp_v2_checkpoint_green = (Convert-ToJsonScalar $cpGreen)
                checkpoint_family_count = (Convert-ToJsonInt $cpFamilyCount)
                kiface_smoke_ok         = (Convert-ToJsonScalar $cpKifaceSmoke)
                checkpoint_elapsed_ms   = (Convert-ToJsonInt (Get-KvValue $checkpointKv "checkpoint_elapsed_ms"))
                exact_blocker_if_any    = (Get-KvValue $checkpointKv "exact_blocker_if_any" "N/A")
                families                = $checkpointFamilies
            }

            $familyTableRows = New-Object System.Collections.Generic.List[string]
            foreach ($family in $checkpointFamilies) {
                $familyName = $family.name
                $elapsedDisplay = Format-ElapsedDisplay (Get-KvValue $checkpointKv "${familyName}_elapsed_ms")
                $okDisplay = if ($null -eq $family.ok) { "N/A" } else { $family.ok.ToString().ToLower() }
                $exitDisplay = if ($null -eq $family.exit_code) { "N/A" } else { $family.exit_code.ToString() }
                [void]$familyTableRows.Add("| $familyName | $okDisplay | $exitDisplay | $elapsedDisplay |")
            }

            $checkpointMdSection = @(
                ""
                "## MCP v2 Checkpoint Detail"
                ""
                "- Checkpoint summary included: true"
                "- Match confidence: $checkpointSummaryMatchConfidence"
                "- mcp_v2_checkpoint_green: $cpGreen"
                "- checkpoint_family_count: $cpFamilyCount"
                "- kiface_smoke_ok: $cpKifaceSmoke"
                "- checkpoint_summary_path: ``$resolvedCheckpointSummary``"
                ""
                "| Family | OK | Exit code | Elapsed ms |"
                "|---|---:|---:|---:|"
            ) + @($familyTableRows)
        }
        else {
            $checkpointSummaryBlocker = "unreadable_checkpoint_summary"
        }
    }
    else {
        $checkpointSummaryBlocker = "missing_checkpoint_summary"
    }

    if (-not $checkpointSummaryIncluded) {
        $checkpointMdSection = @(
            ""
            "## MCP v2 Checkpoint Detail"
            ""
            "- Checkpoint summary included: false"
            "- Match confidence: $checkpointSummaryMatchConfidence"
            "- checkpoint_summary_blocker: $checkpointSummaryBlocker"
        )
    }
}

$sourceFiles = [ordered]@{
    ci_summary      = $resolvedCiSummary
    advisory_report = $resolvedAdvisory
    release_check   = $resolvedReleaseCheck
}
if ($IncludeCheckpointSummary.IsPresent -and $checkpointSummaryIncluded) {
    $sourceFiles.checkpoint_summary = $resolvedCheckpointSummary
}

$packObject = [ordered]@{
    release_pack_green               = $true
    release_decision                 = $releaseDecision
    release_advisory_check_green     = (Convert-ToJsonScalar $releaseAdvisoryCheckGreen)
    release_requires_manual_review   = (Convert-ToJsonScalar $releaseManualReview)
    ci_baseline_green                = (Convert-ToJsonScalar $ciBaselineGreen)
    mcp_v2_checkpoint_green          = (Convert-ToJsonScalar $mcpCheckpointGreen)
    checkpoint_family_count          = (Convert-ToJsonInt $checkpointFamilyCount)
    advisory_overall_status          = $advisoryOverall
    verify_only                      = (Convert-ToJsonScalar $verifyOnly)
    g4_status                        = $g4Status
    g5_status                        = $g5Status
    g6_status                        = $g6Status
    runtime_footprint_workflow_ok    = (Convert-ToJsonScalar $g4Ok)
    footprint_workflow_discovery_consistency_ok = (Convert-ToJsonScalar $g5Ok)
    pcb_apply_drc_save_workflow_ok   = (Convert-ToJsonScalar $g6Ok)
    exact_blocker_if_any             = $blocker
    export_blocker_if_any            = "N/A"
    source_files                     = $sourceFiles
    pack_generated_at                = $generatedAt
}

if ($IncludeCheckpointSummary.IsPresent) {
    $packObject.checkpoint_summary_included = $checkpointSummaryIncluded
    $packObject.checkpoint_summary_match_confidence = $checkpointSummaryMatchConfidence
    $packObject.checkpoint_summary_path = $resolvedCheckpointSummary
    $packObject.checkpoint_summary_blocker = $checkpointSummaryBlocker
    if ($null -ne $checkpointDetailObject) {
        $packObject.checkpoint = $checkpointDetailObject
    }
}

$jsonPath = Join-Path $outDirectory "qa_mcp_v2_release_pack.json"
$jsonDepth = if ($IncludeCheckpointSummary.IsPresent) { 8 } else { 6 }
$packObject | ConvertTo-Json -Depth $jsonDepth | Set-Content -Path $jsonPath -Encoding UTF8

$ciBaselineChecked = ($ciBaselineGreen -eq "true")
$checkpointChecked = ($mcpCheckpointGreen -eq "true")
$manualReviewClear = ($releaseManualReview -ne "true")

$mdLines = [System.Collections.Generic.List[string]]::new()
foreach ($line in @(
    "# MCP v2 Release Pack"
    ""
    "Generated: $generatedAt"
    ""
    "## Decision"
    "- Release decision: **$releaseDecision**"
    "- Release check green: $releaseAdvisoryCheckGreen"
    "- Manual review required: $releaseManualReview"
    "- Verify only run: $verifyOnly"
    ""
    "## CI Gate"
    "- ci_baseline_green: $ciBaselineGreen"
    "- mcp_v2_checkpoint_green: $mcpCheckpointGreen"
    "- checkpoint_family_count: $checkpointFamilyCount"
    ""
    "## Advisories"
    "| Step | Status | OK | Action |"
    "|---|---|---|---|"
    "| G4 runtime footprint workflow | $g4Status | $g4Ok | $(Get-AdvisoryAction $g4Status) |"
    "| G5 workflow discovery consistency | $g5Status | $g5Ok | $(Get-AdvisoryAction $g5Status) |"
    "| G6 PCB apply DRC save workflow | $g6Status | $g6Ok | $(Get-AdvisoryAction $g6Status) |"
    ""
    "Overall advisory status (R180): $advisoryOverall"
    ""
    "## Release Checklist"
    (Format-ChecklistLine -Checked $ciBaselineChecked -Label "CI baseline green")
    (Format-ChecklistLine -Checked $checkpointChecked -Label "Checkpoint MCP v2 green")
    (Format-ChecklistLine -Checked $true -Label "Advisory report generated")
    (Format-ChecklistLine -Checked $true -Label "Release advisory check generated")
    (Format-ChecklistLine -Checked $manualReviewClear -Label "No manual review required")
    (Format-ChecklistLine -Checked ($releaseDecision -eq "go") -Label "Release decision is go (R181)")
)) {
    [void]$mdLines.Add($line)
}
foreach ($line in $checkpointMdSection) {
    [void]$mdLines.Add($line)
}
foreach ($line in @(
    ""
    "## Source files"
    "- CI summary: ``$resolvedCiSummary``"
    "- Advisory report: ``$resolvedAdvisory``"
    "- Release check: ``$resolvedReleaseCheck``"
)) {
    [void]$mdLines.Add($line)
}
if ($checkpointSummaryIncluded) {
    [void]$mdLines.Add("- Checkpoint summary: ``$resolvedCheckpointSummary``")
}
[void]$mdLines.Add("")
[void]$mdLines.Add("> Release decision follows **R181** (check_mcp_v2_release_advisories.ps1), not R180 overall status alone. VerifyOnly is never go.")

$mdPath = Join-Path $outDirectory "qa_mcp_v2_release_pack.md"
@($mdLines) | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$outDirectory"
Write-Output "RELEASE_PACK_MD=$mdPath"
Write-Output "RELEASE_PACK_JSON=$jsonPath"
Write-Host "release_pack_green=true"
Write-Host "release_decision=$releaseDecision"
Write-Host "release_advisory_check_green=$releaseAdvisoryCheckGreen"
Write-Host "export_blocker_if_any=N/A"
if ($IncludeCheckpointSummary.IsPresent) {
    Write-Host "checkpoint_summary_included=$($checkpointSummaryIncluded.ToString().ToLower())"
    Write-Host "checkpoint_summary_match_confidence=$checkpointSummaryMatchConfidence"
    Write-Host "checkpoint_summary_blocker=$checkpointSummaryBlocker"
}
Write-Host "pack_generated_at=$generatedAt"
exit 0
