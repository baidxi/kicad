param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-wiring-v2-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Invoke-TargetedProbe {
    # R96 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/summary inchangés.
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
            ProbeScript = $probeScriptName
            ExitCode = -1
            TargetRunDir = "absent"
            Ok = $false
            ExactBlocker = "probe_script_missing"
        }
    }

    $child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
        -ExtraArgs @('-MinimalPayload') `
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
        $childSummaryPath = Join-Path $targetRunDir $summaryName
        if( !(Test-Path -LiteralPath $childSummaryPath) )
        {
            $exactBlocker = "summary_missing"
        }
        else
        {
            Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir $summaryName) -Force
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

$connectSymbolPin = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_connect_symbol_pin_probe.ps1" -SummaryName "qa_schematic_connect_symbol_pin_session_contract_summary.txt"
$connectPinsLShape = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_connect_pins_l_shape_probe.ps1" -SummaryName "qa_schematic_connect_pins_l_shape_session_contract_summary.txt"
$schematicConnectSymbolPinOk = if( $connectSymbolPin.Ok ) { "true" } else { "false" }
$schematicConnectPinsLShapeOk = if( $connectPinsLShape.Ok ) { "true" } else { "false" }
$familyGreen = ( $schematicConnectSymbolPinOk -eq "true" -and $schematicConnectPinsLShapeOk -eq "true" -and $connectSymbolPin.ExitCode -eq 0 -and $connectPinsLShape.ExitCode -eq 0 -and $connectSymbolPin.ExactBlocker -eq "N/A" -and $connectPinsLShape.ExactBlocker -eq "N/A" )

$firstBlocker = "N/A"
if( $connectSymbolPin.ExitCode -ne 0 ) { $firstBlocker = "schematic_connect_symbol_pin:probe_exit_code_" + $connectSymbolPin.ExitCode }
elseif( $connectSymbolPin.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_connect_symbol_pin:" + $connectSymbolPin.ExactBlocker }
elseif( $connectPinsLShape.ExitCode -ne 0 ) { $firstBlocker = "schematic_connect_pins_l_shape:probe_exit_code_" + $connectPinsLShape.ExitCode }
elseif( $connectPinsLShape.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_connect_pins_l_shape:" + $connectPinsLShape.ExactBlocker }

@(
    "sch_wiring_v2_family_green=$(if($familyGreen){'true'}else{'false'})"
    "schematic_connect_symbol_pin_ok=$schematicConnectSymbolPinOk"
    "schematic_connect_pins_l_shape_ok=$schematicConnectPinsLShapeOk"
    "exact_blocker_if_any=$firstBlocker"
    "schematic_connect_symbol_pin_target_run_dir=" + $connectSymbolPin.TargetRunDir
    "schematic_connect_pins_l_shape_target_run_dir=" + $connectPinsLShape.TargetRunDir
    "schematic_connect_symbol_pin_exit_code=" + $connectSymbolPin.ExitCode
    "schematic_connect_pins_l_shape_exit_code=" + $connectPinsLShape.ExitCode
    ""
) | Set-Content -Path (Join-Path $runDir "qa_sch_wiring_v2_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
