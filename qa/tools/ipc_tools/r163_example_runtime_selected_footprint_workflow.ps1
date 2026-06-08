param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R163 — Runtime-driven IA workflow: read project_readiness_summary (R162) and auto-select footprint assign workflow.
# Workflow A (default when runtime true): hierarchy instance-safe dry-run assign.
# Workflow B (fallback): navigate_current_sheet_dry_run (R156) — documented, not executed when runtime true.

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
$testFootprint = "Resistor_SMD:R_0201_0603Metric"
$r156FallbackScript = Join-Path $PSScriptRoot "r156_example_navigable_sch_footprint_workflow.ps1"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $hierProj)) { throw "Missing hierarchy project $hierProj" }
if (!(Test-Path $hierRootSch)) { throw "Missing hierarchy schematic $hierRootSch" }
if (!(Test-Path $r156FallbackScript)) { throw "Missing R156 fallback script $r156FallbackScript" }

$hierRootSchAbs = (Resolve-Path $hierRootSch).Path
$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r163-runtime-selected-footprint-workflow")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot (Join-Path $BuildDir "ipc-v1-config") -EnableAutomation

$hierFpLibTable = Join-Path (Split-Path $hierRootSch) "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R163 runtime workflow example"))'),
    ")"
) | Set-Content -Path $hierFpLibTable -Encoding ASCII

function Parse-SnapshotReadinessFields {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                                        = ($Text -match 'status=1')
        project_readiness_summary_present                = (& $parseLine "project_readiness_summary_present")
        hierarchy_assign_instance_safe_enabled           = (& $parseLine "readiness_hierarchy_assign_instance_safe_enabled")
        recommended_footprint_assignment_workflow        = (& $parseLine "readiness_recommended_footprint_assignment_workflow")
        fallback_footprint_assignment_workflow         = (& $parseLine "readiness_fallback_footprint_assignment_workflow")
        recommended_assign_tool                          = (& $parseLine "readiness_recommended_assign_tool")
        recommended_assign_scope                         = (& $parseLine "readiness_recommended_assign_scope")
    }
}

function Parse-ReadinessWithoutFootprintTarget {
    param([string]$OutputText)

    $symbolsStart = $OutputText.IndexOf('key: "symbols"')
    if ($symbolsStart -lt 0) { return $null }
    $symbolsEnd = $OutputText.IndexOf('key: "symbols_count"', $symbolsStart)
    if ($symbolsEnd -lt 0) { return $null }
    $symbolsSection = $OutputText.Substring($symbolsStart, $symbolsEnd - $symbolsStart)

    $chunks = [regex]::Split($symbolsSection, '(?=values\s*\{)')
    foreach ($chunk in $chunks) {
        if ($chunk -notmatch 'key:\s+"has_footprint"[\s\S]*?bool_value:\s*false') { continue }

        $symbolId = ""
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') {
            $symbolId = $Matches[1]
        }
        $instancePath = ""
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') {
            $instancePath = $Matches[1]
        }
        $reference = ""
        if ($chunk -match 'key:\s+"reference"[\s\S]*?string_value:\s*"([^"]+)"') {
            $reference = $Matches[1]
        }
        if ($symbolId -and $instancePath) {
            return [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
                Reference    = $reference
            }
        }
    }
    return $null
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

function Parse-BoolField {
    param([string]$Text, [string]$Key)
    if ($Text -match ('(?<![a-z_])' + [regex]::Escape($Key) + '=(true|false)')) { return $Matches[1] }
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]*?bool_value:\s*(true|false)')) { return $Matches[1] }
    return "false"
}

function Parse-StringField {
    param([string]$Text, [string]$Key)
    if ($Text -match ('(?<![a-z_])' + [regex]::Escape($Key) + '=([^\r\n]+)')) { return $Matches[1].Trim() }
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]*?string_value:\s*"([^"]*)"')) { return $Matches[1] }
    return ""
}

function Parse-AssignFootprintResult {
    param([string]$OutputText)
    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if ($OutputText -match 'assign_footprint_ok=(true|false)') { $ok = $Matches[1] }
    elseif ($OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "absent"
    if ($OutputText -match 'assign_footprint_exact_blocker_if_any=([^\r\n]+)') { $blocker = $Matches[1].Trim() }
    elseif ($OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $scope = "absent"
    if ($OutputText -match 'assign_footprint_scope=([^\r\n]+)') { $scope = $Matches[1].Trim() }
    elseif ($OutputText -match 'key:\s+"scope"[\s\S]*?string_value:\s*"([^"]*)"') { $scope = $Matches[1] }
    $globalUpdate = "absent"
    if ($OutputText -match 'assign_footprint_global_update=(true|false)') { $globalUpdate = $Matches[1] }
    elseif ($OutputText -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*(true|false)') { $globalUpdate = $Matches[1] }
    $instanceSafeWriteUsed = "absent"
    if ($OutputText -match 'key:\s+"instance_safe_write_used"[\s\S]*?bool_value:\s*(true|false)') { $instanceSafeWriteUsed = $Matches[1] }
    return @{
        status_ok = $statusOk; ok = $ok; blocker = $blocker; scope = $scope
        global_update = $globalUpdate; instance_safe_write_used = $instanceSafeWriteUsed
    }
}

$r163Green = "false"
$runtimeStatusReadOk = "false"
$hierarchyAssignInstanceSafeEnabled = "absent"
$recommendedFootprintAssignmentWorkflow = "absent"
$recommendedAssignTool = "absent"
$recommendedAssignScope = "absent"
$fallbackFootprintAssignmentWorkflow = "absent"
$selectedWorkflow = "unselected"
$selectedWorkflowMatchesRuntimeOk = "false"
$readinessHierarchyOk = "false"
$assignHierarchyDryRunOk = "false"
$assignHierarchyGlobalUpdateFalseOk = "false"
$assignHierarchyInstanceSafeWriteOk = "skipped"
$mutationPerformed = "false"
$workflowBFallbackDocumented = "false"
$finalBlocker = "N/A"

$targetInstancePath = ""
$targetSymbolId = ""
$targetReference = ""

Stop-KiCadProcesses
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru

try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r163" }
    $url = $resolved.Url

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r163"
    }

    $openRootArgs = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_bootstrap_root.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_bootstrap_root_failed" }

    # A — Read runtime status from get_project_context_snapshot (R162 fields in project_readiness_summary).
    $snapshotOut = Invoke-McpTool -ApiUrl $url -ToolName "get_project_context_snapshot" -ToolArgs @{} -LogBaseName "r163_project_context_snapshot"
    if ($snapshotOut -notmatch 'status=1') { throw "r163_snapshot_failed" }

    $snap = Parse-SnapshotReadinessFields -Text $snapshotOut
    $hierarchyAssignInstanceSafeEnabled = $snap.hierarchy_assign_instance_safe_enabled
    $recommendedFootprintAssignmentWorkflow = $snap.recommended_footprint_assignment_workflow
    $recommendedAssignTool = $snap.recommended_assign_tool
    $recommendedAssignScope = $snap.recommended_assign_scope
    $fallbackFootprintAssignmentWorkflow = $snap.fallback_footprint_assignment_workflow

    if ($snap.project_readiness_summary_present -eq "1" `
            -and $hierarchyAssignInstanceSafeEnabled -ne "absent" `
            -and $recommendedFootprintAssignmentWorkflow -ne "absent" `
            -and $recommendedAssignTool -ne "absent" `
            -and $recommendedAssignScope -ne "absent" `
            -and $fallbackFootprintAssignmentWorkflow -ne "absent") {
        $runtimeStatusReadOk = "true"
    }

    if ($fallbackFootprintAssignmentWorkflow -eq "navigate_current_sheet_dry_run") {
        $workflowBFallbackDocumented = "true"
    }

    # B — Agent workflow selection from runtime fields.
    $useWorkflowA = ($hierarchyAssignInstanceSafeEnabled -eq "true" -and $recommendedAssignScope -eq "hierarchy")
    if ($useWorkflowA) {
        $selectedWorkflow = "hierarchy_instance_safe"
    }
    else {
        $selectedWorkflow = $fallbackFootprintAssignmentWorkflow
        if ([string]::IsNullOrWhiteSpace($selectedWorkflow) -or $selectedWorkflow -eq "absent") {
            $selectedWorkflow = "navigate_current_sheet_dry_run"
        }
    }

    if ($useWorkflowA -and $selectedWorkflow -eq $recommendedFootprintAssignmentWorkflow) {
        $selectedWorkflowMatchesRuntimeOk = "true"
    }
    elseif (-not $useWorkflowA -and $selectedWorkflow -eq $fallbackFootprintAssignmentWorkflow) {
        $selectedWorkflowMatchesRuntimeOk = "true"
    }

    if ($useWorkflowA) {
        # C — Workflow A: readiness hierarchy + assign hierarchy dry-run (no real mutation).
        if ($recommendedAssignTool -ne "schematic_assign_footprint_session") {
            throw "r163_unexpected_recommended_assign_tool"
        }

        $readinessOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_footprint_readiness_session" -ToolArgs @{
            schematic_filename  = $hierRootSchAbs
            scope               = "hierarchy"
            include_sheet_paths = $true
            include_symbols     = $true
            max_symbols         = 50
            validate_footprints = $false
        } -LogBaseName "r163_readiness_hierarchy"

        if ($readinessOut -match 'status=1') {
            $target = Parse-ReadinessWithoutFootprintTarget -OutputText $readinessOut
            if ($null -ne $target) {
                $targetInstancePath = $target.InstancePath
                $targetSymbolId = $target.SymbolId
                $targetReference = $target.Reference
                $readinessHierarchyOk = "true"
            }
        }

        if ($readinessHierarchyOk -eq "true") {
            $assignOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_assign_footprint_session" -ToolArgs @{
                schematic_filename = $hierRootSchAbs
                symbol_id          = $targetSymbolId
                instance_path      = $targetInstancePath
                footprint          = $testFootprint
                scope              = "hierarchy"
                dry_run            = $true
                allow_overwrite    = $true
                validate_footprint = $false
            } -LogBaseName "r163_assign_hierarchy_dry_run"

            $assignResult = Parse-AssignFootprintResult -OutputText $assignOut
            if ($assignResult.instance_safe_write_used -eq "true") {
                $assignHierarchyInstanceSafeWriteOk = "true"
            }
            elseif ($assignResult.instance_safe_write_used -eq "false") {
                $assignHierarchyInstanceSafeWriteOk = "false"
            }

            if ($assignResult.status_ok -and $assignResult.ok -eq "true" `
                    -and $assignResult.blocker -eq "N/A" -and $assignResult.scope -eq "hierarchy") {
                $assignHierarchyDryRunOk = "true"
            }
            if ($assignResult.global_update -eq "false") {
                $assignHierarchyGlobalUpdateFalseOk = "true"
            }
        }
    }
    else {
        # D — Workflow B fallback path: documented only when runtime recommends hierarchy (R163 default env).
        # Executable fallback remains r156_example_navigable_sch_footprint_workflow.ps1 (separate probe, not duplicated here).
        $readinessHierarchyOk = "skipped_workflow_b"
        $assignHierarchyDryRunOk = "skipped_workflow_b"
        $assignHierarchyGlobalUpdateFalseOk = "skipped_workflow_b"
    }

    if ($runtimeStatusReadOk -ne "true") { $finalBlocker = "runtime_status_read_failed" }
    elseif ($selectedWorkflowMatchesRuntimeOk -ne "true") { $finalBlocker = "selected_workflow_mismatch" }
    elseif ($useWorkflowA -and $readinessHierarchyOk -ne "true") { $finalBlocker = "readiness_hierarchy_failed" }
    elseif ($useWorkflowA -and $assignHierarchyDryRunOk -ne "true") { $finalBlocker = "assign_hierarchy_dry_run_failed" }
    elseif ($useWorkflowA -and $assignHierarchyGlobalUpdateFalseOk -ne "true") { $finalBlocker = "assign_hierarchy_global_update_not_false" }
    elseif ($workflowBFallbackDocumented -ne "true") { $finalBlocker = "fallback_workflow_not_documented" }
    else { $r163Green = "true" }

    $summaryPath = Join-Path $runDir "qa_r163_runtime_selected_footprint_workflow_summary.txt"
    @(
        "r163_workflow_green=$r163Green"
        "runtime_status_read_ok=$runtimeStatusReadOk"
        "hierarchy_assign_instance_safe_enabled=$hierarchyAssignInstanceSafeEnabled"
        "recommended_footprint_assignment_workflow=$recommendedFootprintAssignmentWorkflow"
        "recommended_assign_tool=$recommendedAssignTool"
        "recommended_assign_scope=$recommendedAssignScope"
        "fallback_footprint_assignment_workflow=$fallbackFootprintAssignmentWorkflow"
        "selected_workflow=$selectedWorkflow"
        "selected_workflow_matches_runtime_ok=$selectedWorkflowMatchesRuntimeOk"
        "readiness_hierarchy_ok=$readinessHierarchyOk"
        "assign_hierarchy_dry_run_ok=$assignHierarchyDryRunOk"
        "assign_hierarchy_global_update_false_ok=$assignHierarchyGlobalUpdateFalseOk"
        "assign_hierarchy_instance_safe_write_used_ok=$assignHierarchyInstanceSafeWriteOk"
        "workflow_b_fallback_documented=$workflowBFallbackDocumented"
        "workflow_b_fallback_script=$r156FallbackScript"
        "target_instance_path=$targetInstancePath"
        "target_symbol_id=$targetSymbolId"
        "target_reference=$targetReference"
        "mutation_performed=$mutationPerformed"
        "exact_blocker_if_any=$finalBlocker"
    ) | Set-Content -Path $summaryPath -Encoding UTF8

    Write-Output "TARGET_RUN_DIR=$runDir"
    Write-Output "SUMMARY_FILE=$summaryPath"
    if ($r163Green -ne "true") { throw "r163_workflow_failed:$finalBlocker" }
}
finally {
    Stop-KiCadProcesses
}
