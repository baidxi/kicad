param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-negative-contracts-v1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$probeScript = "targeted_pcb_negative_contracts_probe.ps1"
$childSummaryName = "qa_pcb_negative_contracts_summary.txt"

function Get-SummaryField {
    param(
        [string[]]$SummaryLines,
        [string]$Key
    )
    $line = $SummaryLines | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if ( [string]::IsNullOrWhiteSpace( $line ) ) { return $null }
    $v = ( $line -replace ( "^" + [regex]::Escape( $Key ) + "=" ), "" ).Trim()
    return $v
}

$probePath = Join-Path $ScriptDir $probeScript
if ( !(Test-Path -LiteralPath $probePath) ) {
    @(
        "pcb_negative_contracts_v1_family_green=false"
        "pcb_negative_contracts_ok=false"
        "pcb_negative_contracts_target_run_dir=absent"
        "pcb_negative_contracts_exit_code=-1"
        "exact_blocker_if_any=probe_script_missing"
        "ipc_smoke_ipc_retry_extra_family=0"
        "target_run_dir=$runDir"
        ""
    ) | Set-Content -Path ( Join-Path $runDir "qa_pcb_negative_contracts_v1_family_summary.txt" ) -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}

# R90 — spawn via Invoke-IpcQaChildScript (stdout+stderr TARGET_RUN_DIR, logs inchangés).
$child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
    -LogDir $runDir -LogBaseName $probeScript `
    -TargetRunDirSource StdoutAndStderr `
    -SummaryFileName $childSummaryName
$targetRunDir = $child.TargetRunDir

$exactBlocker = "N/A"
$familyGreen = $false
$probeGreen = $false
$ipcRetryExtra = "0"

if ( $targetRunDir -eq "absent" ) {
    $exactBlocker = "probe_target_run_dir_missing"
}
else {
    $childSummaryPath = Join-Path $targetRunDir $childSummaryName
    if ( !(Test-Path -LiteralPath $childSummaryPath) ) {
        $exactBlocker = "summary_missing"
    }
    else {
        Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir ($probeScript + "." + $childSummaryName)) -Force
        $summaryContent = Get-Content -LiteralPath $childSummaryPath
        $greenLine = Get-SummaryField -SummaryLines $summaryContent -Key "pcb_negative_contracts_green"
        $probeGreen = ($greenLine -eq "true")
        $ire = Get-SummaryField -SummaryLines $summaryContent -Key "ipc_smoke_ipc_retry_extra_family"
        if ( -not [string]::IsNullOrWhiteSpace( $ire ) ) { $ipcRetryExtra = $ire }

        $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1
        if ( -not [string]::IsNullOrWhiteSpace( $blockerLine ) ) {
            $exactBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
        }

        if ( $exactBlocker -eq "N/A" -and $child.ExitCode -eq 0 -and $probeGreen ) {
            $familyGreen = $true
        }
        elseif ( $exactBlocker -eq "N/A" -and -not $probeGreen ) {
            $exactBlocker = "pcb_negative_contracts_green_false"
        }
    }
}

if ( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" ) {
    $exactBlocker = "probe_exit_code_" + $child.ExitCode
}

@(
    "pcb_negative_contracts_v1_family_green=" + $( if ( $familyGreen ) { "true" } else { "false" } )
    "pcb_negative_contracts_ok=" + $( if ( $probeGreen ) { "true" } else { "false" } )
    "pcb_negative_contracts_target_run_dir=$targetRunDir"
    "pcb_negative_contracts_exit_code=" + $child.ExitCode
    "exact_blocker_if_any=$exactBlocker"
    "ipc_smoke_ipc_retry_extra_family=$ipcRetryExtra"
    "target_run_dir=$runDir"
    ""
) | Set-Content -Path ( Join-Path $runDir "qa_pcb_negative_contracts_v1_family_summary.txt" ) -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)

if ( -not $familyGreen ) { exit 1 }
exit 0
