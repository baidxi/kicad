param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-clearance-context-v1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$probeScript = "targeted_pcb_clearance_context_probe.ps1"
$summaryName = "qa_pcb_clearance_context_session_contract_summary.txt"

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
        "pcb_clearance_context_v1_family_green=false"
        "pcb_clearance_context_ok=false"
        "target_probe_dir=absent"
        "exact_blocker_if_any=probe_script_missing"
        "ipc_smoke_ipc_retry_extra_family=absent"
        "target_run_dir=$runDir"
        ""
    ) | Set-Content -Path ( Join-Path $runDir "qa_pcb_clearance_context_v1_family_summary.txt" ) -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}

# R92 — spawn via Invoke-IpcQaChildScript (stdout+stderr TARGET_RUN_DIR, logs inchangés).
$child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
    -LogDir $runDir -LogBaseName $probeScript `
    -TargetRunDirSource StdoutAndStderr `
    -SummaryFileName $summaryName
$targetRunDir = $child.TargetRunDir

$exactBlocker = "N/A"
$familyGreen = $false
$probeGreen = $false
$ipcRetryExtra = "absent"

if ( $targetRunDir -eq "absent" ) {
    $exactBlocker = "probe_target_run_dir_missing"
}
else {
    $childSummaryPath = Join-Path $targetRunDir $summaryName
    if ( !(Test-Path -LiteralPath $childSummaryPath) ) {
        $exactBlocker = "summary_missing"
    }
    else {
        Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir ($probeScript + "." + $summaryName)) -Force
        $summaryContent = Get-Content -LiteralPath $childSummaryPath
        $greenLine = Get-SummaryField -SummaryLines $summaryContent -Key "pcb_clearance_context_green"
        $probeGreen = ($greenLine -eq "true")
        $ire = Get-SummaryField -SummaryLines $summaryContent -Key "ipc_smoke_ipc_retry_extra_total"
        if ( -not [string]::IsNullOrWhiteSpace( $ire ) ) { $ipcRetryExtra = $ire }

        $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1
        if ( -not [string]::IsNullOrWhiteSpace( $blockerLine ) ) {
            $exactBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
        }

        if ( $exactBlocker -eq "N/A" -and $child.ExitCode -eq 0 -and $probeGreen ) {
            $familyGreen = $true
        }
        elseif ( $exactBlocker -eq "N/A" -and -not $probeGreen ) {
            $exactBlocker = "pcb_clearance_context_green_false"
        }
    }
}

if ( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" ) {
    $exactBlocker = "probe_exit_code_" + $child.ExitCode
}

@(
    "pcb_clearance_context_v1_family_green=" + $( if ( $familyGreen ) { "true" } else { "false" } )
    "pcb_clearance_context_ok=" + $( if ( $probeGreen ) { "true" } else { "false" } )
    "target_probe_dir=$targetRunDir"
    "exact_blocker_if_any=$exactBlocker"
    "ipc_smoke_ipc_retry_extra_family=$ipcRetryExtra"
    "target_run_dir=$runDir"
    "probe_exit_code=" + $child.ExitCode
    ""
) | Set-Content -Path ( Join-Path $runDir "qa_pcb_clearance_context_v1_family_summary.txt" ) -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)

if ( -not $familyGreen ) { exit 1 }
exit 0
