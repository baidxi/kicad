param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [string]$CheckpointRunDir,

    [string]$ArtifactRoot,

    [int]$ScanLastNCheckpoints = 0
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$commonModule = Join-Path $ScriptDir "ipc_qa_common.ps1"
if( !(Test-Path -LiteralPath $commonModule) )
{
    Write-Error "Missing common IPC QA module at $commonModule"
    exit 1
}
. $commonModule

function Get-IntOrZero {
    param(
        [string]$Value
    )

    if( [string]::IsNullOrWhiteSpace( $Value ) )
    {
        return 0
    }

    if( $Value -match '^\d+$' )
    {
        return [int]$Value
    }

    return 0
}

function Test-IsTrue {
    param(
        [string]$Value
    )

    return ( $Value -eq "true" )
}

function Get-CheckpointAdvisorySnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CheckpointRunDirPath,

        [switch]$AllowLegacyMissingAdvisory
    )

    $summaryPath = Join-Path $CheckpointRunDirPath "qa_mcp_v2_checkpoint_summary.txt"
    if( !(Test-Path -LiteralPath $summaryPath) )
    {
        throw "Missing checkpoint summary: $summaryPath"
    }

    $advisoryDefault = if( $AllowLegacyMissingAdvisory ) { "false" } else { "absent" }
    $retryDefault = if( $AllowLegacyMissingAdvisory ) { "0" } else { "absent" }

    $fields = @{
        CheckpointRunDir = $CheckpointRunDirPath
        SummaryPath = $summaryPath
        McpV2CheckpointGreen = Read-ContractValue -SummaryPath $summaryPath -Key "mcp_v2_checkpoint_green" -DefaultValue "absent"
        ExactBlocker = Read-ContractValue -SummaryPath $summaryPath -Key "exact_blocker_if_any" -DefaultValue "absent"
        KicadExited = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_kicad_exited_post_mutation_detected" -DefaultValue $advisoryDefault
        CleanupRestartAttempted = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_cleanup_restart_attempted" -DefaultValue $advisoryDefault
        CleanupRestartOk = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_cleanup_restart_ok" -DefaultValue $advisoryDefault
        MutationRetry = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_mutation_retry_extra_total" -DefaultValue $retryDefault
        RestoreRetry = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_restore_retry_extra_total" -DefaultValue $retryDefault
        PingRetry = Read-ContractValue -SummaryPath $summaryPath -Key "sch_authoring_place_symbol_post_mutation_ping_retry_extra_total" -DefaultValue $retryDefault
    }

    $required = @(
        $fields.McpV2CheckpointGreen
    )
    if( -not $AllowLegacyMissingAdvisory )
    {
        $required += @(
            $fields.KicadExited
            $fields.CleanupRestartAttempted
            $fields.CleanupRestartOk
            $fields.MutationRetry
            $fields.RestoreRetry
            $fields.PingRetry
        )
    }
    foreach( $fieldValue in $required )
    {
        if( $fieldValue -eq "absent" )
        {
            throw "Unable to parse required advisory field from $summaryPath (missing or unreadable key)"
        }
    }

    $authoringSummaryPath = Join-Path $CheckpointRunDirPath "qa_sch_authoring_v2_family_summary.txt"
    $fields.AuthoringSummaryPath = if( Test-Path -LiteralPath $authoringSummaryPath ) { $authoringSummaryPath } else { "N/A" }
    $fields.FamilyExactBlocker = "N/A"
    if( Test-Path -LiteralPath $authoringSummaryPath )
    {
        $fields.FamilyExactBlocker = Read-ContractValue -SummaryPath $authoringSummaryPath -Key "exact_blocker_if_any" -DefaultValue "N/A"
    }

    $fields.MutationRetryInt = Get-IntOrZero -Value $fields.MutationRetry
    $fields.RestoreRetryInt = Get-IntOrZero -Value $fields.RestoreRetry
    $fields.PingRetryInt = Get-IntOrZero -Value $fields.PingRetry
    $fields.AdvisoryDetected = (
        ( Test-IsTrue -Value $fields.KicadExited ) -or
        ( Test-IsTrue -Value $fields.CleanupRestartAttempted ) -or
        ( $fields.MutationRetryInt -gt 0 ) -or
        ( $fields.RestoreRetryInt -gt 0 ) -or
        ( $fields.PingRetryInt -gt 0 )
    )

    return [pscustomobject]$fields
}

function New-AdvisoryWarningBundle {
    param(
        [Parameter(Mandatory = $true)]
        $Snapshot
    )

    $warningMessages = New-Object System.Collections.Generic.List[string]
    $recommendedActions = New-Object System.Collections.Generic.List[string]

    if( Test-IsTrue -Value $Snapshot.KicadExited )
    {
        [void]$warningMessages.Add( "KiCad exit post-mutation detected (advisory-only): conserve les artefacts probe place_symbol ; ouvrir une investigation crash si repetition (>=2/10 runs place_symbol ou >=1 checkpoint CI/jour avec advisory true)." )
        [void]$recommendedActions.Add( "keep_artifacts_monitor_frequency" )
    }

    if( Test-IsTrue -Value $Snapshot.CleanupRestartAttempted )
    {
        [void]$warningMessages.Add( "Post-mutation cleanup restart attempted: crash/exit recupere par le harness R102." )
        [void]$recommendedActions.Add( "post_mutation_crash_recovered_by_harness" )
    }

    if( ( $Snapshot.MutationRetryInt -gt 0 ) -or ( $Snapshot.RestoreRetryInt -gt 0 ) -or ( $Snapshot.PingRetryInt -gt 0 ) )
    {
        [void]$warningMessages.Add( "Retries IPC post-mutation > 0 (mutation=$($Snapshot.MutationRetryInt) restore=$($Snapshot.RestoreRetryInt) ping=$($Snapshot.PingRetryInt)): instabilite IPC recuperee par le harness R101." )
        [void]$recommendedActions.Add( "ipc_instability_recovered" )
    }

    $blockerForIncident = @($Snapshot.ExactBlocker, $Snapshot.FamilyExactBlocker) -join " "
    if( ( Test-IsTrue -Value $Snapshot.KicadExited ) -and
        ( $blockerForIncident -match 'restore_connection_refused|connection_refused' ) )
    {
        [void]$warningMessages.Add( "Incident QA R104: advisory kicad_exited associe a restore_connection_refused ou connection_refused - traiter comme incident (pas seulement observabilite)." )
        [void]$recommendedActions.Add( "qa_incident_investigate_crash_and_ipc" )
    }

    $recommendedAction = if( $recommendedActions.Count -eq 0 ) { "none_advisory_quiet" } else { ( $recommendedActions | Select-Object -Unique ) -join ";" }

    return [pscustomobject]@{
        WarningMessages = $warningMessages
        RecommendedAction = $recommendedAction
        AdvisoryWarningEmitted = ( $warningMessages.Count -gt 0 )
    }
}

function Get-McpV2CheckpointArtifactDirs {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArtifactsRoot,

        [int]$Take = 0
    )

    $dirs = @( Get-ChildItem -LiteralPath $ArtifactsRoot -Directory -Filter "*-mcp-v2-checkpoint" |
        Where-Object { $_.Name -match '^\d{8}-\d+-mcp-v2-checkpoint$' } |
        Sort-Object Name -Descending )

    if( $Take -gt 0 )
    {
        return ,@( $dirs | Select-Object -First $Take )
    }

    return ,$dirs
}

function Write-AdvisoryReportSummary {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$SummaryLines,

        [Parameter(Mandatory = $true)]
        [string]$ReportSummaryPath
    )

    $filtered = @($SummaryLines | Where-Object { -not [string]::IsNullOrWhiteSpace( $_ ) })
    $filtered | Set-Content -Path $ReportSummaryPath -Encoding UTF8
}

function Emit-AdvisoryWarnings {
    param(
        $WarningMessages
    )

    if( -not $WarningMessages -or $WarningMessages.Count -eq 0 )
    {
        Write-Output "advisory_warnings=(none)"
        return
    }

    foreach( $msg in $WarningMessages )
    {
        Write-Warning $msg
    }
}

try
{
    $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
}
catch
{
    Write-Error "Invalid BuildDir: $BuildDir"
    exit 1
}

if( $ScanLastNCheckpoints -gt 0 -and -not [string]::IsNullOrWhiteSpace( $CheckpointRunDir ) )
{
    Write-Error "Cannot combine -ScanLastNCheckpoints with -CheckpointRunDir"
    exit 1
}

if( $ScanLastNCheckpoints -lt 0 )
{
    Write-Error "ScanLastNCheckpoints must be >= 0"
    exit 1
}

$artifactRootPath = if( [string]::IsNullOrWhiteSpace( $ArtifactRoot ) )
{
    Join-Path $resolvedBuildDir "ipc-v1-artifacts"
}
else
{
    if( !(Test-Path -LiteralPath $ArtifactRoot) )
    {
        Write-Error "ArtifactRoot not found: $ArtifactRoot"
        exit 1
    }
    (Resolve-Path -LiteralPath $ArtifactRoot).Path
}

if( !(Test-Path -LiteralPath $artifactRootPath) )
{
    Write-Error "No ipc-v1-artifacts directory at $artifactRootPath"
    exit 1
}

$reportRunDir = Join-Path $artifactRootPath ( (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-authoring-advisory-report" )
New-Item -ItemType Directory -Force -Path $reportRunDir | Out-Null
$reportSummaryPath = Join-Path $reportRunDir "qa_sch_authoring_advisory_report_summary.txt"

if( $ScanLastNCheckpoints -gt 0 )
{
    $checkpointDirs = Get-McpV2CheckpointArtifactDirs -ArtifactsRoot $artifactRootPath -Take $ScanLastNCheckpoints

    if( $checkpointDirs.Count -eq 0 )
    {
        Write-Error "No *-mcp-v2-checkpoint directory under $artifactRootPath"
        exit 1
    }

    $snapshots = New-Object System.Collections.Generic.List[object]
    foreach( $dir in $checkpointDirs )
    {
        try
        {
            [void]$snapshots.Add( (Get-CheckpointAdvisorySnapshot -CheckpointRunDirPath $dir.FullName -AllowLegacyMissingAdvisory) )
        }
        catch
        {
            Write-Error $_.Exception.Message
            exit 1
        }
    }

    $kicadExitedCount = 0
    $cleanupRestartAttemptedCount = 0
    $cleanupRestartOkCount = 0
    $mutationRetrySum = 0
    $restoreRetrySum = 0
    $pingRetrySum = 0
    $checkpointsWithAdvisory = 0
    $kicadExitedTodayCount = 0
    $todayUtc = [DateTime]::UtcNow.ToString( "yyyy-MM-dd" )

    $allWarnings = New-Object System.Collections.Generic.List[string]
    $allRecommended = New-Object System.Collections.Generic.List[string]

    foreach( $snap in $snapshots )
    {
        if( Test-IsTrue -Value $snap.KicadExited ) { $kicadExitedCount++ }
        if( Test-IsTrue -Value $snap.CleanupRestartAttempted ) { $cleanupRestartAttemptedCount++ }
        if( Test-IsTrue -Value $snap.CleanupRestartOk ) { $cleanupRestartOkCount++ }
        $mutationRetrySum += $snap.MutationRetryInt
        $restoreRetrySum += $snap.RestoreRetryInt
        $pingRetrySum += $snap.PingRetryInt
        if( $snap.AdvisoryDetected ) { $checkpointsWithAdvisory++ }

        if( Test-IsTrue -Value $snap.KicadExited )
        {
            $dirName = Split-Path -Leaf $snap.CheckpointRunDir
            if( $dirName -match '^(\d{8})-' )
            {
                $stamp = $Matches[1]
                $checkpointDate = "{0}-{1}-{2}" -f $stamp.Substring( 0, 4 ), $stamp.Substring( 4, 2 ), $stamp.Substring( 6, 2 )
                if( $checkpointDate -eq $todayUtc )
                {
                    $kicadExitedTodayCount++
                }
            }
        }

        $bundle = New-AdvisoryWarningBundle -Snapshot $snap
        foreach( $msg in $bundle.WarningMessages )
        {
            if( -not $allWarnings.Contains( $msg ) )
            {
                [void]$allWarnings.Add( $msg )
            }
        }
        foreach( $action in ( $bundle.RecommendedAction -split ';' ) )
        {
            if( -not [string]::IsNullOrWhiteSpace( $action ) -and $action -ne "none_advisory_quiet" -and -not $allRecommended.Contains( $action ) )
            {
                [void]$allRecommended.Add( $action )
            }
        }
    }

    if( $kicadExitedCount -ge 2 )
    {
        [void]$allWarnings.Add( "Seuil R104 frequence: kicad_exited_post_mutation_detected_count=$kicadExitedCount sur $($snapshots.Count) checkpoints scannes (>=2) - ouvrir investigation crash root-cause." )
        if( -not $allRecommended.Contains( "investigate_crash_frequency_r104" ) )
        {
            [void]$allRecommended.Add( "investigate_crash_frequency_r104" )
        }
    }

    if( $kicadExitedTodayCount -ge 1 )
    {
        [void]$allWarnings.Add( "Seuil R104 CI quotidien: kicad_exited_post_mutation_detected_count=$kicadExitedTodayCount sur checkpoints du jour UTC ($todayUtc) - traiter comme signal d'investigation advisory." )
        if( -not $allRecommended.Contains( "investigate_crash_daily_ci_r104" ) )
        {
            [void]$allRecommended.Add( "investigate_crash_daily_ci_r104" )
        }
    }

    $recommendedAction = if( $allRecommended.Count -eq 0 ) { "none_advisory_quiet" } else { ( $allRecommended | Select-Object -Unique ) -join ";" }
    $advisoryWarningEmitted = ( $allWarnings.Count -gt 0 )
    $sourceCheckpointRunDirs = ( $snapshots | ForEach-Object { $_.CheckpointRunDir } ) -join ";"

    Write-Output "=== SCH Authoring Advisory Report (R105/R107 scan) ==="
    Write-Output "scan_mode=last_n_checkpoints"
    Write-Output ("scan_last_n_checkpoints=" + $ScanLastNCheckpoints)
    Write-Output ("checkpoints_scanned=" + $snapshots.Count)
    Write-Output ("checkpoints_with_advisory=" + $checkpointsWithAdvisory)
    Write-Output ("kicad_exited_post_mutation_detected_count=" + $kicadExitedCount)
    Write-Output ("cleanup_restart_attempted_count=" + $cleanupRestartAttemptedCount)
    Write-Output ("cleanup_restart_ok_count=" + $cleanupRestartOkCount)
    Write-Output ("mutation_retry_extra_total_sum=" + $mutationRetrySum)
    Write-Output ("restore_retry_extra_total_sum=" + $restoreRetrySum)
    Write-Output ("post_mutation_ping_retry_extra_total_sum=" + $pingRetrySum)
    Write-Output ("advisory_warning_emitted=" + $(if($advisoryWarningEmitted) { "true" } else { "false" }))
    Write-Output ("recommended_action=" + $recommendedAction)
    Write-Output ("source_checkpoint_run_dirs=" + $sourceCheckpointRunDirs)
    Emit-AdvisoryWarnings -WarningMessages $allWarnings

    Write-AdvisoryReportSummary -ReportSummaryPath $reportSummaryPath -SummaryLines @(
        "sch_authoring_advisory_report_green=true"
        "exact_blocker_if_any=N/A"
        "scan_mode=last_n_checkpoints"
        "scan_last_n_checkpoints=$ScanLastNCheckpoints"
        "checkpoints_scanned=$($snapshots.Count)"
        "checkpoints_with_advisory=$checkpointsWithAdvisory"
        "kicad_exited_post_mutation_detected_count=$kicadExitedCount"
        "cleanup_restart_attempted_count=$cleanupRestartAttemptedCount"
        "cleanup_restart_ok_count=$cleanupRestartOkCount"
        "mutation_retry_extra_total_sum=$mutationRetrySum"
        "restore_retry_extra_total_sum=$restoreRetrySum"
        "post_mutation_ping_retry_extra_total_sum=$pingRetrySum"
        "advisory_warning_emitted=" + $(if($advisoryWarningEmitted) { "true" } else { "false" })
        "recommended_action=$recommendedAction"
        "source_checkpoint_run_dirs=$sourceCheckpointRunDirs"
    )
}
else
{
    if( [string]::IsNullOrWhiteSpace( $CheckpointRunDir ) )
    {
        $latestCheckpoint = Get-McpV2CheckpointArtifactDirs -ArtifactsRoot $artifactRootPath -Take 1 | Select-Object -First 1

        if( -not $latestCheckpoint )
        {
            Write-Error "No *-mcp-v2-checkpoint directory under $artifactRootPath"
            exit 1
        }

        $sourceCheckpointRunDir = $latestCheckpoint.FullName
    }
    else
    {
        if( !(Test-Path -LiteralPath $CheckpointRunDir) )
        {
            Write-Error "CheckpointRunDir not found: $CheckpointRunDir"
            exit 1
        }
        $sourceCheckpointRunDir = (Resolve-Path -LiteralPath $CheckpointRunDir).Path
    }

    try
    {
        $snap = Get-CheckpointAdvisorySnapshot -CheckpointRunDirPath $sourceCheckpointRunDir
    }
    catch
    {
        Write-Error $_.Exception.Message
        exit 1
    }

    $bundle = New-AdvisoryWarningBundle -Snapshot $snap
    $cleanupRestartOkSummary = if( Test-IsTrue -Value $snap.CleanupRestartAttempted ) { $snap.CleanupRestartOk } else { "N/A" }

    Write-Output "=== SCH Authoring Advisory Report (R105) ==="
    Write-Output ("source_checkpoint_run_dir=" + $snap.CheckpointRunDir)
    Write-Output ("source_checkpoint_summary_path=" + $snap.SummaryPath)
    Write-Output ("source_authoring_summary_path=" + $snap.AuthoringSummaryPath)
    Write-Output ("mcp_v2_checkpoint_green=" + $snap.McpV2CheckpointGreen)
    Write-Output ("checkpoint_exact_blocker_if_any=" + $snap.ExactBlocker)
    Write-Output ("advisory_detected=" + $(if($snap.AdvisoryDetected) { "yes" } else { "no" }))
    Write-Output ("kicad_exited_post_mutation_detected=" + $snap.KicadExited)
    Write-Output ("cleanup_restart_attempted=" + $snap.CleanupRestartAttempted)
    Write-Output ("cleanup_restart_ok=" + $cleanupRestartOkSummary)
    Write-Output ("mutation_retry_extra_total=" + $snap.MutationRetry)
    Write-Output ("restore_retry_extra_total=" + $snap.RestoreRetry)
    Write-Output ("post_mutation_ping_retry_extra_total=" + $snap.PingRetry)
    Write-Output ("advisory_warning_emitted=" + $(if($bundle.AdvisoryWarningEmitted) { "true" } else { "false" }))
    Write-Output ("recommended_action=" + $bundle.RecommendedAction)
    Emit-AdvisoryWarnings -WarningMessages $bundle.WarningMessages

    Write-AdvisoryReportSummary -ReportSummaryPath $reportSummaryPath -SummaryLines @(
        "sch_authoring_advisory_report_green=true"
        "exact_blocker_if_any=N/A"
        "source_checkpoint_run_dir=$($snap.CheckpointRunDir)"
        "source_checkpoint_summary_path=$($snap.SummaryPath)"
        "source_authoring_summary_path=$($snap.AuthoringSummaryPath)"
        "advisory_warning_emitted=" + $(if($bundle.AdvisoryWarningEmitted) { "true" } else { "false" })
        "kicad_exited_post_mutation_detected=$($snap.KicadExited)"
        "cleanup_restart_attempted=$($snap.CleanupRestartAttempted)"
        "cleanup_restart_ok=$cleanupRestartOkSummary"
        "mutation_retry_extra_total=$($snap.MutationRetry)"
        "restore_retry_extra_total=$($snap.RestoreRetry)"
        "post_mutation_ping_retry_extra_total=$($snap.PingRetry)"
        "mcp_v2_checkpoint_green=$($snap.McpV2CheckpointGreen)"
        "checkpoint_exact_blocker_if_any=$($snap.ExactBlocker)"
        "recommended_action=$($bundle.RecommendedAction)"
    )
}

Write-Output ("report_summary_path=" + $reportSummaryPath)
Write-Output ("TARGET_RUN_DIR=" + $reportRunDir)
exit 0
