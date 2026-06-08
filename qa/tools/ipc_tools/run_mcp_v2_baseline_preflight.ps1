# Préflight MCP v2 local (**R34**) — pas un gate officiel isolé : voir README_IPC_QA_BASELINE.md.
#
# Rôle après **R33** :
#   - optionnel : **BuildMinimal**, vérif binaires (**VerifyBinariesOnly** quitte tôt) ;
#   - optionnel : smoke DLL (**Test-IpcKifaceDllLoad**) si **KICAD_IPC_KIFACE_SMOKE=1** dans cette session ;
#   - garde-fou PATH : **`test_ipc_qa_path_idempotence.ps1`** (**R86**, après init env ; **pas** sous **VerifyBinariesOnly**) ;
#   - **fail-fast PCB** : **`run_pcb_negative_contracts_v1_targeted_probes.ps1`** (**R66**), puis
#     **`targeted_pcb_measure_distance_probe.ps1`** puis **`run_pcb_geometry_v1_targeted_probes.ps1`**
#     (double emploi partiel avec le checkpoint : permet d'échouer vite avant les familles SCH longues) ;
#   - sauf **SkipCheckpoint** : **`run_mcp_v2_checkpoint.ps1`** = gate **officiel à 8 familles** (**R39** : inclut **`pcb_nearby_items_context_v1`**)
#     (inclut **`pcb_clearance_context_v1`** ; **pas** besoin de lancer clearance avant — évite doublon).
#
# Pour baseline CI / toolchain complet : **`run_mcp_v2_ci_baseline.ps1`**.
#
# Usage:
#   .\run_mcp_v2_baseline_preflight.ps1 -BuildDir "C:\dev\kicad-master\build\msvc-win64-release"
#   $env:KICAD_IPC_KIFACE_SMOKE = '1'; .\run_mcp_v2_baseline_preflight.ps1 -BuildDir "..."
# Résumé écrit : **`qa_mcp_v2_baseline_preflight_summary.txt`** sous **`TARGET_RUN_DIR`** (préflight).

param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [switch]$BuildMinimal,
    [switch]$VerifyBinariesOnly,
    [switch]$SkipCheckpoint
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $BuildDir "..\..")).Path

function Test-IpcBaselineBinariesPresent {
    param([string]$Root)
    $need = @(
        (Join-Path $Root "common\kicommon.dll"),
        (Join-Path $Root "pcbnew\_pcbnew.dll"),
        (Join-Path $Root "kicad\kicad.exe"),
        (Join-Path $Root "qa\tools\ipc_tools\qa_ipc_smoke.exe")
    )
    $missing = New-Object System.Collections.ArrayList
    foreach ($p in $need) {
        if (-not (Test-Path -LiteralPath $p)) { [void]$missing.Add($p) }
    }
    return , $missing.ToArray()
}

function Read-SummaryField {
    param(
        [string]$SummaryPath,
        [string]$Key
    )
    if (-not (Test-Path -LiteralPath $SummaryPath)) { return $null }
    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) { return $null }
    return ( $line -replace ( "^" + [regex]::Escape($Key) + "=" ), "" ).Trim()
}

function Invoke-PreflightChild {
    # R89 — wrapper local vers Invoke-IpcQaChildScript (stdout-only TARGET_RUN_DIR, logs sous $runDir).
    param(
        [string]$ScriptPath,
        [string]$LogLabel
    )
    $logBase = if ($LogLabel) { $LogLabel } else { [System.IO.Path]::GetFileNameWithoutExtension($ScriptPath) }
    $child = Invoke-IpcQaChildScript -ScriptPath $ScriptPath -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName $logBase -TargetRunDirSource StdoutOnly
    return [pscustomobject]@{
        ExitCode = $child.ExitCode
        StdOut   = $child.StdOut
        StdErr   = $child.StdErr
        TargetRunDir = $child.TargetRunDir
    }
}

if ($BuildMinimal) {
    $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    foreach ($edition in @("Professional", "Community", "Enterprise")) {
        if (-not (Test-Path $vsDevCmd)) {
            $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\$edition\Common7\Tools\VsDevCmd.bat"
        }
    }
    if (-not (Test-Path $vsDevCmd)) {
        throw "VsDevCmd.bat not found. Install Visual Studio 2022 Build Tools or full VS."
    }
    Push-Location (Resolve-Path $repoRoot)
    try {
        $targets = "kicommon", "pcbnew_kiface", "kicad", "qa_ipc_smoke"
        $targ = $targets -join " "
        $cmd = "`"$vsDevCmd`" -arch=amd64 && cmake --build --preset win64-release --target $targ"
        cmd /c $cmd
        if ($LASTEXITCODE -ne 0) { throw "cmake build failed with exit code $LASTEXITCODE" }
    }
    finally {
        Pop-Location
    }
}

$missingBins = Test-IpcBaselineBinariesPresent -Root $BuildDir
if ($missingBins.Count -gt 0) {
    Write-Error ("Missing baseline binaries: " + ($missingBins -join "; "))
}

if ($VerifyBinariesOnly) {
    Write-Output "BASELINE_BINARIES_OK=1"
    exit 0
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-mcp-v2-baseline-preflight")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $repoRoot

if ($env:KICAD_IPC_KIFACE_SMOKE -eq "1") {
    $smokeRes = Test-IpcKifaceDllLoad -BuildDir $BuildDir -ArtifactsDir $runDir
    if (-not $smokeRes.AllOk) {
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        throw "Test-IpcKifaceDllLoad failed (see qa_kiface_dll_smoke.txt in preflight dir)."
    }
}

# R86/R88 — garde-fou PATH idempotent (helper commun ; hors VerifyBinariesOnly)
$pathGuard = Invoke-IpcPathIdempotenceGuard -BuildDir $BuildDir -IpcToolsDir $ScriptDir -LogDir $runDir
$ipcPathIdempotenceOk = $pathGuard.Ok
$pathIdemSummaryPath = $pathGuard.SummaryPath
$pathIdemExitCode = $pathGuard.ExitCode

# R66 — garde-fou contrats board_filename / max_items (avant probes mesure/géométrie)
$negFam = Join-Path $ScriptDir "run_pcb_negative_contracts_v1_targeted_probes.ps1"
$rNeg = Invoke-PreflightChild -ScriptPath $negFam -LogLabel "run_pcb_negative_contracts_v1_targeted_probes"
$negFamSummaryPath = if ($rNeg.TargetRunDir -ne "absent") {
    Join-Path $rNeg.TargetRunDir "qa_pcb_negative_contracts_v1_family_summary.txt"
} else { "" }
$negFamGreen = if ($negFamSummaryPath -and (Test-Path -LiteralPath $negFamSummaryPath)) {
    Read-SummaryField -SummaryPath $negFamSummaryPath -Key "pcb_negative_contracts_v1_family_green"
} else { $null }
$negFamExitCode = [string]$rNeg.ExitCode
if ($rNeg.ExitCode -ne 0 -or $negFamGreen -ne "true") {
    $negBlocker = if ($negFamSummaryPath) {
        Read-SummaryField -SummaryPath $negFamSummaryPath -Key "exact_blocker_if_any"
    } else { "summary_absent" }
    throw "run_pcb_negative_contracts_v1_targeted_probes.ps1 not green (exit=$negFamExitCode exact_blocker_if_any=$negBlocker)"
}

$probe = Join-Path $ScriptDir "targeted_pcb_measure_distance_probe.ps1"
$rMeas = Invoke-PreflightChild -ScriptPath $probe -LogLabel "targeted_pcb_measure_distance_probe"
if ($rMeas.ExitCode -ne 0) { throw "targeted_pcb_measure_distance_probe.ps1 failed exit $($rMeas.ExitCode)" }

$geom = Join-Path $ScriptDir "run_pcb_geometry_v1_targeted_probes.ps1"
$rGeom = Invoke-PreflightChild -ScriptPath $geom -LogLabel "run_pcb_geometry_v1_targeted_probes"
if ($rGeom.ExitCode -ne 0) { throw "run_pcb_geometry_v1_targeted_probes.ps1 failed exit $($rGeom.ExitCode)" }

$checkpointRun = "skipped"
if (-not $SkipCheckpoint) {
    $ck = Join-Path $ScriptDir "run_mcp_v2_checkpoint.ps1"
    $rCk = Invoke-PreflightChild -ScriptPath $ck -LogLabel "run_mcp_v2_checkpoint"
    if ($rCk.ExitCode -ne 0) { throw "run_mcp_v2_checkpoint.ps1 failed exit $($rCk.ExitCode)" }
    $checkpointRun = "run_mcp_v2_checkpoint_ok_exit_0"
}

@(
    "preflight_ok=true"
    "preflight_role=local_fail_fast_pcb_then_checkpoint_gate"
    "ipc_path_idempotence_ok=$ipcPathIdempotenceOk"
    "ipc_path_idempotence_target_run_dir=" + $pathGuard.TargetRunDir
    "ipc_path_idempotence_exit_code=$pathIdemExitCode"
    "ipc_path_idempotence_summary_path=$pathIdemSummaryPath"
    "pcb_negative_contracts_v1_ok=true"
    "pcb_negative_contracts_v1_target_run_dir=" + $rNeg.TargetRunDir
    "pcb_negative_contracts_v1_exit_code=$negFamExitCode"
    "pcb_negative_contracts_v1_summary_path=$negFamSummaryPath"
    "checkpoint_script=run_mcp_v2_checkpoint.ps1"
    "checkpoint_family_count=8"
    "pcb_negative_contracts_v1_in_checkpoint=false"
    "pcb_negative_contracts_v1_in_preflight=true"
    "pcb_clearance_context_v1_explicit_in_preflight=false"
    "pcb_clearance_context_v1_included_via=run_mcp_v2_checkpoint.ps1"
    "note_after_r66=pcb_negative_contracts_v1 is a preflight/CI-baseline guard only; the 8-family checkpoint child does not run it."
    "kiface_smoke_if_env=KICAD_IPC_KIFACE_SMOKE=1"
    "steps=optional_build_minimal;verify_binaries;optional_kiface_smoke;test_ipc_qa_path_idempotence;run_pcb_negative_contracts_v1_targeted_probes;targeted_pcb_measure_distance;run_pcb_geometry_v1_targeted_probes;$(if($SkipCheckpoint){'checkpoint_skipped'}else{$checkpointRun})"
    "skip_checkpoint=" + $(if($SkipCheckpoint) { "true" } else { "false" })
    "target_run_dir=$runDir"
    ""
) | Set-Content -Path (Join-Path $runDir "qa_mcp_v2_baseline_preflight_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
Write-Output "MCP_V2_BASELINE_PREFLIGHT_OK=1"
exit 0
