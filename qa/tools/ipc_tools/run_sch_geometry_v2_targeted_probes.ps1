param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-geometry-v2-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Invoke-TargetedProbe {
    # R94 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/summary inchangés.
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

$moveSymbol = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_move_symbol_probe.ps1" -SummaryName "qa_schematic_move_symbol_session_contract_summary.txt"
$moveLabel = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_move_label_probe.ps1" -SummaryName "qa_schematic_move_label_session_contract_summary.txt"
$createWire = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_create_wire_segment_probe.ps1" -SummaryName "qa_schematic_create_wire_segment_session_contract_summary.txt"

$schematicMoveSymbolOk = if( $moveSymbol.Ok ) { "true" } else { "false" }
$schematicMoveLabelOk = if( $moveLabel.Ok ) { "true" } else { "false" }
$schematicCreateWireSegmentOk = if( $createWire.Ok ) { "true" } else { "false" }
$familyGreen = ( $schematicMoveSymbolOk -eq "true" -and $schematicMoveLabelOk -eq "true" -and $schematicCreateWireSegmentOk -eq "true" )

$firstBlocker = "N/A"
if( $moveSymbol.ExitCode -ne 0 ) { $firstBlocker = "schematic_move_symbol:probe_exit_code_" + $moveSymbol.ExitCode }
elseif( $moveSymbol.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_move_symbol:" + $moveSymbol.ExactBlocker }
elseif( $moveLabel.ExitCode -ne 0 ) { $firstBlocker = "schematic_move_label:probe_exit_code_" + $moveLabel.ExitCode }
elseif( $moveLabel.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_move_label:" + $moveLabel.ExactBlocker }
elseif( $createWire.ExitCode -ne 0 ) { $firstBlocker = "schematic_create_wire_segment:probe_exit_code_" + $createWire.ExitCode }
elseif( $createWire.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_create_wire_segment:" + $createWire.ExactBlocker }

@(
    "sch_geometry_v2_family_green=" + $(if($familyGreen){"true"}else{"false"})
    "schematic_move_symbol_ok=$schematicMoveSymbolOk"
    "schematic_move_label_ok=$schematicMoveLabelOk"
    "schematic_create_wire_segment_ok=$schematicCreateWireSegmentOk"
    "exact_blocker_if_any=$firstBlocker"
    "schematic_move_symbol_target_run_dir=" + $moveSymbol.TargetRunDir
    "schematic_move_label_target_run_dir=" + $moveLabel.TargetRunDir
    "schematic_create_wire_segment_target_run_dir=" + $createWire.TargetRunDir
    "schematic_move_symbol_exit_code=" + $moveSymbol.ExitCode
    "schematic_move_label_exit_code=" + $moveLabel.ExitCode
    "schematic_create_wire_segment_exit_code=" + $createWire.ExitCode
    ""
) | Set-Content -Path (Join-Path $runDir "qa_sch_geometry_v2_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
