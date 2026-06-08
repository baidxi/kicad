param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-authoring-v2-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Invoke-TargetedProbe {
    # R95 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/summary inchangés.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProbeScriptName,
        [Parameter(Mandatory = $true)]
        [string]$SummaryName
    )

    $probePath = Join-Path $ScriptDir $ProbeScriptName

    if( !(Test-Path -LiteralPath $probePath) )
    {
        return [pscustomobject]@{
            ProbeScript = $ProbeScriptName
            ExitCode = -1
            TargetRunDir = "absent"
            Ok = $false
            ExactBlocker = "probe_script_missing"
        }
    }

    $child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName $ProbeScriptName `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $SummaryName

    $targetRunDir = $child.TargetRunDir

    $exactBlocker = "N/A"
    $ok = $false

    if( $targetRunDir -eq "absent" )
    {
        $exactBlocker = "probe_target_run_dir_missing"
    }
    else
    {
        $childSummaryPath = Join-Path $targetRunDir $SummaryName
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
        $exactBlocker = "probe_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        ProbeScript = $ProbeScriptName
        ExitCode = $child.ExitCode
        TargetRunDir = $targetRunDir
        Ok = $ok
        ExactBlocker = $exactBlocker
    }
}

function Get-PlaceSymbolProbeAdvisory {
    # R103 — relay advisory fields from place_symbol probe summary (non-blocking).
    param(
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath,

        [Parameter(Mandatory = $true)]
        [string]$SourceKey,

        [string]$DefaultValue = "false"
    )

    if( [string]::IsNullOrWhiteSpace( $SummaryPath ) -or !(Test-Path -LiteralPath $SummaryPath) )
    {
        return $DefaultValue
    }

    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($SourceKey + "=*") } | Select-Object -First 1
    if( [string]::IsNullOrWhiteSpace( $line ) )
    {
        return $DefaultValue
    }

    return ( $line -replace ('^' + [regex]::Escape($SourceKey) + '='), '' ).Trim()
}

$createLabel = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_create_label_probe.ps1" -SummaryName "qa_schematic_create_label_session_contract_summary.txt"
$placeSymbol = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_place_symbol_probe.ps1" -SummaryName "qa_schematic_place_symbol_session_contract_summary.txt"

$schematicCreateLabelOk = if( $createLabel.Ok ) { "true" } else { "false" }
$schematicPlaceSymbolOk = if( $placeSymbol.Ok ) { "true" } else { "false" }
$familyGreen = ( $schematicCreateLabelOk -eq "true" -and $schematicPlaceSymbolOk -eq "true" )

$placeSymbolSummaryPath = Join-Path $runDir "qa_schematic_place_symbol_session_contract_summary.txt"
$schematicPlaceSymbolKicadAliveAtPostMutation = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "kicad_alive_at_post_mutation" -DefaultValue "false"
$schematicPlaceSymbolKicadExitedPostMutationDetected = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "kicad_exited_post_mutation_detected" -DefaultValue "false"
$schematicPlaceSymbolCleanupRestartAttempted = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "post_mutation_cleanup_restart_attempted" -DefaultValue "false"
$schematicPlaceSymbolCleanupRestartOk = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "post_mutation_cleanup_restart_ok" -DefaultValue "false"
$schematicPlaceSymbolCreatedItemFoundAfterRestart = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "post_mutation_created_item_found_after_restart" -DefaultValue "false"
$schematicPlaceSymbolCleanupAfterRestartOk = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "post_mutation_cleanup_after_restart_ok" -DefaultValue "false"
$schematicPlaceSymbolMutationRetryExtraTotal = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "place_symbol_mutation_retry_extra_total" -DefaultValue "0"
$schematicPlaceSymbolRestoreRetryExtraTotal = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "place_symbol_restore_retry_extra_total" -DefaultValue "0"
$schematicPlaceSymbolPostMutationPingRetryExtraTotal = Get-PlaceSymbolProbeAdvisory -SummaryPath $placeSymbolSummaryPath -SourceKey "post_mutation_ping_retry_extra_total" -DefaultValue "0"

$firstBlocker = "N/A"
if( $createLabel.ExitCode -ne 0 ) { $firstBlocker = "schematic_create_label:probe_exit_code_" + $createLabel.ExitCode }
elseif( $createLabel.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_create_label:" + $createLabel.ExactBlocker }
elseif( $placeSymbol.ExitCode -ne 0 ) { $firstBlocker = "schematic_place_symbol:probe_exit_code_" + $placeSymbol.ExitCode }
elseif( $placeSymbol.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_place_symbol:" + $placeSymbol.ExactBlocker }

@(
    "sch_authoring_v2_family_green=" + $(if($familyGreen){"true"}else{"false"})
    "schematic_create_label_ok=$schematicCreateLabelOk"
    "schematic_place_symbol_ok=$schematicPlaceSymbolOk"
    "exact_blocker_if_any=$firstBlocker"
    "schematic_create_label_target_run_dir=" + $createLabel.TargetRunDir
    "schematic_place_symbol_target_run_dir=" + $placeSymbol.TargetRunDir
    "schematic_create_label_exit_code=" + $createLabel.ExitCode
    "schematic_place_symbol_exit_code=" + $placeSymbol.ExitCode
    "schematic_place_symbol_kicad_alive_at_post_mutation=$schematicPlaceSymbolKicadAliveAtPostMutation"
    "schematic_place_symbol_kicad_exited_post_mutation_detected=$schematicPlaceSymbolKicadExitedPostMutationDetected"
    "schematic_place_symbol_cleanup_restart_attempted=$schematicPlaceSymbolCleanupRestartAttempted"
    "schematic_place_symbol_cleanup_restart_ok=$schematicPlaceSymbolCleanupRestartOk"
    "schematic_place_symbol_created_item_found_after_restart=$schematicPlaceSymbolCreatedItemFoundAfterRestart"
    "schematic_place_symbol_cleanup_after_restart_ok=$schematicPlaceSymbolCleanupAfterRestartOk"
    "schematic_place_symbol_mutation_retry_extra_total=$schematicPlaceSymbolMutationRetryExtraTotal"
    "schematic_place_symbol_restore_retry_extra_total=$schematicPlaceSymbolRestoreRetryExtraTotal"
    "schematic_place_symbol_post_mutation_ping_retry_extra_total=$schematicPlaceSymbolPostMutationPingRetryExtraTotal"
    ""
) | Set-Content -Path (Join-Path $runDir "qa_sch_authoring_v2_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
