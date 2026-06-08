param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-mcp-v2-checkpoint")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$checkpointStartedAt = [DateTimeOffset]::UtcNow.ToString( "o" )
$checkpointStopwatch = [System.Diagnostics.Stopwatch]::StartNew()

$repoRoot = (Resolve-Path (Join-Path $BuildDir "..\..")).Path
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $repoRoot

$kifaceSmokeSkipped = $true
$kifaceSmokeOk = $false

if( $env:KICAD_IPC_KIFACE_SMOKE -eq "1" )
{
    $kifaceSmokeSkipped = $false
    $smokeRes = Test-IpcKifaceDllLoad -BuildDir $BuildDir -ArtifactsDir $runDir

    $detailLines = New-Object System.Collections.Generic.List[string]
    foreach( $row in $smokeRes.Rows )
    {
        [void]$detailLines.Add( "kiface_smoke_$($row.Label)_ok=" + ($(if($row.Ok) { "true" } else { "false" })) )
        [void]$detailLines.Add( "kiface_smoke_$($row.Label)_path=$($row.Path)" )
        if( -not $row.Ok )
        {
            [void]$detailLines.Add( "kiface_smoke_$($row.Label)_last_error=$($row.LastError)" )
            if( $row.Detail )
            {
                [void]$detailLines.Add( "kiface_smoke_$($row.Label)_detail=$($row.Detail)" )
            }
        }
    }
    [void]$detailLines.Add( "kiface_smoke_all_ok=" + ($(if($smokeRes.AllOk) { "true" } else { "false" })) )
    if( $smokeRes.PathEnvDumpPath )
    {
        [void]$detailLines.Add( "kiface_smoke_path_dump=$($smokeRes.PathEnvDumpPath)" )
    }

    $detailLines | Set-Content -Path (Join-Path $runDir "qa_kiface_dll_smoke.txt") -Encoding UTF8

    if( -not $smokeRes.AllOk )
    {
        $checkpointStopwatch.Stop()
        $checkpointElapsedMsEarly = [int64]$checkpointStopwatch.ElapsedMilliseconds
        $checkpointFinishedAtEarly = [DateTimeOffset]::UtcNow.ToString( "o" )

        @(
            "mcp_v2_checkpoint_green=false"
            "sch_geometry_v2_ok=false"
            "sch_authoring_v2_ok=false"
            "sch_wiring_v2_ok=false"
            "sch_symbol_editing_v2_ok=false"
            "pcb_inspection_v2_ok=false"
            "pcb_geometry_v1_ok=false"
            "pcb_clearance_context_v1_ok=false"
            "pcb_nearby_items_context_v1_ok=false"
            "pcb_drc_violations_context_v1_ok=false"
            "exact_blocker_if_any=kiface_dll_loadlibrary_failed"
            "kiface_smoke_skipped=false"
            "kiface_smoke_ok=false"
            "sch_geometry_v2_target_run_dir=skipped_smoke"
            "sch_authoring_v2_target_run_dir=skipped_smoke"
            "sch_wiring_v2_target_run_dir=skipped_smoke"
            "sch_symbol_editing_v2_target_run_dir=skipped_smoke"
            "pcb_inspection_v2_target_run_dir=skipped_smoke"
            "pcb_geometry_v1_target_run_dir=skipped_smoke"
            "pcb_clearance_context_v1_target_run_dir=skipped_smoke"
            "pcb_nearby_items_context_v1_target_run_dir=skipped_smoke"
            "pcb_drc_violations_context_v1_target_run_dir=skipped_smoke"
            "sch_geometry_v2_exit_code=-1"
            "sch_authoring_v2_exit_code=-1"
            "sch_wiring_v2_exit_code=-1"
            "sch_symbol_editing_v2_exit_code=-1"
            "pcb_inspection_v2_exit_code=-1"
            "pcb_geometry_v1_exit_code=-1"
            "pcb_clearance_context_v1_exit_code=-1"
            "pcb_nearby_items_context_v1_exit_code=-1"
            "pcb_drc_violations_context_v1_exit_code=-1"
            "checkpoint_started_at=$checkpointStartedAt"
            "checkpoint_finished_at=$checkpointFinishedAtEarly"
            "checkpoint_elapsed_ms=$checkpointElapsedMsEarly"
            "sch_geometry_v2_elapsed_ms=-1"
            "sch_authoring_v2_elapsed_ms=-1"
            "sch_wiring_v2_elapsed_ms=-1"
            "sch_symbol_editing_v2_elapsed_ms=-1"
            "pcb_inspection_v2_elapsed_ms=-1"
            "pcb_geometry_v1_elapsed_ms=-1"
            "pcb_clearance_context_v1_elapsed_ms=-1"
            "pcb_nearby_items_context_v1_elapsed_ms=-1"
            "pcb_drc_violations_context_v1_elapsed_ms=-1"
            "ipc_smoke_ipc_retry_extra_max_sch_geometry_v2=0"
            "ipc_smoke_ipc_retry_extra_max_sch_authoring_v2=0"
            "ipc_smoke_ipc_retry_extra_max_sch_wiring_v2=0"
            "ipc_smoke_ipc_retry_extra_max_sch_symbol_editing_v2=0"
            "ipc_smoke_ipc_retry_extra_max_pcb_inspection_v2=0"
            "ipc_smoke_ipc_retry_extra_max_pcb_geometry_v1=0"
            "ipc_smoke_ipc_retry_extra_max_pcb_clearance_context_v1=0"
            "ipc_smoke_ipc_retry_extra_max_pcb_nearby_items_context_v1=0"
            "ipc_smoke_ipc_retry_extra_max_pcb_drc_violations_context_v1=0"
            "ipc_smoke_ipc_retry_extra_total_max_across_families=0"
            "checkpoint_family_count=9"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_mcp_v2_checkpoint_summary.txt") -Encoding UTF8

        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    $kifaceSmokeOk = $true
}

function Invoke-FamilyRunner {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RunnerScriptName,
        [Parameter(Mandatory = $true)]
        [string]$SummaryName
    )

    $runnerPath = Join-Path $ScriptDir $RunnerScriptName

    if( !(Test-Path -LiteralPath $runnerPath) )
    {
        return [pscustomobject]@{
            RunnerScript = $RunnerScriptName
            ExitCode = -1
            TargetRunDir = "absent"
            Ok = $false
            ExactBlocker = "runner_script_missing"
            ElapsedMs = 0L
        }
    }

    # R99 — spawn via Invoke-IpcQaChildScript ; green/summary/blockers inchangés.
    $famSw = [System.Diagnostics.Stopwatch]::StartNew()

    $child = Invoke-IpcQaChildScript `
        -ScriptPath $runnerPath `
        -BuildDir $BuildDir `
        -LogDir $runDir `
        -LogBaseName $RunnerScriptName `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $SummaryName

    $famSw.Stop()

    $targetRunDir = $child.TargetRunDir

    $exactBlocker = "N/A"
    $ok = $false

    if( $targetRunDir -eq "absent" )
    {
        $exactBlocker = "runner_target_run_dir_missing"
    }
    else
    {
        $childSummaryPath = $child.SummaryPath
        if( !(Test-Path -LiteralPath $childSummaryPath) )
        {
            $exactBlocker = "summary_missing"
        }
        else
        {
            Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir $SummaryName) -Force
            $summaryContent = Get-Content -LiteralPath $childSummaryPath
            $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1

            if( -not [string]::IsNullOrWhiteSpace( $blockerLine ) )
            {
                $exactBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
            }
            else
            {
                $exactBlocker = "exact_blocker_field_missing"
            }

            if( $exactBlocker -eq "N/A" -and $child.ExitCode -eq 0 )
            {
                $ok = $true
            }
        }
    }

    if( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" )
    {
        $exactBlocker = "runner_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        RunnerScript = $RunnerScriptName
        ExitCode = $child.ExitCode
        TargetRunDir = $targetRunDir
        Ok = $ok
        ExactBlocker = $exactBlocker
        ElapsedMs = [int64]$famSw.ElapsedMilliseconds
    }
}

function Get-MaxIpcRetryFromFamilySummary {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath
    )

    $patterns = @(
        '^ipc_smoke_ipc_retry_extra_total=(\d+)$',
        '^ipc_smoke_ipc_retry_extra_family=(\d+)$',
        '^ipc_smoke_ipc_retry_extra_total_measure=(\d+)$',
        '^ipc_smoke_ipc_retry_extra_total_item_geometry=(\d+)$'
    )

    if( !(Test-Path -LiteralPath $SummaryPath) )
    {
        return 0
    }

    $max = 0
    foreach( $line in Get-Content -LiteralPath $SummaryPath )
    {
        foreach( $pat in $patterns )
        {
            if( $line -match $pat )
            {
                $v = [int]$Matches[1]
                if( $v -gt $max ) { $max = $v }
            }
        }
    }

    return $max
}

function Get-SchAuthoringFamilyAdvisory {
    # R103 — relay advisory fields from copied sch_authoring family summary (non-blocking).
    param(
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath,

        [Parameter(Mandatory = $true)]
        [string]$FamilyKey,

        [string]$DefaultValue = "false"
    )

    if( !(Test-Path -LiteralPath $SummaryPath) )
    {
        return $DefaultValue
    }

    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($FamilyKey + "=*") } | Select-Object -First 1
    if( [string]::IsNullOrWhiteSpace( $line ) )
    {
        return $DefaultValue
    }

    return ( $line -replace ('^' + [regex]::Escape($FamilyKey) + '='), '' ).Trim()
}

$schGeometryV2 = Invoke-FamilyRunner -RunnerScriptName "run_sch_geometry_v2_targeted_probes.ps1" -SummaryName "qa_sch_geometry_v2_family_summary.txt"
$schAuthoringV2 = Invoke-FamilyRunner -RunnerScriptName "run_sch_authoring_v2_targeted_probes.ps1" -SummaryName "qa_sch_authoring_v2_family_summary.txt"
$schWiringV2 = Invoke-FamilyRunner -RunnerScriptName "run_sch_wiring_v2_targeted_probes.ps1" -SummaryName "qa_sch_wiring_v2_family_summary.txt"
$schSymbolEditingV2 = Invoke-FamilyRunner -RunnerScriptName "run_sch_symbol_editing_v2_targeted_probes.ps1" -SummaryName "qa_sch_symbol_editing_v2_family_summary.txt"
$pcbInspectionV2 = Invoke-FamilyRunner -RunnerScriptName "run_pcb_inspection_v2_targeted_probes.ps1" -SummaryName "qa_pcb_inspection_v2_family_summary.txt"
$pcbGeometryV1 = Invoke-FamilyRunner -RunnerScriptName "run_pcb_geometry_v1_targeted_probes.ps1" -SummaryName "qa_pcb_geometry_v1_family_summary.txt"
$pcbClearanceContextV1 = Invoke-FamilyRunner -RunnerScriptName "run_pcb_clearance_context_v1_targeted_probes.ps1" -SummaryName "qa_pcb_clearance_context_v1_family_summary.txt"
$pcbNearbyItemsContextV1 = Invoke-FamilyRunner -RunnerScriptName "run_pcb_nearby_items_context_v1_targeted_probes.ps1" -SummaryName "qa_pcb_nearby_items_context_v1_family_summary.txt"
$pcbDrcViolationsContextV1 = Invoke-FamilyRunner -RunnerScriptName "run_pcb_drc_violations_context_v1_targeted_probes.ps1" -SummaryName "qa_pcb_drc_violations_context_v1_family_summary.txt"

$schGeometryV2Ok = if( $schGeometryV2.Ok ) { "true" } else { "false" }
$schAuthoringV2Ok = if( $schAuthoringV2.Ok ) { "true" } else { "false" }
$schWiringV2Ok = if( $schWiringV2.Ok ) { "true" } else { "false" }
$schSymbolEditingV2Ok = if( $schSymbolEditingV2.Ok ) { "true" } else { "false" }
$pcbInspectionV2Ok = if( $pcbInspectionV2.Ok ) { "true" } else { "false" }
$pcbGeometryV1Ok = if( $pcbGeometryV1.Ok ) { "true" } else { "false" }
$pcbClearanceContextV1Ok = if( $pcbClearanceContextV1.Ok ) { "true" } else { "false" }
$pcbNearbyItemsContextV1Ok = if( $pcbNearbyItemsContextV1.Ok ) { "true" } else { "false" }
$pcbDrcViolationsContextV1Ok = if( $pcbDrcViolationsContextV1.Ok ) { "true" } else { "false" }
$checkpointGreen = ( $schGeometryV2Ok -eq "true" -and $schAuthoringV2Ok -eq "true" -and $schWiringV2Ok -eq "true" -and $schSymbolEditingV2Ok -eq "true" -and $pcbInspectionV2Ok -eq "true" -and $pcbGeometryV1Ok -eq "true" -and $pcbClearanceContextV1Ok -eq "true" -and $pcbNearbyItemsContextV1Ok -eq "true" -and $pcbDrcViolationsContextV1Ok -eq "true" )

# Max ipc_smoke retry counter across copied family summaries (flake telemetry; R33 clearance + R39 nearby + R126 DRC v1).
$ipcRetrySummaryNames = @(
    "qa_sch_geometry_v2_family_summary.txt",
    "qa_sch_authoring_v2_family_summary.txt",
    "qa_sch_wiring_v2_family_summary.txt",
    "qa_sch_symbol_editing_v2_family_summary.txt",
    "qa_pcb_inspection_v2_family_summary.txt",
    "qa_pcb_geometry_v1_family_summary.txt",
    "qa_pcb_clearance_context_v1_family_summary.txt",
    "qa_pcb_nearby_items_context_v1_family_summary.txt",
    "qa_pcb_drc_violations_context_v1_family_summary.txt"
)
$ipcRetryLinePatterns = @(
    '^ipc_smoke_ipc_retry_extra_total=(\d+)$',
    '^ipc_smoke_ipc_retry_extra_family=(\d+)$',
    '^ipc_smoke_ipc_retry_extra_total_measure=(\d+)$',
    '^ipc_smoke_ipc_retry_extra_total_item_geometry=(\d+)$'
)
$ipcRetryMax = 0
foreach( $sn in $ipcRetrySummaryNames )
{
    $sp = Join-Path $runDir $sn
    if( Test-Path -LiteralPath $sp )
    {
        $lines = Get-Content -LiteralPath $sp
        foreach( $line in $lines )
        {
            foreach( $pat in $ipcRetryLinePatterns )
            {
                if( $line -match $pat )
                {
                    $v = [int]$Matches[1]
                    if( $v -gt $ipcRetryMax ) { $ipcRetryMax = $v }
                }
            }
        }
    }
}

$firstBlocker = "N/A"
if( $schGeometryV2.ExitCode -ne 0 ) { $firstBlocker = "sch_geometry_v2:runner_exit_code_" + $schGeometryV2.ExitCode }
elseif( $schGeometryV2.ExactBlocker -ne "N/A" ) { $firstBlocker = "sch_geometry_v2:" + $schGeometryV2.ExactBlocker }
elseif( $schAuthoringV2.ExitCode -ne 0 ) { $firstBlocker = "sch_authoring_v2:runner_exit_code_" + $schAuthoringV2.ExitCode }
elseif( $schAuthoringV2.ExactBlocker -ne "N/A" ) { $firstBlocker = "sch_authoring_v2:" + $schAuthoringV2.ExactBlocker }
elseif( $schWiringV2.ExitCode -ne 0 ) { $firstBlocker = "sch_wiring_v2:runner_exit_code_" + $schWiringV2.ExitCode }
elseif( $schWiringV2.ExactBlocker -ne "N/A" ) { $firstBlocker = "sch_wiring_v2:" + $schWiringV2.ExactBlocker }
elseif( $schSymbolEditingV2.ExitCode -ne 0 ) { $firstBlocker = "sch_symbol_editing_v2:runner_exit_code_" + $schSymbolEditingV2.ExitCode }
elseif( $schSymbolEditingV2.ExactBlocker -ne "N/A" ) { $firstBlocker = "sch_symbol_editing_v2:" + $schSymbolEditingV2.ExactBlocker }
elseif( $pcbInspectionV2.ExitCode -ne 0 ) { $firstBlocker = "pcb_inspection_v2:runner_exit_code_" + $pcbInspectionV2.ExitCode }
elseif( $pcbInspectionV2.ExactBlocker -ne "N/A" ) { $firstBlocker = "pcb_inspection_v2:" + $pcbInspectionV2.ExactBlocker }
elseif( $pcbGeometryV1.ExitCode -ne 0 ) { $firstBlocker = "pcb_geometry_v1:runner_exit_code_" + $pcbGeometryV1.ExitCode }
elseif( $pcbGeometryV1.ExactBlocker -ne "N/A" ) { $firstBlocker = "pcb_geometry_v1:" + $pcbGeometryV1.ExactBlocker }
elseif( $pcbClearanceContextV1.ExitCode -ne 0 ) { $firstBlocker = "pcb_clearance_context_v1:runner_exit_code_" + $pcbClearanceContextV1.ExitCode }
elseif( $pcbClearanceContextV1.ExactBlocker -ne "N/A" ) { $firstBlocker = "pcb_clearance_context_v1:" + $pcbClearanceContextV1.ExactBlocker }
elseif( $pcbNearbyItemsContextV1.ExitCode -ne 0 ) { $firstBlocker = "pcb_nearby_items_context_v1:runner_exit_code_" + $pcbNearbyItemsContextV1.ExitCode }
elseif( $pcbNearbyItemsContextV1.ExactBlocker -ne "N/A" ) { $firstBlocker = "pcb_nearby_items_context_v1:" + $pcbNearbyItemsContextV1.ExactBlocker }
elseif( $pcbDrcViolationsContextV1.ExitCode -ne 0 ) { $firstBlocker = "pcb_drc_violations_context_v1:runner_exit_code_" + $pcbDrcViolationsContextV1.ExitCode }
elseif( $pcbDrcViolationsContextV1.ExactBlocker -ne "N/A" ) { $firstBlocker = "pcb_drc_violations_context_v1:" + $pcbDrcViolationsContextV1.ExactBlocker }

$checkpointStopwatch.Stop()
$checkpointElapsedMs = [int64]$checkpointStopwatch.ElapsedMilliseconds
$checkpointFinishedAt = [DateTimeOffset]::UtcNow.ToString( "o" )

$ipcRetryMaxSchGeometryV2 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_sch_geometry_v2_family_summary.txt")
$ipcRetryMaxSchAuthoringV2 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_sch_authoring_v2_family_summary.txt")
$ipcRetryMaxSchWiringV2 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_sch_wiring_v2_family_summary.txt")
$ipcRetryMaxSchSymbolEditingV2 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_sch_symbol_editing_v2_family_summary.txt")
$ipcRetryMaxPcbInspectionV2 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_pcb_inspection_v2_family_summary.txt")
$ipcRetryMaxPcbGeometryV1 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_pcb_geometry_v1_family_summary.txt")
$ipcRetryMaxPcbClearanceContextV1 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_pcb_clearance_context_v1_family_summary.txt")
$ipcRetryMaxPcbNearbyItemsContextV1 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_pcb_nearby_items_context_v1_family_summary.txt")
$ipcRetryMaxPcbDrcViolationsContextV1 = Get-MaxIpcRetryFromFamilySummary -SummaryPath (Join-Path $runDir "qa_pcb_drc_violations_context_v1_family_summary.txt")

$schAuthoringFamilySummaryPath = Join-Path $runDir "qa_sch_authoring_v2_family_summary.txt"
$schAuthoringPlaceSymbolKicadExitedPostMutationDetected = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_kicad_exited_post_mutation_detected" -DefaultValue "false"
$schAuthoringPlaceSymbolCleanupRestartAttempted = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_cleanup_restart_attempted" -DefaultValue "false"
$schAuthoringPlaceSymbolCleanupRestartOk = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_cleanup_restart_ok" -DefaultValue "false"
$schAuthoringPlaceSymbolMutationRetryExtraTotal = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_mutation_retry_extra_total" -DefaultValue "0"
$schAuthoringPlaceSymbolRestoreRetryExtraTotal = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_restore_retry_extra_total" -DefaultValue "0"
$schAuthoringPlaceSymbolPostMutationPingRetryExtraTotal = Get-SchAuthoringFamilyAdvisory -SummaryPath $schAuthoringFamilySummaryPath -FamilyKey "schematic_place_symbol_post_mutation_ping_retry_extra_total" -DefaultValue "0"

@(
    "mcp_v2_checkpoint_green=" + $(if($checkpointGreen){"true"}else{"false"})
    "sch_geometry_v2_ok=$schGeometryV2Ok"
    "sch_authoring_v2_ok=$schAuthoringV2Ok"
    "sch_wiring_v2_ok=$schWiringV2Ok"
    "sch_symbol_editing_v2_ok=$schSymbolEditingV2Ok"
    "pcb_inspection_v2_ok=$pcbInspectionV2Ok"
    "pcb_geometry_v1_ok=$pcbGeometryV1Ok"
    "pcb_clearance_context_v1_ok=$pcbClearanceContextV1Ok"
    "pcb_nearby_items_context_v1_ok=$pcbNearbyItemsContextV1Ok"
    "pcb_drc_violations_context_v1_ok=$pcbDrcViolationsContextV1Ok"
    "exact_blocker_if_any=$firstBlocker"
    "sch_geometry_v2_target_run_dir=" + $schGeometryV2.TargetRunDir
    "sch_authoring_v2_target_run_dir=" + $schAuthoringV2.TargetRunDir
    "sch_wiring_v2_target_run_dir=" + $schWiringV2.TargetRunDir
    "sch_symbol_editing_v2_target_run_dir=" + $schSymbolEditingV2.TargetRunDir
    "pcb_inspection_v2_target_run_dir=" + $pcbInspectionV2.TargetRunDir
    "pcb_geometry_v1_target_run_dir=" + $pcbGeometryV1.TargetRunDir
    "pcb_clearance_context_v1_target_run_dir=" + $pcbClearanceContextV1.TargetRunDir
    "pcb_nearby_items_context_v1_target_run_dir=" + $pcbNearbyItemsContextV1.TargetRunDir
    "pcb_drc_violations_context_v1_target_run_dir=" + $pcbDrcViolationsContextV1.TargetRunDir
    "sch_geometry_v2_exit_code=" + $schGeometryV2.ExitCode
    "sch_authoring_v2_exit_code=" + $schAuthoringV2.ExitCode
    "sch_wiring_v2_exit_code=" + $schWiringV2.ExitCode
    "sch_symbol_editing_v2_exit_code=" + $schSymbolEditingV2.ExitCode
    "pcb_inspection_v2_exit_code=" + $pcbInspectionV2.ExitCode
    "pcb_geometry_v1_exit_code=" + $pcbGeometryV1.ExitCode
    "pcb_clearance_context_v1_exit_code=" + $pcbClearanceContextV1.ExitCode
    "pcb_nearby_items_context_v1_exit_code=" + $pcbNearbyItemsContextV1.ExitCode
    "pcb_drc_violations_context_v1_exit_code=" + $pcbDrcViolationsContextV1.ExitCode
    "ipc_smoke_ipc_retry_extra_total_max_across_families=$ipcRetryMax"
    "ipc_smoke_ipc_retry_extra_max_sch_geometry_v2=$ipcRetryMaxSchGeometryV2"
    "ipc_smoke_ipc_retry_extra_max_sch_authoring_v2=$ipcRetryMaxSchAuthoringV2"
    "ipc_smoke_ipc_retry_extra_max_sch_wiring_v2=$ipcRetryMaxSchWiringV2"
    "ipc_smoke_ipc_retry_extra_max_sch_symbol_editing_v2=$ipcRetryMaxSchSymbolEditingV2"
    "ipc_smoke_ipc_retry_extra_max_pcb_inspection_v2=$ipcRetryMaxPcbInspectionV2"
    "ipc_smoke_ipc_retry_extra_max_pcb_geometry_v1=$ipcRetryMaxPcbGeometryV1"
    "ipc_smoke_ipc_retry_extra_max_pcb_clearance_context_v1=$ipcRetryMaxPcbClearanceContextV1"
    "ipc_smoke_ipc_retry_extra_max_pcb_nearby_items_context_v1=$ipcRetryMaxPcbNearbyItemsContextV1"
    "ipc_smoke_ipc_retry_extra_max_pcb_drc_violations_context_v1=$ipcRetryMaxPcbDrcViolationsContextV1"
    "sch_authoring_place_symbol_kicad_exited_post_mutation_detected=$schAuthoringPlaceSymbolKicadExitedPostMutationDetected"
    "sch_authoring_place_symbol_cleanup_restart_attempted=$schAuthoringPlaceSymbolCleanupRestartAttempted"
    "sch_authoring_place_symbol_cleanup_restart_ok=$schAuthoringPlaceSymbolCleanupRestartOk"
    "sch_authoring_place_symbol_mutation_retry_extra_total=$schAuthoringPlaceSymbolMutationRetryExtraTotal"
    "sch_authoring_place_symbol_restore_retry_extra_total=$schAuthoringPlaceSymbolRestoreRetryExtraTotal"
    "sch_authoring_place_symbol_post_mutation_ping_retry_extra_total=$schAuthoringPlaceSymbolPostMutationPingRetryExtraTotal"
    "family_summary_files_in_checkpoint_dir=" + ($ipcRetrySummaryNames -join ";")
    "checkpoint_family_count=9"
    "checkpoint_started_at=$checkpointStartedAt"
    "checkpoint_finished_at=$checkpointFinishedAt"
    "checkpoint_elapsed_ms=$checkpointElapsedMs"
    "sch_geometry_v2_elapsed_ms=" + $schGeometryV2.ElapsedMs
    "sch_authoring_v2_elapsed_ms=" + $schAuthoringV2.ElapsedMs
    "sch_wiring_v2_elapsed_ms=" + $schWiringV2.ElapsedMs
    "sch_symbol_editing_v2_elapsed_ms=" + $schSymbolEditingV2.ElapsedMs
    "pcb_inspection_v2_elapsed_ms=" + $pcbInspectionV2.ElapsedMs
    "pcb_geometry_v1_elapsed_ms=" + $pcbGeometryV1.ElapsedMs
    "pcb_clearance_context_v1_elapsed_ms=" + $pcbClearanceContextV1.ElapsedMs
    "pcb_nearby_items_context_v1_elapsed_ms=" + $pcbNearbyItemsContextV1.ElapsedMs
    "pcb_drc_violations_context_v1_elapsed_ms=" + $pcbDrcViolationsContextV1.ElapsedMs
    "kiface_smoke_skipped=" + $(if($kifaceSmokeSkipped) { "true" } else { "false" })
    "kiface_smoke_ok=" + $(if($kifaceSmokeSkipped) { "N/A" } elseif($kifaceSmokeOk) { "true" } else { "false" })
    ""
) | Set-Content -Path (Join-Path $runDir "qa_mcp_v2_checkpoint_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
