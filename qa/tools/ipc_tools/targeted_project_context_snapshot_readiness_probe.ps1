param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R138 — Bloc issue19090 : mutations FOOTPRINT pour validate ; restore + Stop-KiCadProcesses ;
# voir MCP_DECISIONS.md R138 (audit isolation QA SCH).

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

function Sync-IpcDrcV1Fixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )
    $src = Join-Path $RepoRoot "qa\data\pcbnew\drc\ipc_drc_v1"
    $dst = Join-Path $BuildDir "ipc-drc-v1-proj"
    if (!(Test-Path -LiteralPath $src)) { throw "Missing ipc_drc_v1 fixture at $src" }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcDrcV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project" }
if (!(Test-Path $pcb)) { throw "Missing board" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-project-context-snapshot-readiness-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$script:ProbeIpcRetryExtraTotal = 0

function Parse-SnapshotOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                               = ($Text -match 'status=1')
        project_readiness_summary_present       = (& $parseLine "project_readiness_summary_present")
        readiness_available                     = (& $parseLine "readiness_available")
        readiness_project_loaded                = (& $parseLine "readiness_project_loaded")
        readiness_schematic_open                = (& $parseLine "readiness_schematic_open")
        readiness_pcb_open                      = (& $parseLine "readiness_pcb_open")
        readiness_pcb_absolute_path_present     = (& $parseLine "readiness_pcb_absolute_path_present")
        readiness_drc_available                 = (& $parseLine "readiness_drc_available")
        readiness_drc_summary_present           = (& $parseLine "readiness_drc_summary_present")
        readiness_drc_has_violations            = (& $parseLine "readiness_drc_has_violations")
        readiness_drc_has_clearance_violations  = (& $parseLine "readiness_drc_has_clearance_violations")
        readiness_footprints_summary_available  = (& $parseLine "readiness_footprints_summary_available")
        readiness_symbols_count               = (& $parseLine "readiness_symbols_count")
        readiness_symbols_without_footprint_count = (& $parseLine "readiness_symbols_without_footprint_count")
        readiness_sch_to_pcb_ready            = (& $parseLine "readiness_sch_to_pcb_ready")
        readiness_symbols_with_invalid_footprint_count = (& $parseLine "readiness_symbols_with_invalid_footprint_count")
        readiness_sch_to_pcb_ready_validated  = (& $parseLine "readiness_sch_to_pcb_ready_validated")
        readiness_footprint_validation_performed = (& $parseLine "readiness_footprint_validation_performed")
        readiness_footprints_scope              = (& $parseLine "readiness_footprints_scope")
        readiness_symbols_instances_count       = (& $parseLine "readiness_symbols_instances_count")
        readiness_footprints_summary_diagnostic = (& $parseLine "readiness_footprints_summary_diagnostic")
        readiness_exact_blocker_if_any          = (& $parseLine "readiness_exact_blocker_if_any")
        readiness_recommended_next_actions      = (& $parseLine "readiness_recommended_next_actions")
        readiness_recommended_tools             = (& $parseLine "readiness_recommended_tools")
        readiness_hierarchy_assign_instance_safe_enabled = (& $parseLine "readiness_hierarchy_assign_instance_safe_enabled")
        readiness_recommended_footprint_assignment_workflow = (& $parseLine "readiness_recommended_footprint_assignment_workflow")
        readiness_fallback_footprint_assignment_workflow = (& $parseLine "readiness_fallback_footprint_assignment_workflow")
        readiness_recommended_assign_tool       = (& $parseLine "readiness_recommended_assign_tool")
        readiness_recommended_assign_scope      = (& $parseLine "readiness_recommended_assign_scope")
        footprints_summary_present              = (& $parseLine "footprints_summary_present")
        footprints_summary_available            = (& $parseLine "footprints_summary_available")
        footprints_summary_scope                = (& $parseLine "footprints_summary_scope")
        footprints_summary_symbols_count        = (& $parseLine "footprints_summary_symbols_count")
        footprints_summary_symbols_instances_count = (& $parseLine "footprints_summary_symbols_instances_count")
        footprints_summary_sch_to_pcb_ready     = (& $parseLine "footprints_summary_sch_to_pcb_ready")
        drc_summary_present                     = (& $parseLine "drc_summary_present")
        drc_summary_run_mode                    = (& $parseLine "drc_summary_run_mode")
        drc_summary_violations_count            = (& $parseLine "drc_summary_violations_count")
        drc_summary_has_clearance_violations    = (& $parseLine "drc_summary_has_clearance_violations")
        snapshot_open_pcb_board_absolute_path_present = (& $parseLine "snapshot_open_pcb_board_absolute_path_present")
        raw                                     = $Text
    }
}

function Invoke-ProjectContextSnapshot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_project_context_snapshot", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return @{
        Parsed = Parse-SnapshotOutput -Text $out
        Status = if ($out -match 'status=1') { "ok" } else { "error" }
        Raw    = $out
    }
}

function Test-ActionsInclude {
    param(
        [string]$Csv,
        [string]$Action
    )
    if ([string]::IsNullOrWhiteSpace($Csv) -or $Csv -eq "absent") { return $false }
    $parts = @($Csv.Split(',') | ForEach-Object { $_.Trim() })
    return ($parts -contains $Action)
}

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
}

function Take-FirstValid {
    param([System.Text.RegularExpressions.MatchCollection]$Matches)
    foreach ($m in $Matches) {
        if ($m.Groups.Count -lt 2) { continue }
        $v = $m.Groups[1].Value.Trim()
        if (Test-KiidLike $v) { return $v }
    }
    return ""
}

function Resolve-SymbolIdFromSchItemsText {
    param([string]$ItemsOut)
    if ([string]::IsNullOrWhiteSpace($ItemsOut)) { return "" }
    $symbolId = ""
    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
    $symbolId = Take-FirstValid $typedSchSymId
    if ([string]::IsNullOrWhiteSpace($symbolId)) {
        $typedSchSymUuid = [regex]::Matches(
            $ItemsOut,
            'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
        $symbolId = Take-FirstValid $typedSchSymUuid
    }
    if ([string]::IsNullOrWhiteSpace($symbolId)) {
        $idValueCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
        $symbolId = Take-FirstValid $idValueCandidates
    }
    return $symbolId
}

function Resolve-AllSymbolIdsFromSchItemsText {
    param([string]$ItemsOut)
    $ids = @()
    if ([string]::IsNullOrWhiteSpace($ItemsOut)) { return $ids }
    foreach ($m in [regex]::Matches($ItemsOut, 'sch_item_id=([0-9a-fA-F-]{36})')) {
        $v = $m.Groups[1].Value.Trim()
        if (Test-KiidLike $v) { $ids += $v }
    }
    if ($ids.Count -gt 0) { return @($ids | Select-Object -Unique) }
    return @()
}

function Set-SymbolFootprintFieldForSnapshot {
    param(
        [string]$ApiUrl,
        [string]$SchFilename,
        [string]$SymId,
        [string]$FpValue,
        [string]$LogSuffix
    )
    $args = @{
        schematic_filename = $SchFilename
        symbol_id          = $SymId
        field_name         = "Footprint"
        new_value          = $FpValue
        dry_run            = $false
    } | ConvertTo-Json -Compress
    $inputPath = Join-Path $runDir ("snapshot_set_symbol_field_" + $LogSuffix + ".json")
    Set-Content -Path $inputPath -Value $args -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "schematic_set_symbol_field_session", ("@" + $inputPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($sm.Text -notmatch 'status=1') { throw "snapshot_set_symbol_field_failed_$LogSuffix" }
}

$defaultReadinessOk = "false"
$defaultNoDrcOk = "false"
$footprintsUnknownOk = "false"
$drcExistingMarkersReadinessOk = "false"
$drcEngineReadinessOk = "false"
$recommendedActionsOk = "false"
$pcbAbsolutePathReadinessOk = "false"
$footprintsOptInNoSchOk = "false"
$footprintsOptInHappyOk = "false"
$footprintsOptInValidateTrueOk = "false"
$fixInvalidFootprintsActionOk = "false"
$footprintsHierarchyScopeOk = "false"
$hierarchyAssignRuntimeOk = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }
    Start-Sleep -Seconds 3

    $tDefault = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{} -LogBaseName "snapshot_default_readiness"
    if ($tDefault.Status -eq "ok" `
            -and $tDefault.Parsed.project_readiness_summary_present -eq "1" `
            -and $tDefault.Parsed.readiness_available -eq "true" `
            -and $tDefault.Parsed.readiness_project_loaded -eq "true" `
            -and $tDefault.Parsed.readiness_pcb_open -eq "true" `
            -and $tDefault.Parsed.readiness_pcb_absolute_path_present -eq "true" `
            -and $tDefault.Parsed.readiness_drc_available -eq "true" `
            -and $tDefault.Parsed.readiness_drc_summary_present -eq "false" `
            -and $tDefault.Parsed.readiness_drc_has_violations -eq "null" `
            -and $tDefault.Parsed.readiness_drc_has_clearance_violations -eq "null" `
            -and $tDefault.Parsed.readiness_footprints_summary_available -eq "false" `
            -and $tDefault.Parsed.readiness_exact_blocker_if_any -eq "N/A" `
            -and $tDefault.Parsed.drc_summary_present -eq "0") {
        $defaultReadinessOk = "true"
    }
    if ($tDefault.Parsed.drc_summary_present -eq "0" -or $tDefault.Parsed.drc_summary_present -eq "absent") {
        $defaultNoDrcOk = "true"
    }
    if ($tDefault.Parsed.readiness_footprints_summary_available -eq "false" `
            -and $tDefault.Parsed.readiness_footprints_scope -eq "absent" `
            -and $tDefault.Parsed.readiness_footprints_summary_diagnostic -match "schematic_footprint_readiness_session") {
        $footprintsUnknownOk = "true"
    }
    if ($tDefault.Parsed.readiness_pcb_absolute_path_present -eq "true" `
            -and $tDefault.Parsed.snapshot_open_pcb_board_absolute_path_present -eq "true") {
        $pcbAbsolutePathReadinessOk = "true"
    }
    if ($tDefault.Parsed.readiness_recommended_next_actions -ne "absent" `
            -and (Test-ActionsInclude $tDefault.Parsed.readiness_recommended_next_actions "request_drc_summary_engine_run")) {
        $recommendedActionsOk = "true"
    }
    if ($tDefault.Parsed.readiness_hierarchy_assign_instance_safe_enabled -eq "true" `
            -and $tDefault.Parsed.readiness_recommended_footprint_assignment_workflow -eq "hierarchy_instance_safe" `
            -and $tDefault.Parsed.readiness_fallback_footprint_assignment_workflow -eq "navigate_current_sheet_dry_run" `
            -and $tDefault.Parsed.readiness_recommended_assign_tool -eq "schematic_assign_footprint_session" `
            -and $tDefault.Parsed.readiness_recommended_assign_scope -eq "hierarchy") {
        $hierarchyAssignRuntimeOk = "true"
    }

    $tMarkers = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "existing_markers"
    } -LogBaseName "snapshot_existing_markers_readiness"
    if ($tMarkers.Status -eq "ok" `
            -and $tMarkers.Parsed.readiness_drc_summary_present -eq "true" `
            -and $tMarkers.Parsed.drc_summary_present -eq "1" `
            -and $tMarkers.Parsed.drc_summary_run_mode -eq "existing_markers" `
            -and $tMarkers.Parsed.readiness_drc_has_violations -eq "false" `
            -and (Test-ActionsInclude $tMarkers.Parsed.readiness_recommended_next_actions "request_drc_summary_engine_run")) {
        $drcExistingMarkersReadinessOk = "true"
    }

    $tEngine = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_drc_summary = $true
        drc_run_mode          = "engine_run"
        drc_max_violations    = 10
    } -LogBaseName "snapshot_engine_run_readiness"
    $violCount = 0
    if ($tEngine.Parsed.drc_summary_violations_count -match '^[0-9]+$') {
        $violCount = [int]$tEngine.Parsed.drc_summary_violations_count
    }
    if ($tEngine.Status -eq "ok" `
            -and $tEngine.Parsed.readiness_drc_summary_present -eq "true" `
            -and $tEngine.Parsed.drc_summary_present -eq "1" `
            -and $tEngine.Parsed.readiness_drc_has_violations -eq "true" `
            -and $tEngine.Parsed.readiness_drc_has_clearance_violations -eq "true" `
            -and $violCount -ge 1 `
            -and (Test-ActionsInclude $tEngine.Parsed.readiness_recommended_next_actions "run_drc_detail")) {
        $drcEngineReadinessOk = "true"
    }

    $tOptInNoSch = Invoke-ProjectContextSnapshot -ApiUrl $url -ArgsTable @{
        include_footprints_summary = $true
    } -LogBaseName "snapshot_footprints_opt_in_no_sch"
    if ($tOptInNoSch.Status -eq "ok" `
            -and $tOptInNoSch.Parsed.footprints_summary_present -eq "1" `
            -and $tOptInNoSch.Parsed.readiness_footprints_summary_available -eq "false" `
            -and $tOptInNoSch.Parsed.footprints_summary_available -eq "false") {
        $footprintsOptInNoSchOk = "true"
    }

    $green = "true"
    if ($defaultReadinessOk -ne "true") { $green = "false"; $finalBlocker = "default_readiness_failed" }
    elseif ($defaultNoDrcOk -ne "true") { $green = "false"; $finalBlocker = "default_no_drc_regression" }
    elseif ($footprintsUnknownOk -ne "true") { $green = "false"; $finalBlocker = "footprints_unknown_diagnostic_failed" }
    elseif ($drcExistingMarkersReadinessOk -ne "true") { $green = "false"; $finalBlocker = "drc_existing_markers_readiness_failed" }
    elseif ($drcEngineReadinessOk -ne "true") { $green = "false"; $finalBlocker = "drc_engine_readiness_failed" }
    elseif ($recommendedActionsOk -ne "true") { $green = "false"; $finalBlocker = "recommended_actions_default_failed" }
    elseif ($hierarchyAssignRuntimeOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_assign_runtime_status_failed" }
    elseif ($pcbAbsolutePathReadinessOk -ne "true") { $green = "false"; $finalBlocker = "pcb_absolute_path_readiness_failed" }
    elseif ($footprintsOptInNoSchOk -ne "true") { $green = "false"; $finalBlocker = "footprints_opt_in_no_sch_failed" }
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

if ($green -eq "true") {
    $issueProj = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_pro"
    $issueSch = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
    $qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"
    if ((Test-Path $issueProj) -and (Test-Path $issueSch) -and (Test-Path $qaResistorPretty)) {
        Stop-KiCadProcesses
        Start-Sleep -Seconds 2
        $null = Clear-IpcSockets
        $schAbsIssue = (Resolve-Path $issueSch).Path
        $issueFpLibTable = Join-Path (Split-Path $issueSch) "fp-lib-table"
        $qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
        @(
            "(fp_lib_table",
            "  (version 7)",
            ('  (lib (name "qa_resistors")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA deterministic resistor footprint library"))'),
            ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA alias for issue19090 restore footprint"))'),
            ")"
        ) | Set-Content -Path $issueFpLibTable -Encoding ASCII
        $kicadProc2 = Start-Process -FilePath $kicadExe -ArgumentList $issueProj -WorkingDirectory $RepoRoot -PassThru
        try {
            $resolved2 = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc2.Id `
                -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
            if (-not $resolved2.Resolved) { throw "ipc_url_unresolved_issue19090" }
            $url2 = $resolved2.Url
            if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url2 -WorkingDirectory $RepoRoot)) {
                throw "project_not_loaded_issue19090"
            }
            $openSchArgs = @{ type = "sch"; filename = $schAbsIssue } | ConvertTo-Json -Compress
            $openSchInput = Join-Path $runDir "qa_open_editor_frame_issue19090_sch.json"
            Set-Content -Path $openSchInput -Value $openSchArgs -Encoding UTF8
            $openSchSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url2, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openSchInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            if ($openSchSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_sch_failed" }
            Start-Sleep -Seconds 3

            $validFootprint = "Resistor_SMD:R_0201_0603Metric"
            $restoreFootprint = "Resistor_SMD:R_0805_2012Metric"
            $itemsPrepArgs = @{ schematic_filename = $schAbsIssue; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
            $itemsPrepInput = Join-Path $runDir "get_sch_items_issue19090_prep.json"
            Set-Content -Path $itemsPrepInput -Value $itemsPrepArgs -Encoding UTF8
            $itemsPrepSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url2, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsPrepInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            $symbolIdsIssue = @(Resolve-AllSymbolIdsFromSchItemsText -ItemsOut $itemsPrepSm.Text)
            if ($symbolIdsIssue.Count -lt 1) { throw "symbol_id_discovery_failed_issue19090" }
            foreach ($sid in $symbolIdsIssue) {
                Set-SymbolFootprintFieldForSnapshot -ApiUrl $url2 -SchFilename $schAbsIssue -SymId $sid -FpValue $validFootprint -LogSuffix ("valid_" + $sid.Substring(0, 8))
            }
            Start-Sleep -Seconds 1

            $tOptInHappy = Invoke-ProjectContextSnapshot -ApiUrl $url2 -ArgsTable @{
                include_footprints_summary = $true
                footprints_max_symbols   = 50
            } -LogBaseName "snapshot_footprints_opt_in_happy"

            $symCountHappy = 0
            if ($tOptInHappy.Parsed.readiness_symbols_count -match '^[0-9]+$') {
                $symCountHappy = [int]$tOptInHappy.Parsed.readiness_symbols_count
            }
            if ($tOptInHappy.Status -eq "ok" `
                    -and $tOptInHappy.Parsed.footprints_summary_present -eq "1" `
                    -and $tOptInHappy.Parsed.readiness_footprints_summary_available -eq "true" `
                    -and $tOptInHappy.Parsed.footprints_summary_available -eq "true" `
                    -and $symCountHappy -ge 2 `
                    -and $tOptInHappy.Parsed.readiness_sch_to_pcb_ready -eq "true" `
                    -and $tOptInHappy.Parsed.readiness_symbols_without_footprint_count -eq "0" `
                    -and $tOptInHappy.Parsed.readiness_footprint_validation_performed -eq "false" `
                    -and $tOptInHappy.Parsed.readiness_sch_to_pcb_ready_validated -eq "null" `
                    -and -not (Test-ActionsInclude $tOptInHappy.Parsed.readiness_recommended_next_actions "assign_missing_footprints")) {
                $footprintsOptInHappyOk = "true"
            } else {
                $green = "false"
                $finalBlocker = "footprints_opt_in_happy_failed"
            }

            $tOptInValidate = Invoke-ProjectContextSnapshot -ApiUrl $url2 -ArgsTable @{
                include_footprints_summary = $true
                footprints_max_symbols     = 50
                footprints_validate        = $true
            } -LogBaseName "snapshot_footprints_opt_in_validate_true"

            if ($tOptInValidate.Status -eq "ok" `
                    -and $tOptInValidate.Parsed.readiness_footprint_validation_performed -eq "true" `
                    -and $tOptInValidate.Parsed.readiness_symbols_with_invalid_footprint_count -eq "0" `
                    -and $tOptInValidate.Parsed.readiness_sch_to_pcb_ready_validated -eq "true") {
                $footprintsOptInValidateTrueOk = "true"
            } else {
                $green = "false"
                $finalBlocker = "footprints_opt_in_validate_true_failed"
            }

            $symbolId = $symbolIdsIssue[0]
            Set-SymbolFootprintFieldForSnapshot -ApiUrl $url2 -SchFilename $schAbsIssue -SymId $symbolId -FpValue "qa_resistors:missing_fp" -LogSuffix "invalid"
            Start-Sleep -Seconds 1

            $tOptInInvalid = Invoke-ProjectContextSnapshot -ApiUrl $url2 -ArgsTable @{
                    include_footprints_summary = $true
                    footprints_max_symbols     = 50
                    footprints_validate        = $true
                } -LogBaseName "snapshot_footprints_opt_in_validate_invalid"

                if ($tOptInInvalid.Status -eq "ok" `
                        -and $tOptInInvalid.Parsed.readiness_footprint_validation_performed -eq "true" `
                        -and ($tOptInInvalid.Parsed.readiness_symbols_with_invalid_footprint_count -match '^[1-9][0-9]*$') `
                        -and $tOptInInvalid.Parsed.readiness_sch_to_pcb_ready_validated -eq "false" `
                        -and (Test-ActionsInclude $tOptInInvalid.Parsed.readiness_recommended_next_actions "fix_invalid_footprints")) {
                $fixInvalidFootprintsActionOk = "true"
            } else {
                $green = "false"
                $finalBlocker = "fix_invalid_footprints_action_failed"
            }

            foreach ($sid in $symbolIdsIssue) {
                Set-SymbolFootprintFieldForSnapshot -ApiUrl $url2 -SchFilename $schAbsIssue -SymId $sid -FpValue $restoreFootprint -LogSuffix ("restore_" + $sid.Substring(0, 8))
            }
        }
        finally {
            if ($kicadProc2 -and -not $kicadProc2.HasExited) {
                Stop-Process -Id $kicadProc2.Id -Force -ErrorAction SilentlyContinue
            }
            Stop-KiCadProcesses
        }
    } else {
        $footprintsOptInHappyOk = "skipped"
        $footprintsOptInValidateTrueOk = "skipped"
        $fixInvalidFootprintsActionOk = "skipped"
    }

    if ($footprintsOptInHappyOk -ne "true" -and $footprintsOptInHappyOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "footprints_opt_in_happy_failed" }
    }
    if ($footprintsOptInValidateTrueOk -ne "true" -and $footprintsOptInValidateTrueOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "footprints_opt_in_validate_true_failed" }
    }
    if ($fixInvalidFootprintsActionOk -ne "true" -and $fixInvalidFootprintsActionOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "fix_invalid_footprints_action_failed" }
    }
}

$hierProj = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_pro"
$hierSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_sch"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"
if ($green -eq "true" -and (Test-Path $hierProj) -and (Test-Path $hierSch) -and (Test-Path $qaResistorPretty)) {
    Stop-KiCadProcesses
    Start-Sleep -Seconds 2
    $null = Clear-IpcSockets
    $hierSchAbs = (Resolve-Path $hierSch).Path
    $hierFpLibTable = Join-Path (Split-Path $hierSch) "fp-lib-table"
    $qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
    @(
        "(fp_lib_table",
        "  (version 7)",
        ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA hierarchy snapshot readiness"))'),
        ")"
    ) | Set-Content -Path $hierFpLibTable -Encoding ASCII

    $kicadProcHier = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru
    try {
        $resolvedHier = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcHier.Id `
            -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
        if (-not $resolvedHier.Resolved) { throw "ipc_url_unresolved_hierarchy_snapshot" }
        $urlHier = $resolvedHier.Url
        if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $urlHier -WorkingDirectory $RepoRoot)) {
            throw "project_not_loaded_hierarchy_snapshot"
        }

        $openHierArgs = @{ type = "sch"; filename = $hierSchAbs } | ConvertTo-Json -Compress
        $openHierInput = Join-Path $runDir "qa_open_editor_frame_hierarchy_snapshot_sch.json"
        Set-Content -Path $openHierInput -Value $openHierArgs -Encoding UTF8
        $openHierSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($urlHier, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openHierInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        if ($openHierSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_hierarchy_snapshot_failed" }
        Start-Sleep -Seconds 3

        $tHierarchySnapshot = Invoke-ProjectContextSnapshot -ApiUrl $urlHier -ArgsTable @{
            include_footprints_summary = $true
            footprints_scope           = "hierarchy"
            footprints_max_symbols     = 50
        } -LogBaseName "snapshot_footprints_opt_in_hierarchy"

        $hierSymCount = 0
        $hierInstances = 0
        if ($tHierarchySnapshot.Parsed.readiness_symbols_count -match '^[0-9]+$') {
            $hierSymCount = [int]$tHierarchySnapshot.Parsed.readiness_symbols_count
        }
        if ($tHierarchySnapshot.Parsed.readiness_symbols_instances_count -match '^[0-9]+$') {
            $hierInstances = [int]$tHierarchySnapshot.Parsed.readiness_symbols_instances_count
        }

        if ($tHierarchySnapshot.Status -eq "ok" `
                -and $tHierarchySnapshot.Parsed.footprints_summary_present -eq "1" `
                -and $tHierarchySnapshot.Parsed.readiness_footprints_summary_available -eq "true" `
                -and $tHierarchySnapshot.Parsed.readiness_footprints_scope -eq "hierarchy" `
                -and $tHierarchySnapshot.Parsed.footprints_summary_scope -eq "hierarchy" `
                -and $hierSymCount -ge 3 `
                -and $hierInstances -ge 3 `
                -and $tHierarchySnapshot.Parsed.readiness_sch_to_pcb_ready -eq "false" `
                -and (Test-ActionsInclude $tHierarchySnapshot.Parsed.readiness_recommended_next_actions "assign_missing_footprints")) {
            $footprintsHierarchyScopeOk = "true"
        } else {
            $green = "false"
            $finalBlocker = "footprints_hierarchy_scope_failed"
        }
    }
    finally {
        if ($kicadProcHier -and -not $kicadProcHier.HasExited) {
            Stop-Process -Id $kicadProcHier.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
    }
} else {
    $footprintsHierarchyScopeOk = "skipped"
}

if ($green -eq "true") {
    @(
        "project_context_snapshot_readiness_green=$green"
        "default_readiness_ok=$defaultReadinessOk"
        "default_no_drc_ok=$defaultNoDrcOk"
        "footprints_unknown_ok=$footprintsUnknownOk"
        "footprints_opt_in_no_sch_ok=$footprintsOptInNoSchOk"
        "footprints_opt_in_happy_ok=$footprintsOptInHappyOk"
        "footprints_opt_in_validate_true_ok=$footprintsOptInValidateTrueOk"
        "footprints_hierarchy_scope_ok=$footprintsHierarchyScopeOk"
        "fix_invalid_footprints_action_ok=$fixInvalidFootprintsActionOk"
        "drc_existing_markers_readiness_ok=$drcExistingMarkersReadinessOk"
        "drc_engine_readiness_ok=$drcEngineReadinessOk"
        "recommended_actions_ok=$recommendedActionsOk"
        "hierarchy_assign_runtime_ok=$hierarchyAssignRuntimeOk"
        "pcb_absolute_path_readiness_ok=$pcbAbsolutePathReadinessOk"
        "exact_blocker_if_any=$finalBlocker"
        "ipc_smoke_ipc_retry_extra_total=$script:ProbeIpcRetryExtraTotal"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        "reported_readiness_recommended_next_actions_default=$($tDefault.Parsed.readiness_recommended_next_actions)"
        "reported_readiness_recommended_next_actions_engine=$($tEngine.Parsed.readiness_recommended_next_actions)"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_project_context_snapshot_readiness_summary.txt") -Encoding UTF8
} else {
    @(
        "project_context_snapshot_readiness_green=$green"
        "default_readiness_ok=$defaultReadinessOk"
        "default_no_drc_ok=$defaultNoDrcOk"
        "footprints_unknown_ok=$footprintsUnknownOk"
        "footprints_opt_in_no_sch_ok=$footprintsOptInNoSchOk"
        "footprints_opt_in_happy_ok=$footprintsOptInHappyOk"
        "footprints_opt_in_validate_true_ok=$footprintsOptInValidateTrueOk"
        "footprints_hierarchy_scope_ok=$footprintsHierarchyScopeOk"
        "fix_invalid_footprints_action_ok=$fixInvalidFootprintsActionOk"
        "drc_existing_markers_readiness_ok=$drcExistingMarkersReadinessOk"
        "drc_engine_readiness_ok=$drcEngineReadinessOk"
        "recommended_actions_ok=$recommendedActionsOk"
        "hierarchy_assign_runtime_ok=$hierarchyAssignRuntimeOk"
        "pcb_absolute_path_readiness_ok=$pcbAbsolutePathReadinessOk"
        "exact_blocker_if_any=$finalBlocker"
        "ipc_smoke_ipc_retry_extra_total=$script:ProbeIpcRetryExtraTotal"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_project_context_snapshot_readiness_summary.txt") -Encoding UTF8
}

if ($green -ne "true") {
    Write-Host "TARGET_RUN_DIR=$runDir"
    exit 1
}

Write-Host "TARGET_RUN_DIR=$runDir"
exit 0
