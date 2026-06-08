param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-connectivity-v1-targeted")
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

$netInspect = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_net_inspect_probe.ps1" -SummaryName "qa_schematic_net_inspect_session_contract_summary.txt"
$symbolConnectivity = Invoke-TargetedProbe -ProbeScriptName "targeted_schematic_symbol_connectivity_probe.ps1" -SummaryName "qa_schematic_symbol_connectivity_session_contract_summary.txt"

$netInspectOk = if( $netInspect.Ok ) { "true" } else { "false" }
$symbolConnectivityOk = if( $symbolConnectivity.Ok ) { "true" } else { "false" }
$familyGreen = ( $netInspectOk -eq "true" -and $symbolConnectivityOk -eq "true" )

$firstBlocker = "N/A"
if( $netInspect.ExitCode -ne 0 ) { $firstBlocker = "schematic_net_inspect:probe_exit_code_" + $netInspect.ExitCode }
elseif( $netInspect.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_net_inspect:" + $netInspect.ExactBlocker }
elseif( $symbolConnectivity.ExitCode -ne 0 ) { $firstBlocker = "schematic_symbol_connectivity:probe_exit_code_" + $symbolConnectivity.ExitCode }
elseif( $symbolConnectivity.ExactBlocker -ne "N/A" ) { $firstBlocker = "schematic_symbol_connectivity:" + $symbolConnectivity.ExactBlocker }

@(
    "sch_connectivity_v1_family_green=" + $(if($familyGreen){"true"}else{"false"})
    "schematic_net_inspect_ok=$netInspectOk"
    "schematic_symbol_connectivity_ok=$symbolConnectivityOk"
    "exact_blocker_if_any=$firstBlocker"
    "schematic_net_inspect_target_run_dir=" + $netInspect.TargetRunDir
    "schematic_symbol_connectivity_target_run_dir=" + $symbolConnectivity.TargetRunDir
    "schematic_net_inspect_exit_code=" + $netInspect.ExitCode
    "schematic_symbol_connectivity_exit_code=" + $symbolConnectivity.ExitCode
    ""
) | Set-Content -Path (Join-Path $runDir "qa_sch_connectivity_v1_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
