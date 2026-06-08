# Pipeline baseline MCP v2 reproductible (local / pré-CI) — **R28**.
# Enchaîne : pré-check processus → toolchain MSVC → build minimal optionnel → smoke KIFACE →
# garde-fou PATH (**test_ipc_qa_path_idempotence.ps1**, **R87**) → probes pcb_geometry →
# famille pcb_geometry v1 → checkpoint MCP v2 (smoke KIFACE si demandé) →
# report advisory SCH authoring (**report_sch_authoring_advisory.ps1**, **R106/R108**, observability-only) →
# runtime footprint workflow (**r163_example_runtime_selected_footprint_workflow.ps1**, **R164**, observability-only) →
# dual-source footprint workflow consistency (**report_footprint_workflow_discovery_consistency.ps1**, **R167**, observability-only) →
# PCB apply DRC save workflow (**r177_example_pcb_apply_drc_save_workflow.ps1**, **R178**, observability-only).
# Alert policy G4/G5/G6 documented in MCP_DECISIONS.md R168/R179 — ci_baseline_green does not suppress advisory failures.
# Official MCP v2 release (go/no-go + pack): run_mcp_v2_release_pack.ps1 — MCP_DECISIONS.md R185 (distinct from dev CI gate).
#
# Usage:
#   .\run_mcp_v2_ci_baseline.ps1 -BuildDir "C:\dev\kicad-master\build\msvc-win64-release" -BuildMinimal
#   .\run_mcp_v2_ci_baseline.ps1 -BuildDir "..." -SkipBuild -KillRunningKiCad
#   .\run_mcp_v2_ci_baseline.ps1 -BuildDir "..." -VerifyOnly
#
# Voir README_IPC_QA_BASELINE.md section « Pipeline CI baseline MCP v2 ».

param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [switch]$BuildMinimal,
    [switch]$KillRunningKiCad,
    [switch]$SkipBuild,
    [switch]$SkipCheckpoint,

    [Parameter()]
    [bool]$EnableKifaceSmoke = $true,

    [switch]$VerifyOnly,

    [int]$SchAuthoringAdvisoryScanLastN = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($SchAuthoringAdvisoryScanLastN -le 0) {
    throw "SchAuthoringAdvisoryScanLastN must be >= 1 (got $SchAuthoringAdvisoryScanLastN). Scan cannot be disabled via N<=0."
}

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")

$repoRoot = (Resolve-Path (Join-Path $BuildDir "..\..")).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-mcp-v2-ci-baseline")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$ciBaselineStartedAt = [DateTimeOffset]::UtcNow.ToString("o")
$ciBaselineSw = [System.Diagnostics.Stopwatch]::StartNew()
$buildElapsedMsStr = "not_run_yet"

function Get-VsDevCmdPath {
    $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    foreach ($edition in @("Professional", "Community", "Enterprise")) {
        if (-not (Test-Path -LiteralPath $vsDevCmd)) {
            $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\$edition\Common7\Tools\VsDevCmd.bat"
        }
    }
    return $vsDevCmd
}

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

function Test-MsvcToolchainArtifacts {
    param(
        [string]$VsDevCmdPath,
        [string]$LogPath
    )
    # Cursor / minimal Windows shells often launch cmd.exe without %SystemRoot%\System32 on PATH.
    # VsDevCmd does not restore it; bare `where` then fails. Prepend System32 and use where.exe + call.
    $sys32 = "%SystemRoot%\System32"
    # Do not use `if errorlevel 1 (...)` inside a long `&&` chain: cmd stops the chain silently.
    $inner = @(
        "echo [toolchain] where.exe cl",
        "(where.exe cl || (echo FAIL:cl_missing & exit /b 11))",
        "echo [toolchain] where.exe cmake",
        "(where.exe cmake || (echo FAIL:cmake_missing & exit /b 12))",
        "echo [toolchain] where.exe ninja",
        "(where.exe ninja || (echo FAIL:ninja_missing & exit /b 13))",
        "echo [toolchain] cmake --version",
        "(cmake --version || (echo FAIL:cmake_version & exit /b 14))",
        "echo [toolchain] ninja --version",
        "(ninja --version || (echo FAIL:ninja_version & exit /b 15))"
    ) -join " && "
    $cmd = "set PATH=$sys32;%PATH% && call `"$VsDevCmdPath`" -arch=amd64 && $inner"
    $out = cmd.exe /c $cmd 2>&1
    $txt = $out | Out-String
    $txt | Set-Content -Path $LogPath -Encoding UTF8
    if ($LASTEXITCODE -ne 0) {
        $missing = "unknown"
        if ($txt -match 'FAIL:([a-z_]+)') { $missing = $Matches[1] }
        $script:toolchainMissingBinary = $missing
        throw "MSVC toolchain check failed ($missing, exit=$LASTEXITCODE). See toolchain_verify.log in TARGET_RUN_DIR."
    }
}

function Read-SummaryField {
    param(
        [string]$SummaryPath,
        [string]$Key
    )
    if (-not (Test-Path -LiteralPath $SummaryPath)) {
        return $null
    }
    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return $null
    }
    return ( $line -replace ( "^" + [regex]::Escape($Key) + "=" ), "" ).Trim()
}

function Invoke-ProbeCaptureTargetDir {
    # R89 — wrapper local vers Invoke-IpcQaChildScript (stdout-only TARGET_RUN_DIR, logs sous $runDir).
    param(
        [string]$ProbeScriptPath,
        [string[]]$ExtraArgs
    )
    $logBase = [System.IO.Path]::GetFileNameWithoutExtension($ProbeScriptPath)
    $child = Invoke-IpcQaChildScript -ScriptPath $ProbeScriptPath -BuildDir $BuildDir `
        -ExtraArgs $ExtraArgs -LogDir $runDir -LogBaseName $logBase -TargetRunDirSource StdoutOnly
    return [pscustomobject]@{
        ExitCode = $child.ExitCode
        StdOut   = $child.StdOut
        StdErr   = $child.StdErr
        TargetRunDir = $child.TargetRunDir
    }
}

function Set-SchAuthoringAdvisoryScanCiSummary {
    param(
        [string]$Ok,
        [int]$ExitCode,
        [string]$TargetRunDir,
        [string]$SummaryPath,
        [string]$WarningEmitted,
        [string]$RecommendedAction,
        [string]$CheckpointsScanned,
        [string]$CheckpointsWithAdvisory,
        [string]$KicadExitedCount,
        [string]$MutationRetrySum,
        [string]$RestoreRetrySum,
        [string]$PingRetrySum,
        [string]$Blocker = "N/A"
    )
    $sum.sch_authoring_advisory_scan_ok = $Ok
    $sum.sch_authoring_advisory_scan_exit_code = [string]$ExitCode
    $sum.sch_authoring_advisory_scan_target_run_dir = if ($TargetRunDir) { $TargetRunDir } else { "N/A" }
    $sum.sch_authoring_advisory_scan_summary_path = if ($SummaryPath) { $SummaryPath } else { "" }
    $sum.sch_authoring_advisory_scan_warning_emitted = $WarningEmitted
    $sum.sch_authoring_advisory_scan_recommended_action = $RecommendedAction
    $sum.sch_authoring_advisory_scan_checkpoints_scanned = $CheckpointsScanned
    $sum.sch_authoring_advisory_scan_checkpoints_with_advisory = $CheckpointsWithAdvisory
    $sum.sch_authoring_advisory_scan_kicad_exited_count = $KicadExitedCount
    $sum.sch_authoring_advisory_scan_mutation_retry_extra_total_sum = $MutationRetrySum
    $sum.sch_authoring_advisory_scan_restore_retry_extra_total_sum = $RestoreRetrySum
    $sum.sch_authoring_advisory_scan_post_mutation_ping_retry_extra_total_sum = $PingRetrySum
    $sum.sch_authoring_advisory_scan_blocker = $Blocker
}

function Invoke-SchAuthoringAdvisoryScan {
    # R108 — observability-only ; n'affecte pas ci_baseline_green.
    param(
        [int]$ScanLastN = $SchAuthoringAdvisoryScanLastN
    )
    $reportScript = Join-Path $ScriptDir "report_sch_authoring_advisory.ps1"
    if (-not (Test-Path -LiteralPath $reportScript)) {
        Set-SchAuthoringAdvisoryScanCiSummary -Ok "false" -ExitCode -1 -TargetRunDir "N/A" -SummaryPath "" `
            -WarningEmitted "N/A" -RecommendedAction "N/A" -CheckpointsScanned "N/A" -CheckpointsWithAdvisory "N/A" `
            -KicadExitedCount "N/A" -MutationRetrySum "N/A" -RestoreRetrySum "N/A" -PingRetrySum "N/A" `
            -Blocker "report_script_missing"
        return
    }
    $child = Invoke-IpcQaChildScript -ScriptPath $reportScript -BuildDir $BuildDir `
        -ExtraArgs @('-ScanLastNCheckpoints', [string]$ScanLastN) `
        -LogDir $runDir -LogBaseName "report_sch_authoring_advisory_scan" -TargetRunDirSource StdoutOnly
    $scanTargetRunDir = $child.TargetRunDir
    $scanSummaryPath = if ($scanTargetRunDir -and $scanTargetRunDir -ne "absent") {
        Join-Path $scanTargetRunDir "qa_sch_authoring_advisory_report_summary.txt"
    } else {
        ""
    }
    if ($child.ExitCode -ne 0) {
        Set-SchAuthoringAdvisoryScanCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $scanTargetRunDir `
            -SummaryPath $scanSummaryPath -WarningEmitted "N/A" -RecommendedAction "N/A" -CheckpointsScanned "N/A" `
            -CheckpointsWithAdvisory "N/A" -KicadExitedCount "N/A" -MutationRetrySum "N/A" -RestoreRetrySum "N/A" `
            -PingRetrySum "N/A" -Blocker "scan_exit_code=$($child.ExitCode)"
        return
    }
    if (-not (Test-Path -LiteralPath $scanSummaryPath)) {
        Set-SchAuthoringAdvisoryScanCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $scanTargetRunDir `
            -SummaryPath $scanSummaryPath -WarningEmitted "N/A" -RecommendedAction "N/A" -CheckpointsScanned "N/A" `
            -CheckpointsWithAdvisory "N/A" -KicadExitedCount "N/A" -MutationRetrySum "N/A" -RestoreRetrySum "N/A" `
            -PingRetrySum "N/A" -Blocker "scan_summary_missing"
        return
    }
    $scanMode = Read-SummaryField -SummaryPath $scanSummaryPath -Key "scan_mode"
    if ($scanMode -ne "last_n_checkpoints") {
        Set-SchAuthoringAdvisoryScanCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $scanTargetRunDir `
            -SummaryPath $scanSummaryPath -WarningEmitted "N/A" -RecommendedAction "N/A" -CheckpointsScanned "N/A" `
            -CheckpointsWithAdvisory "N/A" -KicadExitedCount "N/A" -MutationRetrySum "N/A" -RestoreRetrySum "N/A" `
            -PingRetrySum "N/A" -Blocker "scan_mode_unexpected=$scanMode"
        return
    }
    Set-SchAuthoringAdvisoryScanCiSummary -Ok "true" -ExitCode $child.ExitCode -TargetRunDir $scanTargetRunDir `
        -SummaryPath $scanSummaryPath `
        -WarningEmitted $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "advisory_warning_emitted") `
        -RecommendedAction $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "recommended_action") `
        -CheckpointsScanned $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "checkpoints_scanned") `
        -CheckpointsWithAdvisory $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "checkpoints_with_advisory") `
        -KicadExitedCount $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "kicad_exited_post_mutation_detected_count") `
        -MutationRetrySum $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "mutation_retry_extra_total_sum") `
        -RestoreRetrySum $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "restore_retry_extra_total_sum") `
        -PingRetrySum $(Read-SummaryField -SummaryPath $scanSummaryPath -Key "post_mutation_ping_retry_extra_total_sum")
}

function Set-RuntimeFootprintWorkflowCiSummary {
    param(
        [string]$Ok,
        [int]$ExitCode,
        [string]$TargetRunDir,
        [string]$SummaryPath,
        [string]$Green,
        [string]$SelectedWorkflow,
        [string]$RuntimeStatusReadOk,
        [string]$HierarchyAssignInstanceSafeEnabled,
        [string]$AssignHierarchyDryRunOk,
        [string]$MutationPerformed,
        [string]$Blocker = "N/A"
    )
    $sum.runtime_footprint_workflow_ok = $Ok
    $sum.runtime_footprint_workflow_exit_code = [string]$ExitCode
    $sum.runtime_footprint_workflow_target_run_dir = if ($TargetRunDir) { $TargetRunDir } else { "N/A" }
    $sum.runtime_footprint_workflow_summary_path = if ($SummaryPath) { $SummaryPath } else { "" }
    $sum.runtime_footprint_workflow_green = $Green
    $sum.runtime_footprint_workflow_selected_workflow = $SelectedWorkflow
    $sum.runtime_footprint_workflow_runtime_status_read_ok = $RuntimeStatusReadOk
    $sum.runtime_footprint_workflow_hierarchy_assign_instance_safe_enabled = $HierarchyAssignInstanceSafeEnabled
    $sum.runtime_footprint_workflow_assign_hierarchy_dry_run_ok = $AssignHierarchyDryRunOk
    $sum.runtime_footprint_workflow_mutation_performed = $MutationPerformed
    $sum.runtime_footprint_workflow_blocker = $Blocker
}

function Invoke-RuntimeFootprintWorkflowReport {
    # R164 — observability-only ; n'affecte pas ci_baseline_green.
    $r163Script = Join-Path $ScriptDir "r163_example_runtime_selected_footprint_workflow.ps1"
    if (-not (Test-Path -LiteralPath $r163Script)) {
        Set-RuntimeFootprintWorkflowCiSummary -Ok "false" -ExitCode -1 -TargetRunDir "N/A" -SummaryPath "" `
            -Green "N/A" -SelectedWorkflow "N/A" -RuntimeStatusReadOk "N/A" `
            -HierarchyAssignInstanceSafeEnabled "N/A" -AssignHierarchyDryRunOk "N/A" -MutationPerformed "N/A" `
            -Blocker "r163_script_missing"
        return
    }
    $child = Invoke-IpcQaChildScript -ScriptPath $r163Script -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName "r163_runtime_footprint_workflow" -TargetRunDirSource StdoutOnly
    $r163TargetRunDir = $child.TargetRunDir
    $r163SummaryPath = if ($r163TargetRunDir -and $r163TargetRunDir -ne "absent") {
        Join-Path $r163TargetRunDir "qa_r163_runtime_selected_footprint_workflow_summary.txt"
    } else {
        ""
    }
    if ($child.ExitCode -ne 0) {
        $failBlocker = "r163_exit_code=$($child.ExitCode)"
        if (Test-Path -LiteralPath $r163SummaryPath) {
            $b = Read-SummaryField -SummaryPath $r163SummaryPath -Key "exact_blocker_if_any"
            if ($b) { $failBlocker = $b }
        }
        Set-RuntimeFootprintWorkflowCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $r163TargetRunDir `
            -SummaryPath $r163SummaryPath -Green "false" `
            -SelectedWorkflow $(Read-SummaryField -SummaryPath $r163SummaryPath -Key "selected_workflow") `
            -RuntimeStatusReadOk $(Read-SummaryField -SummaryPath $r163SummaryPath -Key "runtime_status_read_ok") `
            -HierarchyAssignInstanceSafeEnabled $(Read-SummaryField -SummaryPath $r163SummaryPath -Key "hierarchy_assign_instance_safe_enabled") `
            -AssignHierarchyDryRunOk $(Read-SummaryField -SummaryPath $r163SummaryPath -Key "assign_hierarchy_dry_run_ok") `
            -MutationPerformed $(Read-SummaryField -SummaryPath $r163SummaryPath -Key "mutation_performed") `
            -Blocker $failBlocker
        return
    }
    if (-not (Test-Path -LiteralPath $r163SummaryPath)) {
        Set-RuntimeFootprintWorkflowCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $r163TargetRunDir `
            -SummaryPath $r163SummaryPath -Green "N/A" -SelectedWorkflow "N/A" -RuntimeStatusReadOk "N/A" `
            -HierarchyAssignInstanceSafeEnabled "N/A" -AssignHierarchyDryRunOk "N/A" -MutationPerformed "N/A" `
            -Blocker "r163_summary_missing"
        return
    }
    $r163Green = Read-SummaryField -SummaryPath $r163SummaryPath -Key "r163_workflow_green"
    $selectedWorkflow = Read-SummaryField -SummaryPath $r163SummaryPath -Key "selected_workflow"
    $runtimeStatusReadOk = Read-SummaryField -SummaryPath $r163SummaryPath -Key "runtime_status_read_ok"
    $hierarchyEnabled = Read-SummaryField -SummaryPath $r163SummaryPath -Key "hierarchy_assign_instance_safe_enabled"
    $assignDryRunOk = Read-SummaryField -SummaryPath $r163SummaryPath -Key "assign_hierarchy_dry_run_ok"
    $mutationPerformed = Read-SummaryField -SummaryPath $r163SummaryPath -Key "mutation_performed"
    $blocker = Read-SummaryField -SummaryPath $r163SummaryPath -Key "exact_blocker_if_any"
    $obsOk = if ($r163Green -eq "true") { "true" } else { "false" }
    Set-RuntimeFootprintWorkflowCiSummary -Ok $obsOk -ExitCode $child.ExitCode -TargetRunDir $r163TargetRunDir `
        -SummaryPath $r163SummaryPath -Green $(if ($r163Green) { $r163Green } else { "N/A" }) `
        -SelectedWorkflow $(if ($selectedWorkflow) { $selectedWorkflow } else { "N/A" }) `
        -RuntimeStatusReadOk $(if ($runtimeStatusReadOk) { $runtimeStatusReadOk } else { "N/A" }) `
        -HierarchyAssignInstanceSafeEnabled $(if ($hierarchyEnabled) { $hierarchyEnabled } else { "N/A" }) `
        -AssignHierarchyDryRunOk $(if ($assignDryRunOk) { $assignDryRunOk } else { "N/A" }) `
        -MutationPerformed $(if ($mutationPerformed) { $mutationPerformed } else { "N/A" }) `
        -Blocker $(if ($blocker) { $blocker } else { "N/A" })
}

function Set-FootprintWorkflowDiscoveryConsistencyCiSummary {
    param(
        [string]$Ok,
        [int]$ExitCode,
        [string]$TargetRunDir,
        [string]$SummaryPath,
        [string]$Green,
        [string]$SnapshotStatusOk,
        [string]$ReadinessStatusOk,
        [string]$ScopeMatchOk,
        [string]$AgentPrimarySource,
        [string]$AgentLocalSource,
        [string]$OnDivergence,
        [string]$Blocker = "N/A"
    )
    $sum.footprint_workflow_discovery_consistency_ok = $Ok
    $sum.footprint_workflow_discovery_consistency_exit_code = [string]$ExitCode
    $sum.footprint_workflow_discovery_consistency_target_run_dir = if ($TargetRunDir) { $TargetRunDir } else { "N/A" }
    $sum.footprint_workflow_discovery_consistency_summary_path = if ($SummaryPath) { $SummaryPath } else { "" }
    $sum.footprint_workflow_discovery_consistency_green = $Green
    $sum.footprint_workflow_discovery_consistency_snapshot_status_ok = $SnapshotStatusOk
    $sum.footprint_workflow_discovery_consistency_readiness_status_ok = $ReadinessStatusOk
    $sum.footprint_workflow_discovery_consistency_scope_match_ok = $ScopeMatchOk
    $sum.footprint_workflow_discovery_consistency_agent_primary_source = $AgentPrimarySource
    $sum.footprint_workflow_discovery_consistency_agent_local_source = $AgentLocalSource
    $sum.footprint_workflow_discovery_consistency_on_divergence = $OnDivergence
    $sum.footprint_workflow_discovery_consistency_blocker = $Blocker
}

function Invoke-FootprintWorkflowDiscoveryConsistencyReport {
    # R167 — observability-only ; n'affecte pas ci_baseline_green.
    $reportScript = Join-Path $ScriptDir "report_footprint_workflow_discovery_consistency.ps1"
    if (-not (Test-Path -LiteralPath $reportScript)) {
        Set-FootprintWorkflowDiscoveryConsistencyCiSummary -Ok "false" -ExitCode -1 -TargetRunDir "N/A" -SummaryPath "" `
            -Green "N/A" -SnapshotStatusOk "N/A" -ReadinessStatusOk "N/A" -ScopeMatchOk "N/A" `
            -AgentPrimarySource "N/A" -AgentLocalSource "N/A" -OnDivergence "N/A" `
            -Blocker "consistency_report_script_missing"
        return
    }
    $child = Invoke-IpcQaChildScript -ScriptPath $reportScript -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName "report_footprint_workflow_discovery_consistency" -TargetRunDirSource StdoutOnly
    $reportTargetRunDir = $child.TargetRunDir
    $reportSummaryPath = if ($reportTargetRunDir -and $reportTargetRunDir -ne "absent") {
        Join-Path $reportTargetRunDir "qa_footprint_workflow_discovery_consistency_summary.txt"
    } else {
        ""
    }
    if ($child.ExitCode -ne 0) {
        $failBlocker = "consistency_exit_code=$($child.ExitCode)"
        if (Test-Path -LiteralPath $reportSummaryPath) {
            $b = Read-SummaryField -SummaryPath $reportSummaryPath -Key "exact_blocker_if_any"
            if ($b) { $failBlocker = $b }
        }
        Set-FootprintWorkflowDiscoveryConsistencyCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
            -SummaryPath $reportSummaryPath -Green "false" `
            -SnapshotStatusOk $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "snapshot_runtime_status_read_ok") `
            -ReadinessStatusOk $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "readiness_runtime_status_read_ok") `
            -ScopeMatchOk $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "recommended_assign_scope_match_ok") `
            -AgentPrimarySource $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_primary_source") `
            -AgentLocalSource $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_local_source") `
            -OnDivergence $(Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_on_divergence") `
            -Blocker $failBlocker
        return
    }
    if (-not (Test-Path -LiteralPath $reportSummaryPath)) {
        Set-FootprintWorkflowDiscoveryConsistencyCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
            -SummaryPath $reportSummaryPath -Green "N/A" -SnapshotStatusOk "N/A" -ReadinessStatusOk "N/A" -ScopeMatchOk "N/A" `
            -AgentPrimarySource "N/A" -AgentLocalSource "N/A" -OnDivergence "N/A" `
            -Blocker "consistency_summary_missing"
        return
    }
    $consistencyGreen = Read-SummaryField -SummaryPath $reportSummaryPath -Key "footprint_workflow_discovery_consistency_green"
    $snapshotStatusOk = Read-SummaryField -SummaryPath $reportSummaryPath -Key "snapshot_runtime_status_read_ok"
    $readinessStatusOk = Read-SummaryField -SummaryPath $reportSummaryPath -Key "readiness_runtime_status_read_ok"
    $scopeMatchOk = Read-SummaryField -SummaryPath $reportSummaryPath -Key "recommended_assign_scope_match_ok"
    $agentPrimary = Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_primary_source"
    $agentLocal = Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_local_source"
    $onDivergence = Read-SummaryField -SummaryPath $reportSummaryPath -Key "agent_on_divergence"
    $blocker = Read-SummaryField -SummaryPath $reportSummaryPath -Key "exact_blocker_if_any"
    $obsOk = if ($consistencyGreen -eq "true") { "true" } else { "false" }
    Set-FootprintWorkflowDiscoveryConsistencyCiSummary -Ok $obsOk -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
        -SummaryPath $reportSummaryPath -Green $(if ($consistencyGreen) { $consistencyGreen } else { "N/A" }) `
        -SnapshotStatusOk $(if ($snapshotStatusOk) { $snapshotStatusOk } else { "N/A" }) `
        -ReadinessStatusOk $(if ($readinessStatusOk) { $readinessStatusOk } else { "N/A" }) `
        -ScopeMatchOk $(if ($scopeMatchOk) { $scopeMatchOk } else { "N/A" }) `
        -AgentPrimarySource $(if ($agentPrimary) { $agentPrimary } else { "N/A" }) `
        -AgentLocalSource $(if ($agentLocal) { $agentLocal } else { "N/A" }) `
        -OnDivergence $(if ($onDivergence) { $onDivergence } else { "N/A" }) `
        -Blocker $(if ($blocker) { $blocker } else { "N/A" })
}

function Set-PcbApplyDrcSaveWorkflowCiSummary {
    param(
        [string]$Ok,
        [int]$ExitCode,
        [string]$Green,
        [string]$PreviewOk,
        [string]$ApplyConfirmRequiredOk,
        [string]$ApplyWithDrcAdvisoryOk,
        [string]$PostApplyDrcSummaryPresentOk,
        [string]$PostApplySyncReadinessOk,
        [string]$SaveConfirmRequiredOk,
        [string]$SaveConfirmedOk,
        [string]$PostSaveVerifyOk,
        [string]$Blocker = "N/A"
    )
    $sum.pcb_apply_drc_save_workflow_ok = $Ok
    $sum.pcb_apply_drc_save_workflow_exit_code = [string]$ExitCode
    $sum.pcb_apply_drc_save_workflow_green = $Green
    $sum.pcb_apply_drc_save_workflow_preview_ok = $PreviewOk
    $sum.pcb_apply_drc_save_workflow_apply_confirm_required_ok = $ApplyConfirmRequiredOk
    $sum.pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok = $ApplyWithDrcAdvisoryOk
    $sum.pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok = $PostApplyDrcSummaryPresentOk
    $sum.pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok = $PostApplySyncReadinessOk
    $sum.pcb_apply_drc_save_workflow_save_confirm_required_ok = $SaveConfirmRequiredOk
    $sum.pcb_apply_drc_save_workflow_save_confirmed_ok = $SaveConfirmedOk
    $sum.pcb_apply_drc_save_workflow_post_save_verify_ok = $PostSaveVerifyOk
    $sum.pcb_apply_drc_save_workflow_blocker = $Blocker
}

function Invoke-PcbApplyDrcSaveWorkflowReport {
    # R178 — observability-only ; n'affecte pas ci_baseline_green.
    # Politique alerte G6 : MCP_DECISIONS.md R179 (clôture série PCB apply/save R170–R179).
    $r177Script = Join-Path $ScriptDir "r177_example_pcb_apply_drc_save_workflow.ps1"
    $na = "N/A"
    if (-not (Test-Path -LiteralPath $r177Script)) {
        Set-PcbApplyDrcSaveWorkflowCiSummary -Ok "false" -ExitCode -1 -Green $na -PreviewOk $na `
            -ApplyConfirmRequiredOk $na -ApplyWithDrcAdvisoryOk $na -PostApplyDrcSummaryPresentOk $na `
            -PostApplySyncReadinessOk $na -SaveConfirmRequiredOk $na -SaveConfirmedOk $na -PostSaveVerifyOk $na `
            -Blocker "r177_script_missing"
        return
    }
    $child = Invoke-IpcQaChildScript -ScriptPath $r177Script -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName "r177_pcb_apply_drc_save_workflow" -TargetRunDirSource StdoutOnly
    $r177TargetRunDir = $child.TargetRunDir
    $r177SummaryPath = if ($r177TargetRunDir -and $r177TargetRunDir -ne "absent") {
        Join-Path $r177TargetRunDir "qa_r177_pcb_apply_drc_save_workflow_summary.txt"
    } else {
        ""
    }
    $readField = {
        param($Key)
        if (-not (Test-Path -LiteralPath $r177SummaryPath)) { return $na }
        $v = Read-SummaryField -SummaryPath $r177SummaryPath -Key $Key
        if ($v) { return $v }
        return $na
    }
    if ($child.ExitCode -ne 0) {
        $failBlocker = "r177_exit_code=$($child.ExitCode)"
        if (Test-Path -LiteralPath $r177SummaryPath) {
            $b = Read-SummaryField -SummaryPath $r177SummaryPath -Key "exact_blocker_if_any"
            if ($b) { $failBlocker = $b }
        }
        Set-PcbApplyDrcSaveWorkflowCiSummary -Ok "false" -ExitCode $child.ExitCode `
            -Green $( & $readField "r177_workflow_green" ) `
            -PreviewOk $( & $readField "preview_ok" ) `
            -ApplyConfirmRequiredOk $( & $readField "apply_confirm_required_ok" ) `
            -ApplyWithDrcAdvisoryOk $( & $readField "apply_with_drc_advisory_ok" ) `
            -PostApplyDrcSummaryPresentOk $( & $readField "post_apply_drc_summary_present_ok" ) `
            -PostApplySyncReadinessOk $( & $readField "post_apply_sync_readiness_ok" ) `
            -SaveConfirmRequiredOk $( & $readField "save_confirm_required_ok" ) `
            -SaveConfirmedOk $( & $readField "save_confirmed_ok" ) `
            -PostSaveVerifyOk $( & $readField "post_save_verify_ok" ) `
            -Blocker $failBlocker
        return
    }
    if (-not (Test-Path -LiteralPath $r177SummaryPath)) {
        Set-PcbApplyDrcSaveWorkflowCiSummary -Ok "false" -ExitCode $child.ExitCode -Green $na -PreviewOk $na `
            -ApplyConfirmRequiredOk $na -ApplyWithDrcAdvisoryOk $na -PostApplyDrcSummaryPresentOk $na `
            -PostApplySyncReadinessOk $na -SaveConfirmRequiredOk $na -SaveConfirmedOk $na -PostSaveVerifyOk $na `
            -Blocker "r177_summary_missing"
        return
    }
    $r177Green = Read-SummaryField -SummaryPath $r177SummaryPath -Key "r177_workflow_green"
    $blocker = Read-SummaryField -SummaryPath $r177SummaryPath -Key "exact_blocker_if_any"
    $obsOk = if ($r177Green -eq "true") { "true" } else { "false" }
    Set-PcbApplyDrcSaveWorkflowCiSummary -Ok $obsOk -ExitCode $child.ExitCode `
        -Green $(if ($r177Green) { $r177Green } else { $na }) `
        -PreviewOk $( & $readField "preview_ok" ) `
        -ApplyConfirmRequiredOk $( & $readField "apply_confirm_required_ok" ) `
        -ApplyWithDrcAdvisoryOk $( & $readField "apply_with_drc_advisory_ok" ) `
        -PostApplyDrcSummaryPresentOk $( & $readField "post_apply_drc_summary_present_ok" ) `
        -PostApplySyncReadinessOk $( & $readField "post_apply_sync_readiness_ok" ) `
        -SaveConfirmRequiredOk $( & $readField "save_confirm_required_ok" ) `
        -SaveConfirmedOk $( & $readField "save_confirmed_ok" ) `
        -PostSaveVerifyOk $( & $readField "post_save_verify_ok" ) `
        -Blocker $(if ($blocker) { $blocker } else { "N/A" })
}

function Set-SchAuthoringAdvisoryCiSummary {
    param(
        [string]$Ok,
        [int]$ExitCode,
        [string]$TargetRunDir,
        [string]$SummaryPath,
        [string]$WarningEmitted,
        [string]$RecommendedAction,
        [string]$Blocker = "N/A"
    )
    $sum.sch_authoring_advisory_report_ok = $Ok
    $sum.sch_authoring_advisory_report_exit_code = [string]$ExitCode
    $sum.sch_authoring_advisory_report_target_run_dir = if ($TargetRunDir) { $TargetRunDir } else { "N/A" }
    $sum.sch_authoring_advisory_report_summary_path = if ($SummaryPath) { $SummaryPath } else { "" }
    $sum.sch_authoring_advisory_warning_emitted = $WarningEmitted
    $sum.sch_authoring_advisory_recommended_action = $RecommendedAction
    $sum.sch_authoring_advisory_report_blocker = $Blocker
}

function Invoke-SchAuthoringAdvisoryReport {
    # R106 — observability-only ; n'affecte pas ci_baseline_green.
    param(
        [Parameter(Mandatory = $true)]
        [string]$CheckpointTargetRunDir
    )
    $reportScript = Join-Path $ScriptDir "report_sch_authoring_advisory.ps1"
    if (-not (Test-Path -LiteralPath $reportScript)) {
        Set-SchAuthoringAdvisoryCiSummary -Ok "false" -ExitCode -1 -TargetRunDir "N/A" -SummaryPath "" `
            -WarningEmitted "N/A" -RecommendedAction "N/A" -Blocker "report_script_missing"
        return
    }
    $child = Invoke-IpcQaChildScript -ScriptPath $reportScript -BuildDir $BuildDir `
        -ExtraArgs @('-CheckpointRunDir', "`"$CheckpointTargetRunDir`"") `
        -LogDir $runDir -LogBaseName "report_sch_authoring_advisory" -TargetRunDirSource StdoutOnly
    $reportTargetRunDir = $child.TargetRunDir
    $reportSummaryPath = if ($reportTargetRunDir -and $reportTargetRunDir -ne "absent") {
        Join-Path $reportTargetRunDir "qa_sch_authoring_advisory_report_summary.txt"
    } else {
        ""
    }
    if ($child.ExitCode -ne 0) {
        Set-SchAuthoringAdvisoryCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
            -SummaryPath $reportSummaryPath -WarningEmitted "N/A" -RecommendedAction "N/A" `
            -Blocker "report_exit_code=$($child.ExitCode)"
        return
    }
    if (-not (Test-Path -LiteralPath $reportSummaryPath)) {
        Set-SchAuthoringAdvisoryCiSummary -Ok "false" -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
            -SummaryPath $reportSummaryPath -WarningEmitted "N/A" -RecommendedAction "N/A" `
            -Blocker "report_summary_missing"
        return
    }
    $warningEmitted = Read-SummaryField -SummaryPath $reportSummaryPath -Key "advisory_warning_emitted"
    $recommendedAction = Read-SummaryField -SummaryPath $reportSummaryPath -Key "recommended_action"
    Set-SchAuthoringAdvisoryCiSummary -Ok "true" -ExitCode $child.ExitCode -TargetRunDir $reportTargetRunDir `
        -SummaryPath $reportSummaryPath -WarningEmitted $(if ($warningEmitted) { $warningEmitted } else { "N/A" }) `
        -RecommendedAction $(if ($recommendedAction) { $recommendedAction } else { "N/A" })
}

function Set-IpcPathIdempotenceCiSummary {
    param([pscustomobject]$Guard)
    $sum.ipc_path_idempotence_ok = $Guard.Ok
    $sum.ipc_path_idempotence_target_run_dir = $Guard.TargetRunDir
    $sum.ipc_path_idempotence_exit_code = $Guard.ExitCode
    $sum.ipc_path_idempotence_summary_path = $Guard.SummaryPath
}

# --- Summary accumulator ---
$sum = [ordered]@{
    build_ok                      = "false"
    msvc_toolchain_ok             = "false"
    toolchain_missing_binary      = "N/A"
    kiface_smoke_ok               = "N/A"
    pcb_measure_distance_ok       = "false"
    pcb_item_geometry_ok          = "false"
    pcb_geometry_v1_ok            = "false"
    pcb_negative_contracts_v1_ok  = "false"
    mcp_v2_checkpoint_green       = "N/A"
    exact_blocker_if_any          = "N/A"
    ipc_smoke_retry_max_note      = "N/A"
    ci_baseline_green             = "false"
    target_run_dir                = $runDir
    artifact_root                 = $runDir
    ci_started_at                 = ""
    ci_finished_at                = ""
    ci_elapsed_ms                 = "N/A"
    build_elapsed_ms              = "N/A"
    checkpoint_elapsed_ms         = "N/A"
    checkpoint_family_count       = "N/A"
    checkpoint_target_run_dir     = "N/A"
    checkpoint_ipc_retry_max_across_families = "N/A"
    vsdevcmd_log                  = (Join-Path $runDir "toolchain_verify.log")
    kiface_smoke_artifact         = ""
    pcb_measure_summary_path      = ""
    pcb_item_geom_summary_path    = ""
    pcb_geometry_v1_summary_path  = ""
    pcb_negative_contracts_v1_summary_path = ""
    pcb_negative_contracts_v1_target_run_dir = "N/A"
    pcb_negative_contracts_v1_exit_code = "N/A"
    checkpoint_summary_path       = ""
    ipc_path_idempotence_ok       = "N/A"
    ipc_path_idempotence_target_run_dir = "N/A"
    ipc_path_idempotence_exit_code = "N/A"
    ipc_path_idempotence_summary_path = ""
    sch_authoring_advisory_report_ok = "N/A"
    sch_authoring_advisory_report_exit_code = "N/A"
    sch_authoring_advisory_report_target_run_dir = "N/A"
    sch_authoring_advisory_report_summary_path = ""
    sch_authoring_advisory_warning_emitted = "N/A"
    sch_authoring_advisory_recommended_action = "N/A"
    sch_authoring_advisory_report_blocker = "N/A"
    sch_authoring_advisory_scan_ok = "N/A"
    sch_authoring_advisory_scan_exit_code = "N/A"
    sch_authoring_advisory_scan_target_run_dir = "N/A"
    sch_authoring_advisory_scan_summary_path = ""
    sch_authoring_advisory_scan_warning_emitted = "N/A"
    sch_authoring_advisory_scan_recommended_action = "N/A"
    sch_authoring_advisory_scan_checkpoints_scanned = "N/A"
    sch_authoring_advisory_scan_checkpoints_with_advisory = "N/A"
    sch_authoring_advisory_scan_kicad_exited_count = "N/A"
    sch_authoring_advisory_scan_mutation_retry_extra_total_sum = "N/A"
    sch_authoring_advisory_scan_restore_retry_extra_total_sum = "N/A"
    sch_authoring_advisory_scan_post_mutation_ping_retry_extra_total_sum = "N/A"
    sch_authoring_advisory_scan_blocker = "N/A"
    sch_authoring_advisory_scan_last_n = "N/A"
    runtime_footprint_workflow_ok = "N/A"
    runtime_footprint_workflow_exit_code = "N/A"
    runtime_footprint_workflow_target_run_dir = "N/A"
    runtime_footprint_workflow_summary_path = ""
    runtime_footprint_workflow_green = "N/A"
    runtime_footprint_workflow_selected_workflow = "N/A"
    runtime_footprint_workflow_runtime_status_read_ok = "N/A"
    runtime_footprint_workflow_hierarchy_assign_instance_safe_enabled = "N/A"
    runtime_footprint_workflow_assign_hierarchy_dry_run_ok = "N/A"
    runtime_footprint_workflow_mutation_performed = "N/A"
    runtime_footprint_workflow_blocker = "N/A"
    footprint_workflow_discovery_consistency_ok = "N/A"
    footprint_workflow_discovery_consistency_exit_code = "N/A"
    footprint_workflow_discovery_consistency_target_run_dir = "N/A"
    footprint_workflow_discovery_consistency_summary_path = ""
    footprint_workflow_discovery_consistency_green = "N/A"
    footprint_workflow_discovery_consistency_snapshot_status_ok = "N/A"
    footprint_workflow_discovery_consistency_readiness_status_ok = "N/A"
    footprint_workflow_discovery_consistency_scope_match_ok = "N/A"
    footprint_workflow_discovery_consistency_agent_primary_source = "N/A"
    footprint_workflow_discovery_consistency_agent_local_source = "N/A"
    footprint_workflow_discovery_consistency_on_divergence = "N/A"
    footprint_workflow_discovery_consistency_blocker = "N/A"
    pcb_apply_drc_save_workflow_ok = "N/A"
    pcb_apply_drc_save_workflow_exit_code = "N/A"
    pcb_apply_drc_save_workflow_green = "N/A"
    pcb_apply_drc_save_workflow_preview_ok = "N/A"
    pcb_apply_drc_save_workflow_apply_confirm_required_ok = "N/A"
    pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok = "N/A"
    pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok = "N/A"
    pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok = "N/A"
    pcb_apply_drc_save_workflow_save_confirm_required_ok = "N/A"
    pcb_apply_drc_save_workflow_save_confirmed_ok = "N/A"
    pcb_apply_drc_save_workflow_post_save_verify_ok = "N/A"
    pcb_apply_drc_save_workflow_blocker = "N/A"
    verify_only                   = if ($VerifyOnly) { "true" } else { "false" }
}

$failMessage = $null
$verifyExitOk = $false
$toolchainMissingBinary = "N/A"

try {
    $sum.ci_started_at = $ciBaselineStartedAt

    # A — Pré-check processus
    $running = New-Object System.Collections.ArrayList
    foreach ($n in @("kicad", "qa_ipc_smoke")) {
        $procs = Get-Process -Name $n -ErrorAction SilentlyContinue
        if ($procs) {
            foreach ($pr in $procs) { [void]$running.Add("${n}:pid=$($pr.Id)") }
        }
    }
    if ($running.Count -gt 0) {
        if ($KillRunningKiCad) {
            Stop-KiCadProcesses
            Start-Sleep -Seconds 2
        }
        else {
            throw "Blocking processes: " + ($running -join "; ") + ". Close KiCad/qa_ipc_smoke or pass -KillRunningKiCad."
        }
    }

    $vsDevCmd = Get-VsDevCmdPath
    if (-not (Test-Path -LiteralPath $vsDevCmd)) {
        throw "VsDevCmd.bat not found. Install Visual Studio 2022 Build Tools or full VS."
    }

    # B — Environnement MSVC (always verify when not VerifyOnly-only skip... always verify toolchain before build)
    Test-MsvcToolchainArtifacts -VsDevCmdPath $vsDevCmd -LogPath $sum.vsdevcmd_log
    $sum.msvc_toolchain_ok = "true"

    if ($VerifyOnly) {
        $missingBins = Test-IpcBaselineBinariesPresent -Root $BuildDir
        if ($missingBins.Count -gt 0) {
            throw "VerifyOnly: missing binaries: " + ($missingBins -join "; ")
        }
        $sum.build_ok = "verify_only_binaries_present"
        if ($EnableKifaceSmoke) {
            $null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $repoRoot
            $smokeRes = Test-IpcKifaceDllLoad -BuildDir $BuildDir -ArtifactsDir $runDir
            $sum.kiface_smoke_ok = if ($smokeRes.AllOk) { "true" } else { "false" }
            $sum.kiface_smoke_artifact = Join-Path $runDir "qa_kiface_dll_smoke.txt"
            if (-not $smokeRes.AllOk) {
                throw "Test-IpcKifaceDllLoad failed (VerifyOnly)."
            }
        }
        else {
            $sum.kiface_smoke_ok = "skipped"
        }
        # R87/R88 — VerifyOnly : garde-fou PATH commun (pas de probes/checkpoint).
        Set-IpcPathIdempotenceCiSummary -Guard (Invoke-IpcPathIdempotenceGuard -BuildDir $BuildDir -IpcToolsDir $ScriptDir -LogDir $runDir)
        $sum.exact_blocker_if_any = "N/A"
        $sum.ci_baseline_green = "true"
        $sum.pcb_measure_distance_ok = "N/A"
        $sum.pcb_item_geometry_ok = "N/A"
        $sum.pcb_geometry_v1_ok = "N/A"
        $sum.pcb_negative_contracts_v1_ok = "N/A"
        $sum.pcb_negative_contracts_v1_target_run_dir = "N/A"
        $sum.pcb_negative_contracts_v1_exit_code = "N/A"
        $sum.mcp_v2_checkpoint_green = "N/A"
        $buildElapsedMsStr = "verify_only"
        $sum.checkpoint_elapsed_ms = "N/A"
        $sum.checkpoint_family_count = "N/A"
        $sum.checkpoint_target_run_dir = "N/A"
        $sum.checkpoint_ipc_retry_max_across_families = "N/A"
        $failMessage = $null
        $verifyExitOk = $true
    }
    else {
        $verifyExitOk = $false

        # C — Build minimal
        if ($BuildMinimal -and -not $SkipBuild) {
            $buildSw = [System.Diagnostics.Stopwatch]::StartNew()
            Push-Location (Resolve-Path $repoRoot)
            try {
                $targets = "kicommon", "pcbnew_kiface", "kicad", "qa_ipc_smoke"
                $targ = $targets -join " "
                $cmd = "`"$vsDevCmd`" -arch=amd64 && cmake --build --preset win64-release --target $targ"
                cmd /c $cmd
                if ($LASTEXITCODE -ne 0) {
                    throw "cmake build failed exit $LASTEXITCODE (fermer KiCad si LNK1104 sur _pcbnew.dll)"
                }
            }
            finally {
                Pop-Location
            }
            $buildSw.Stop()
            $buildElapsedMsStr = [string][int64]$buildSw.ElapsedMilliseconds
            $sum.build_ok = "true"
        }
        elseif ($SkipBuild) {
            $sum.build_ok = "skipped"
            $buildElapsedMsStr = "skipped"
        }
        else {
            $sum.build_ok = "not_requested"
            $buildElapsedMsStr = "not_requested"
        }

    $missingBins = Test-IpcBaselineBinariesPresent -Root $BuildDir
    if ($missingBins.Count -gt 0) {
        throw "Missing baseline binaries: " + ($missingBins -join "; ")
    }

    $null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $repoRoot

    # D — Smoke KIFACE
    if ($EnableKifaceSmoke) {
        $smokeRes = Test-IpcKifaceDllLoad -BuildDir $BuildDir -ArtifactsDir $runDir
        $sum.kiface_smoke_ok = if ($smokeRes.AllOk) { "true" } else { "false" }
        $sum.kiface_smoke_artifact = Join-Path $runDir "qa_kiface_dll_smoke.txt"
        if (-not $smokeRes.AllOk) {
            throw "Test-IpcKifaceDllLoad failed."
        }
    }
    else {
        $sum.kiface_smoke_ok = "skipped"
    }

    # D2 — Garde-fou PATH idempotent (R87/R88, avant probes PCB)
    Set-IpcPathIdempotenceCiSummary -Guard (Invoke-IpcPathIdempotenceGuard -BuildDir $BuildDir -IpcToolsDir $ScriptDir -LogDir $runDir)

    # E — Probes critiques
    $measureProbe = Join-Path $ScriptDir "targeted_pcb_measure_distance_probe.ps1"
    $rMeas = Invoke-ProbeCaptureTargetDir -ProbeScriptPath $measureProbe
    if ($rMeas.ExitCode -ne 0) {
        throw "targeted_pcb_measure_distance_probe.ps1 failed exit $($rMeas.ExitCode)"
    }
    $measSummary = Join-Path $rMeas.TargetRunDir "qa_pcb_measure_distance_session_contract_summary.txt"
    $sum.pcb_measure_summary_path = $measSummary
    $sum.pcb_measure_distance_ok = Read-SummaryField -SummaryPath $measSummary -Key "pcb_measure_distance_green"

    $itemProbe = Join-Path $ScriptDir "targeted_pcb_item_geometry_probe.ps1"
    $rItem = Invoke-ProbeCaptureTargetDir -ProbeScriptPath $itemProbe
    if ($rItem.ExitCode -ne 0) {
        throw "targeted_pcb_item_geometry_probe.ps1 failed exit $($rItem.ExitCode)"
    }
    $itemSummary = Join-Path $rItem.TargetRunDir "qa_pcb_item_geometry_session_contract_summary.txt"
    $sum.pcb_item_geom_summary_path = $itemSummary
    $sum.pcb_item_geometry_ok = Read-SummaryField -SummaryPath $itemSummary -Key "pcb_item_geometry_green"

    # F — Famille pcb_geometry v1
    $geomFam = Join-Path $ScriptDir "run_pcb_geometry_v1_targeted_probes.ps1"
    $rFam = Invoke-ProbeCaptureTargetDir -ProbeScriptPath $geomFam
    if ($rFam.ExitCode -ne 0) {
        throw "run_pcb_geometry_v1_targeted_probes.ps1 failed exit $($rFam.ExitCode)"
    }
    $famSummary = Join-Path $rFam.TargetRunDir "qa_pcb_geometry_v1_family_summary.txt"
    $sum.pcb_geometry_v1_summary_path = $famSummary
    $pg = Read-SummaryField -SummaryPath $famSummary -Key "pcb_geometry_v1_family_green"
    $sum.pcb_geometry_v1_ok = $pg
    $imax = Read-SummaryField -SummaryPath $famSummary -Key "ipc_smoke_ipc_retry_extra_total_measure"
    $imax2 = Read-SummaryField -SummaryPath $famSummary -Key "ipc_smoke_ipc_retry_extra_total_item_geometry"
    if ($imax -or $imax2) {
        $sum.ipc_smoke_retry_max_note = "measure=$imax;item_geometry=$imax2"
    }

    # F2 — Famille pcb_negative_contracts v1 (garde-fou contrats board_filename / max_items, R65)
    $negFam = Join-Path $ScriptDir "run_pcb_negative_contracts_v1_targeted_probes.ps1"
    $rNeg = Invoke-ProbeCaptureTargetDir -ProbeScriptPath $negFam
    if ($rNeg.ExitCode -ne 0) {
        throw "run_pcb_negative_contracts_v1_targeted_probes.ps1 failed exit $($rNeg.ExitCode)"
    }
    $negFamSummary = Join-Path $rNeg.TargetRunDir "qa_pcb_negative_contracts_v1_family_summary.txt"
    $sum.pcb_negative_contracts_v1_summary_path = $negFamSummary
    $sum.pcb_negative_contracts_v1_target_run_dir = $rNeg.TargetRunDir
    $sum.pcb_negative_contracts_v1_exit_code = Read-SummaryField -SummaryPath $negFamSummary -Key "pcb_negative_contracts_exit_code"
    $negOk = Read-SummaryField -SummaryPath $negFamSummary -Key "pcb_negative_contracts_v1_family_green"
    $sum.pcb_negative_contracts_v1_ok = $negOk
    if ($negOk -ne "true") {
        $negBlocker = Read-SummaryField -SummaryPath $negFamSummary -Key "exact_blocker_if_any"
        throw "pcb_negative_contracts_v1 not green (exact_blocker_if_any=$negBlocker)"
    }

    # G — Checkpoint MCP v2
    if (-not $SkipCheckpoint) {
        if ($EnableKifaceSmoke) {
            $env:KICAD_IPC_KIFACE_SMOKE = "1"
        }
        else {
            Remove-Item Env:KICAD_IPC_KIFACE_SMOKE -ErrorAction SilentlyContinue
        }
        $ckScript = Join-Path $ScriptDir "run_mcp_v2_checkpoint.ps1"
        $rCk = Invoke-ProbeCaptureTargetDir -ProbeScriptPath $ckScript
        if ($rCk.ExitCode -ne 0) {
            throw "run_mcp_v2_checkpoint.ps1 failed exit $($rCk.ExitCode)"
        }
        $ckSummaryPath = Join-Path $rCk.TargetRunDir "qa_mcp_v2_checkpoint_summary.txt"
        $sum.checkpoint_summary_path = $ckSummaryPath
        $sum.checkpoint_target_run_dir = $rCk.TargetRunDir
        $sum.checkpoint_family_count = "9"
        $sum.mcp_v2_checkpoint_green = Read-SummaryField -SummaryPath $ckSummaryPath -Key "mcp_v2_checkpoint_green"
        $sum.exact_blocker_if_any = Read-SummaryField -SummaryPath $ckSummaryPath -Key "exact_blocker_if_any"
        $sum.checkpoint_elapsed_ms = Read-SummaryField -SummaryPath $ckSummaryPath -Key "checkpoint_elapsed_ms"
        $ipcMax = Read-SummaryField -SummaryPath $ckSummaryPath -Key "ipc_smoke_ipc_retry_extra_total_max_across_families"
        $sum.checkpoint_ipc_retry_max_across_families = if ($ipcMax) { $ipcMax } else { "N/A" }
        if ($ipcMax) {
            $sum.ipc_smoke_retry_max_note = if ($sum.ipc_smoke_retry_max_note -eq "N/A") { $ipcMax } else { $sum.ipc_smoke_retry_max_note + ";checkpoint_max=$ipcMax" }
        }
        if ($sum.mcp_v2_checkpoint_green -ne "true") {
            throw "Checkpoint not green (exact_blocker_if_any=$($sum.exact_blocker_if_any))."
        }

        # G2 — Report advisory SCH authoring (R106, observability-only ; hors ci_baseline_green)
        Invoke-SchAuthoringAdvisoryReport -CheckpointTargetRunDir $rCk.TargetRunDir

        # G3 — Scan advisory multi-checkpoint (R108/R110, observability-only ; hors ci_baseline_green)
        $sum.sch_authoring_advisory_scan_last_n = [string]$SchAuthoringAdvisoryScanLastN
        Invoke-SchAuthoringAdvisoryScan -ScanLastN $SchAuthoringAdvisoryScanLastN

        # G4 — Runtime footprint workflow R163 (R164, observability-only ; hors ci_baseline_green)
        Invoke-RuntimeFootprintWorkflowReport

        # G5 — Dual-source footprint workflow consistency (R167, observability-only ; hors ci_baseline_green)
        Invoke-FootprintWorkflowDiscoveryConsistencyReport

        # G6 — PCB apply DRC save workflow R177 (R178, observability-only ; hors ci_baseline_green)
        Invoke-PcbApplyDrcSaveWorkflowReport
    }
    else {
        $sum.mcp_v2_checkpoint_green = "skipped"
        $sum.exact_blocker_if_any = "N/A"
        $sum.checkpoint_elapsed_ms = "skipped"
        $sum.checkpoint_family_count = "skipped"
        $sum.checkpoint_target_run_dir = "skipped"
        $sum.checkpoint_ipc_retry_max_across_families = "skipped"
        $sum.sch_authoring_advisory_report_ok = "skipped"
        $sum.sch_authoring_advisory_report_exit_code = "N/A"
        $sum.sch_authoring_advisory_report_target_run_dir = "skipped"
        $sum.sch_authoring_advisory_report_summary_path = ""
        $sum.sch_authoring_advisory_warning_emitted = "skipped"
        $sum.sch_authoring_advisory_recommended_action = "skipped"
        $sum.sch_authoring_advisory_report_blocker = "N/A"
        $sum.sch_authoring_advisory_scan_ok = "skipped"
        $sum.sch_authoring_advisory_scan_exit_code = "N/A"
        $sum.sch_authoring_advisory_scan_target_run_dir = "skipped"
        $sum.sch_authoring_advisory_scan_summary_path = ""
        $sum.sch_authoring_advisory_scan_warning_emitted = "skipped"
        $sum.sch_authoring_advisory_scan_recommended_action = "skipped"
        $sum.sch_authoring_advisory_scan_checkpoints_scanned = "skipped"
        $sum.sch_authoring_advisory_scan_checkpoints_with_advisory = "skipped"
        $sum.sch_authoring_advisory_scan_kicad_exited_count = "skipped"
        $sum.sch_authoring_advisory_scan_mutation_retry_extra_total_sum = "skipped"
        $sum.sch_authoring_advisory_scan_restore_retry_extra_total_sum = "skipped"
        $sum.sch_authoring_advisory_scan_post_mutation_ping_retry_extra_total_sum = "skipped"
        $sum.sch_authoring_advisory_scan_blocker = "N/A"
        $sum.sch_authoring_advisory_scan_last_n = "skipped"
        $sum.runtime_footprint_workflow_ok = "skipped"
        $sum.runtime_footprint_workflow_exit_code = "N/A"
        $sum.runtime_footprint_workflow_target_run_dir = "skipped"
        $sum.runtime_footprint_workflow_summary_path = ""
        $sum.runtime_footprint_workflow_green = "skipped"
        $sum.runtime_footprint_workflow_selected_workflow = "skipped"
        $sum.runtime_footprint_workflow_runtime_status_read_ok = "skipped"
        $sum.runtime_footprint_workflow_hierarchy_assign_instance_safe_enabled = "skipped"
        $sum.runtime_footprint_workflow_assign_hierarchy_dry_run_ok = "skipped"
        $sum.runtime_footprint_workflow_mutation_performed = "skipped"
        $sum.runtime_footprint_workflow_blocker = "N/A"
        $sum.footprint_workflow_discovery_consistency_ok = "skipped"
        $sum.footprint_workflow_discovery_consistency_exit_code = "N/A"
        $sum.footprint_workflow_discovery_consistency_target_run_dir = "skipped"
        $sum.footprint_workflow_discovery_consistency_summary_path = ""
        $sum.footprint_workflow_discovery_consistency_green = "skipped"
        $sum.footprint_workflow_discovery_consistency_snapshot_status_ok = "skipped"
        $sum.footprint_workflow_discovery_consistency_readiness_status_ok = "skipped"
        $sum.footprint_workflow_discovery_consistency_scope_match_ok = "skipped"
        $sum.footprint_workflow_discovery_consistency_agent_primary_source = "skipped"
        $sum.footprint_workflow_discovery_consistency_agent_local_source = "skipped"
        $sum.footprint_workflow_discovery_consistency_on_divergence = "skipped"
        $sum.footprint_workflow_discovery_consistency_blocker = "N/A"
        $sum.pcb_apply_drc_save_workflow_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_exit_code = "N/A"
        $sum.pcb_apply_drc_save_workflow_green = "skipped"
        $sum.pcb_apply_drc_save_workflow_preview_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_apply_confirm_required_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_apply_with_drc_advisory_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_post_apply_drc_summary_present_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_post_apply_sync_readiness_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_save_confirm_required_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_save_confirmed_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_post_save_verify_ok = "skipped"
        $sum.pcb_apply_drc_save_workflow_blocker = "N/A"
    }

        $sum.ci_baseline_green = "true"
    }
}
catch {
    $failMessage = $_.Exception.Message
    if ($toolchainMissingBinary -ne "N/A") {
        $sum.toolchain_missing_binary = $toolchainMissingBinary
    }
    if ($sum.exact_blocker_if_any -eq "N/A" -or [string]::IsNullOrWhiteSpace($sum.exact_blocker_if_any)) {
        $sum.exact_blocker_if_any = $failMessage
    }
    $sum.ci_baseline_green = "false"
}

$ciBaselineSw.Stop()
$sum.ci_finished_at = [DateTimeOffset]::UtcNow.ToString("o")
$sum.ci_elapsed_ms = [string][int64]$ciBaselineSw.ElapsedMilliseconds
if ($buildElapsedMsStr -eq "not_run_yet") {
    $sum.build_elapsed_ms = "N/A"
} else {
    $sum.build_elapsed_ms = $buildElapsedMsStr
}

$summaryPath = Join-Path $runDir "qa_mcp_v2_ci_baseline_summary.txt"
$lines = @()
foreach ($k in $sum.Keys) {
    $lines += "$k=$($sum[$k])"
}
$lines += "artifact_probe_logs=$runDir"
$lines | Set-Content -Path $summaryPath -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
Write-Output ("SUMMARY_FILE=" + $summaryPath)

if ($failMessage) {
    Write-Error $failMessage
    exit 1
}

Write-Output "MCP_V2_CI_BASELINE_OK=1"
exit 0
