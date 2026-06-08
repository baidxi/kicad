param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R166 — Read-only dual-source consistency: project_readiness_summary vs schematic_footprint_readiness_session
# workflow discovery fields (R162/R165). Standalone: exit 1 if divergence. Not a MCP v2 gate.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$hierProj = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_pro"
$hierRootSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_sch"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $hierProj)) { throw "Missing hierarchy project $hierProj" }
if (!(Test-Path $hierRootSch)) { throw "Missing hierarchy schematic $hierRootSch" }

$hierRootSchAbs = (Resolve-Path $hierRootSch).Path
$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-footprint-workflow-discovery-consistency")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot (Join-Path $BuildDir "ipc-v1-config") -EnableAutomation

$hierFpLibTable = Join-Path (Split-Path $hierRootSch) "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R166 workflow discovery consistency"))'),
    ")"
) | Set-Content -Path $hierFpLibTable -Encoding ASCII

$WorkflowFieldKeys = @(
    "hierarchy_assign_instance_safe_enabled",
    "recommended_footprint_assignment_workflow",
    "fallback_footprint_assignment_workflow",
    "recommended_assign_tool",
    "recommended_assign_scope"
)

function Parse-FlattenLine {
    param([string]$Text, [string]$Prefix, [string]$FieldKey)
    $fullKey = $Prefix + $FieldKey
    if ($Text -match ('(?<![a-z_])' + [regex]::Escape($fullKey) + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) {
        return $Matches[1].Trim()
    }
    return "absent"
}

function Parse-SnapshotWorkflowFields {
    param([string]$Text)
    $out = @{}
    foreach ($k in $WorkflowFieldKeys) {
        $out[$k] = Parse-FlattenLine -Text $Text -Prefix "readiness_" -FieldKey $k
    }
    $out["status_ok"] = ($Text -match 'status=1')
    $out["project_readiness_summary_present"] = Parse-FlattenLine -Text $Text -Prefix "" -FieldKey "project_readiness_summary_present"
    return $out
}

function Parse-ReadinessWorkflowFields {
    param([string]$Text)
    $out = @{}
    foreach ($k in $WorkflowFieldKeys) {
        $out[$k] = Parse-FlattenLine -Text $Text -Prefix "footprint_readiness_" -FieldKey $k
    }
    $out["status_ok"] = ($Text -match 'status=1')
    $out["footprint_readiness_ok"] = Parse-FlattenLine -Text $Text -Prefix "" -FieldKey "footprint_readiness_ok"
    return $out
}

function Invoke-McpTool {
    param(
        [string]$ApiUrl,
        [string]$ToolName,
        [hashtable]$ToolArgs,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", $ToolName, ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return $out
}

function Test-WorkflowFieldsReadable {
    param([hashtable]$Fields)
    foreach ($k in $WorkflowFieldKeys) {
        if ($Fields[$k] -eq "absent" -or [string]::IsNullOrWhiteSpace($Fields[$k])) {
            return $false
        }
    }
    return $true
}

$consistencyGreen = "false"
$snapshotRuntimeStatusReadOk = "false"
$readinessRuntimeStatusReadOk = "false"
$hierarchyAssignMatchOk = "false"
$recommendedWorkflowMatchOk = "false"
$fallbackWorkflowMatchOk = "false"
$recommendedToolMatchOk = "false"
$recommendedScopeMatchOk = "false"
$snapshotRecommendedAssignScope = "absent"
$readinessRecommendedAssignScope = "absent"
$finalBlocker = "N/A"

Stop-KiCadProcesses
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru

try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r166" }
    $url = $resolved.Url

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r166"
    }

    $openRootArgs = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_bootstrap_root.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_bootstrap_root_failed" }

    $snapshotOut = Invoke-McpTool -ApiUrl $url -ToolName "get_project_context_snapshot" -ToolArgs @{} -LogBaseName "r166_project_context_snapshot"
    if ($snapshotOut -notmatch 'status=1') { throw "r166_snapshot_failed" }
    $snap = Parse-SnapshotWorkflowFields -Text $snapshotOut

    $readinessOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_footprint_readiness_session" -ToolArgs @{
        schematic_filename  = $hierRootSchAbs
        scope               = "hierarchy"
        include_sheet_paths = $true
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $false
    } -LogBaseName "r166_footprint_readiness_hierarchy"
    if ($readinessOut -notmatch 'status=1') { throw "r166_footprint_readiness_failed" }
    $ready = Parse-ReadinessWorkflowFields -Text $readinessOut

    if ($snap["status_ok"] -and $snap["project_readiness_summary_present"] -eq "1" `
            -and (Test-WorkflowFieldsReadable -Fields $snap)) {
        $snapshotRuntimeStatusReadOk = "true"
    }

    if ($ready["status_ok"] -and $ready["footprint_readiness_ok"] -eq "true" `
            -and (Test-WorkflowFieldsReadable -Fields $ready)) {
        $readinessRuntimeStatusReadOk = "true"
    }

    $snapshotRecommendedAssignScope = $snap["recommended_assign_scope"]
    $readinessRecommendedAssignScope = $ready["recommended_assign_scope"]

    if ($snap["hierarchy_assign_instance_safe_enabled"] -eq $ready["hierarchy_assign_instance_safe_enabled"]) {
        $hierarchyAssignMatchOk = "true"
    }
    if ($snap["recommended_footprint_assignment_workflow"] -eq $ready["recommended_footprint_assignment_workflow"]) {
        $recommendedWorkflowMatchOk = "true"
    }
    if ($snap["fallback_footprint_assignment_workflow"] -eq $ready["fallback_footprint_assignment_workflow"]) {
        $fallbackWorkflowMatchOk = "true"
    }
    if ($snap["recommended_assign_tool"] -eq $ready["recommended_assign_tool"]) {
        $recommendedToolMatchOk = "true"
    }
    if ($snap["recommended_assign_scope"] -eq $ready["recommended_assign_scope"]) {
        $recommendedScopeMatchOk = "true"
    }

    if ($snapshotRuntimeStatusReadOk -ne "true") { $finalBlocker = "snapshot_runtime_status_read_failed" }
    elseif ($readinessRuntimeStatusReadOk -ne "true") { $finalBlocker = "readiness_runtime_status_read_failed" }
    elseif ($hierarchyAssignMatchOk -ne "true") { $finalBlocker = "hierarchy_assign_instance_safe_enabled_mismatch" }
    elseif ($recommendedWorkflowMatchOk -ne "true") { $finalBlocker = "recommended_footprint_assignment_workflow_mismatch" }
    elseif ($fallbackWorkflowMatchOk -ne "true") { $finalBlocker = "fallback_footprint_assignment_workflow_mismatch" }
    elseif ($recommendedToolMatchOk -ne "true") { $finalBlocker = "recommended_assign_tool_mismatch" }
    elseif ($recommendedScopeMatchOk -ne "true") { $finalBlocker = "recommended_assign_scope_mismatch" }
    else { $consistencyGreen = "true" }

    $summaryPath = Join-Path $runDir "qa_footprint_workflow_discovery_consistency_summary.txt"
    @(
        "footprint_workflow_discovery_consistency_green=$consistencyGreen"
        "snapshot_runtime_status_read_ok=$snapshotRuntimeStatusReadOk"
        "readiness_runtime_status_read_ok=$readinessRuntimeStatusReadOk"
        "hierarchy_assign_instance_safe_enabled_match_ok=$hierarchyAssignMatchOk"
        "recommended_footprint_assignment_workflow_match_ok=$recommendedWorkflowMatchOk"
        "fallback_footprint_assignment_workflow_match_ok=$fallbackWorkflowMatchOk"
        "recommended_assign_tool_match_ok=$recommendedToolMatchOk"
        "recommended_assign_scope_match_ok=$recommendedScopeMatchOk"
        "snapshot_hierarchy_assign_instance_safe_enabled=$($snap['hierarchy_assign_instance_safe_enabled'])"
        "readiness_hierarchy_assign_instance_safe_enabled=$($ready['hierarchy_assign_instance_safe_enabled'])"
        "snapshot_recommended_footprint_assignment_workflow=$($snap['recommended_footprint_assignment_workflow'])"
        "readiness_recommended_footprint_assignment_workflow=$($ready['recommended_footprint_assignment_workflow'])"
        "snapshot_recommended_assign_scope=$snapshotRecommendedAssignScope"
        "readiness_recommended_assign_scope=$readinessRecommendedAssignScope"
        "agent_primary_source=project_readiness_summary"
        "agent_local_source=schematic_footprint_readiness_session"
        "agent_on_divergence=do_not_mutate_and_recheck_snapshot"
        "exact_blocker_if_any=$finalBlocker"
        "target_run_dir=$runDir"
        "fixture_project=$hierProj"
        "fixture_schematic=$hierRootSchAbs"
    ) | Set-Content -Path $summaryPath -Encoding UTF8

    Write-Output "TARGET_RUN_DIR=$runDir"
    Write-Output "SUMMARY_FILE=$summaryPath"
    if ($consistencyGreen -ne "true") {
        Write-Warning "footprint_workflow_discovery_consistency_failed:$finalBlocker"
        exit 1
    }
}
finally {
    Stop-KiCadProcesses
}
