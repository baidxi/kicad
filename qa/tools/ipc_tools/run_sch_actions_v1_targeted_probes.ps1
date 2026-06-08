param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R138 — Enchaine 4 probes mutantes sur issue19090 (value, field, label, assign) ;
# chaque probe enfant gere son restore ; preferer Stop-KiCadProcesses avant checkpoint MCP v2.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-actions-v1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Invoke-TargetedProbe {
    # R113 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/summary inchangés.
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

    $child = Invoke-IpcQaChildScript `
        -ScriptPath $probePath `
        -BuildDir $BuildDir `
        -LogDir $runDir `
        -LogBaseName $ProbeScriptName `
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

$setSymbolValue = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_set_symbol_value_probe.ps1" -SummaryName "qa_schematic_set_symbol_value_session_contract_summary.txt"
$setSymbolField = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_set_symbol_field_probe.ps1" -SummaryName "qa_schematic_set_symbol_field_session_contract_summary.txt"
$setLabelText = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_set_label_text_probe.ps1" -SummaryName "qa_schematic_set_label_text_session_contract_summary.txt"
$assignFootprint = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_assign_footprint_probe.ps1" -SummaryName "qa_schematic_assign_footprint_session_contract_summary.txt"

$setSymbolValueOk = if( $setSymbolValue.Ok ) { "true" } else { "false" }
$setSymbolFieldOk = if( $setSymbolField.Ok ) { "true" } else { "false" }
$setLabelTextOk = if( $setLabelText.Ok ) { "true" } else { "false" }
$assignFootprintOk = if( $assignFootprint.Ok ) { "true" } else { "false" }
$familyGreen = ( $setSymbolValueOk -eq "true" -and $setSymbolFieldOk -eq "true" -and $setLabelTextOk -eq "true" -and $assignFootprintOk -eq "true" )

$firstBlocker = "N/A"
if( $setSymbolValue.ExitCode -ne 0 ) { $firstBlocker = "schematic_set_symbol_value:probe_exit_code_" + $setSymbolValue.ExitCode }
elseif( $setSymbolValue.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_set_symbol_value:" + $setSymbolValue.ExactBlocker }
elseif( $setSymbolField.ExitCode -ne 0 ) { $firstBlocker = "schematic_set_symbol_field:probe_exit_code_" + $setSymbolField.ExitCode }
elseif( $setSymbolField.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_set_symbol_field:" + $setSymbolField.ExactBlocker }
elseif( $setLabelText.ExitCode -ne 0 ) { $firstBlocker = "schematic_set_label_text:probe_exit_code_" + $setLabelText.ExitCode }
elseif( $setLabelText.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_set_label_text:" + $setLabelText.ExactBlocker }
elseif( $assignFootprint.ExitCode -ne 0 ) { $firstBlocker = "schematic_assign_footprint:probe_exit_code_" + $assignFootprint.ExitCode }
elseif( $assignFootprint.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_assign_footprint:" + $assignFootprint.ExactBlocker }

@(
    "sch_actions_v1_family_green=" + $(if($familyGreen){"true"}else{"false"})
    "schematic_set_symbol_value_ok=$setSymbolValueOk"
    "schematic_set_symbol_field_ok=$setSymbolFieldOk"
    "schematic_set_label_text_ok=$setLabelTextOk"
    "schematic_assign_footprint_ok=$assignFootprintOk"
    "exact_blocker_if_any=$firstBlocker"
    "schematic_set_symbol_value_target_run_dir=" + $setSymbolValue.TargetRunDir
    "schematic_set_symbol_field_target_run_dir=" + $setSymbolField.TargetRunDir
    "schematic_set_label_text_target_run_dir=" + $setLabelText.TargetRunDir
    "schematic_assign_footprint_target_run_dir=" + $assignFootprint.TargetRunDir
    "schematic_set_symbol_value_exit_code=" + $setSymbolValue.ExitCode
    "schematic_set_symbol_field_exit_code=" + $setSymbolField.ExitCode
    "schematic_set_label_text_exit_code=" + $setLabelText.ExitCode
    "schematic_assign_footprint_exit_code=" + $assignFootprint.ExitCode
    ""
) | Set-Content -Path (Join-Path $runDir "qa_sch_actions_v1_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
