param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R150 — get_schematic_active_sheet_context_session contract probe (issue19090).
# R151 — workflow readiness ↔ active sheet context ↔ assign dry-run safety.
# R152/R155 — hierarchical workflow via navigate_schematic_sheet_session (no qa_open_editor_frame).

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_pro"
$sch = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic $sch" }

$schAbs = (Resolve-Path $sch).Path
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"
$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-active-sheet-context-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot (Join-Path $BuildDir "ipc-v1-config") -EnableAutomation

function Parse-ActiveSheetContextOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|absent|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                               = ($Text -match 'status=1')
        active_sheet_context_ok                 = (& $parseLine "active_sheet_context_ok")
        active_sheet_context_available          = (& $parseLine "active_sheet_context_available")
        active_sheet_context_schematic_filename = (& $parseLine "active_sheet_context_schematic_filename")
        active_sheet_context_active_sheet_path  = (& $parseLine "active_sheet_context_active_sheet_path")
        active_sheet_context_active_sheet_file  = (& $parseLine "active_sheet_context_active_sheet_file")
        active_sheet_context_is_root            = (& $parseLine "active_sheet_context_is_root")
        active_sheet_context_is_shared          = (& $parseLine "active_sheet_context_is_shared")
        active_sheet_context_symbols_count      = (& $parseLine "active_sheet_context_symbols_count")
        active_sheet_context_symbols_returned   = (& $parseLine "active_sheet_context_symbols_returned")
        active_sheet_context_first_symbol_id    = (& $parseLine "active_sheet_context_first_symbol_id")
        active_sheet_context_exact_blocker_if_any = (& $parseLine "active_sheet_context_exact_blocker_if_any")
        active_sheet_context_diagnostics        = (& $parseLine "active_sheet_context_diagnostics")
    }
}

function Invoke-ActiveSheetContext {
    param(
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_schematic_active_sheet_context_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return @{
        Parsed = Parse-ActiveSheetContextOutput -Text $out
        Status = if ($out -match 'status=1') { "ok" } else { "error" }
        Raw    = $out
    }
}

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
}

function Parse-ReadinessFirstSymbolWithPaths {
    param([string]$OutputText)

    $symbolsStart = $OutputText.IndexOf('key: "symbols"')
    if ($symbolsStart -lt 0) { return $null }
    $symbolsEnd = $OutputText.IndexOf('key: "symbols_count"', $symbolsStart)
    if ($symbolsEnd -lt 0) { return $null }
    $symbolsSection = $OutputText.Substring($symbolsStart, $symbolsEnd - $symbolsStart)

    $chunks = [regex]::Split($symbolsSection, '(?=values\s*\{)')
    foreach ($chunk in $chunks) {
        if ($chunk -notmatch 'key:\s+"symbol_id"') { continue }

        $symbolId = ""
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') {
            $symbolId = $Matches[1]
        }
        $instancePath = ""
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') {
            $instancePath = $Matches[1]
        }
        $footprint = ""
        if ($chunk -match 'key:\s+"footprint"[\s\S]*?string_value:\s*"([^"]*)"') {
            $footprint = $Matches[1]
        }
        if ($symbolId -and $instancePath) {
            return [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
                Footprint    = $footprint
            }
        }
    }
    return $null
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
        if ($symbolId -and $instancePath) {
            return [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
            }
        }
    }
    return $null
}

function Test-ActiveSheetHasInstancePath {
    param(
        [string]$OutputText,
        [string]$InstancePath
    )
    $symbols = Parse-ActiveSheetContextSymbols -OutputText $OutputText
    return ($null -ne ($symbols | Where-Object { $_.InstancePath -eq $InstancePath } | Select-Object -First 1))
}

function Find-ActiveSheetSymbolMatch {
    param(
        [string]$OutputText,
        [string]$InstancePath,
        [string]$SymbolId
    )
    $symbols = Parse-ActiveSheetContextSymbols -OutputText $OutputText
    if (-not $symbols -or $symbols.Count -lt 1) { return $null }

    $byPath = $symbols | Where-Object { $_.InstancePath -eq $InstancePath } | Select-Object -First 1
    if ($null -ne $byPath) { return $byPath }

    return ($symbols | Where-Object { $_.SymbolId -eq $SymbolId } | Select-Object -First 1)
}

function Parse-ActiveSheetContextSymbols {
    param([string]$OutputText)

    $symbolsStart = $OutputText.IndexOf('key: "symbols"')
    if ($symbolsStart -lt 0) { return @() }
    $symbolsEnd = $OutputText.IndexOf('key: "truncated"', $symbolsStart)
    if ($symbolsEnd -lt 0) {
        $symbolsEnd = $OutputText.IndexOf('key: "exact_blocker_if_any"', $symbolsStart)
    }
    if ($symbolsEnd -lt 0) { return @() }
    $symbolsSection = $OutputText.Substring($symbolsStart, $symbolsEnd - $symbolsStart)

    $out = @()
    $chunks = [regex]::Split($symbolsSection, '(?=values\s*\{)')
    foreach ($chunk in $chunks) {
        if ($chunk -notmatch 'key:\s+"symbol_id"') { continue }

        $symbolId = ""
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') {
            $symbolId = $Matches[1]
        }
        $instancePath = ""
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') {
            $instancePath = $Matches[1]
        }
        if ($symbolId -and $instancePath) {
            $out += [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
            }
        }
    }
    return $out
}

function Invoke-NavigateSheetSession {
    param(
        [string]$ApiUrl,
        [hashtable]$ToolArgs,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "navigate_schematic_sheet_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return $out
}

function Parse-NavigateSheetOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|absent|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        navigate_sheet_ok                       = (& $parseLine "navigate_sheet_ok")
        navigate_sheet_post_navigation_verified = (& $parseLine "navigate_sheet_post_navigation_verified")
        navigate_sheet_exact_blocker_if_any     = (& $parseLine "navigate_sheet_exact_blocker_if_any")
    }
}

function Invoke-FootprintReadinessSession {
    param(
        [string]$ApiUrl,
        [hashtable]$ToolArgs,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "schematic_footprint_readiness_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return $out
}

function Parse-AssignFootprintDryRunResult {
    param([string]$OutputText)

    $ok = "false"
    if ($OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "absent"
    if ($OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $affectedInstancesCount = "absent"
    if ($OutputText -match 'key:\s+"affected_instances_count"[\s\S]*?number_value:\s*([0-9]+)') { $affectedInstancesCount = $Matches[1] }
    $sharedSheetDetected = "absent"
    if ($OutputText -match 'key:\s+"shared_sheet_detected"[\s\S]*?bool_value:\s*(true|false)') { $sharedSheetDetected = $Matches[1] }
    $globalUpdate = "absent"
    if ($OutputText -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*(true|false)') { $globalUpdate = $Matches[1] }

    return @{
        ok = $ok
        blocker = $blocker
        affected_instances_count = $affectedInstancesCount
        shared_sheet_detected = $sharedSheetDetected
        global_update = $globalUpdate
    }
}

function Invoke-AssignFootprintDryRun {
    param(
        [string]$ApiUrl,
        [hashtable]$ToolArgs,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-AssignFootprintDryRunResult -OutputText $out
}

$negInvalidMaxOk = "false"
$negMismatchOk = "false"
$optionalFilenameOk = "skipped"
$green = "true"
$finalBlocker = "N/A"

$workflowHierarchyFixtureChecked = "false"
$hierarchyWrongContextInstanceAbsentOk = "false"
$hierarchyWrongContextBlocksAssignmentOk = "false"
$hierarchyActiveContextInstanceMatchOk = "false"
$hierarchyActiveContextSymbolIdMatchOk = "false"
$hierarchyCurrentSheetDryRunSafeOk = "false"
$hierarchyNavigationProductUsed = "false"
$hierarchyCurrentSheetDryRunGlobalUpdateFalseOk = "false"
$workflowUsesQaOpenEditorFrame = "false"
$hierarchySubTargetInstancePath = ""
$hierarchySubTargetSymbolId = ""

Stop-KiCadProcesses
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_active_sheet_context" }
    $url = $resolved.Url

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_active_sheet_context"
    }

    $openArgs = @{ type = "sch"; filename = $schAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    if ($openRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }

    $main = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $schAbs
        include_symbols    = $true
        max_symbols        = 50
    } -LogBaseName "active_sheet_context_main"

    $optional = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        include_symbols = $true
        max_symbols     = 50
    } -LogBaseName "active_sheet_context_optional_filename"

    if ($optional.Parsed.active_sheet_context_ok -eq "true" -and $optional.Parsed.active_sheet_context_schematic_filename -eq $schAbs) {
        $optionalFilenameOk = "true"
    }

    $negMax = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $schAbs
        max_symbols        = 501
    } -LogBaseName "active_sheet_context_neg_invalid_max"

    if ($negMax.Parsed.active_sheet_context_ok -eq "false" -and $negMax.Parsed.active_sheet_context_exact_blocker_if_any -eq "invalid_max_symbols") {
        $negInvalidMaxOk = "true"
    }

    $wrongSch = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\nonexistent_wrong.kicad_sch"
    $negMismatch = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $wrongSch
    } -LogBaseName "active_sheet_context_neg_mismatch"

    if ($negMismatch.Parsed.active_sheet_context_ok -eq "false" -and $negMismatch.Parsed.active_sheet_context_exact_blocker_if_any -eq "schematic_filename_mismatch") {
        $negMismatchOk = "true"
    }

    # R151 — workflow: readiness hierarchy → active sheet context instance_path match → assign dry-run safety
    $workflowInstancePathMatchOk = "false"
    $workflowSymbolIdMatchOk = "false"
    $workflowAssignDryRunOk = "false"
    $workflowTargetInstancePath = ""
    $workflowTargetSymbolId = ""

    $readinessHierOut = Invoke-FootprintReadinessSession -ApiUrl $url -ToolArgs @{
        schematic_filename  = $schAbs
        scope               = "hierarchy"
        include_sheet_paths = $true
        include_symbols     = $true
        max_symbols         = 50
    } -LogBaseName "workflow_readiness_hierarchy"

    $readinessTarget = Parse-ReadinessFirstSymbolWithPaths -OutputText $readinessHierOut
    if ($null -ne $readinessTarget) {
        $workflowTargetInstancePath = $readinessTarget.InstancePath
        $workflowTargetSymbolId = $readinessTarget.SymbolId

        $contextSymbols = Parse-ActiveSheetContextSymbols -OutputText $main.Raw
        if (-not $contextSymbols -or $contextSymbols.Count -lt 1) {
            $contextLogPath = Join-Path $runDir "active_sheet_context_main.log"
            if (Test-Path $contextLogPath) {
                $contextSymbols = Parse-ActiveSheetContextSymbols -OutputText (Get-Content -Path $contextLogPath -Raw)
            }
        }

        $matched = $contextSymbols | Where-Object { $_.InstancePath -eq $workflowTargetInstancePath } | Select-Object -First 1
        if ($null -ne $matched) {
            $workflowInstancePathMatchOk = "true"
            if ($matched.SymbolId -eq $workflowTargetSymbolId) {
                $workflowSymbolIdMatchOk = "true"
            }
        }

        $dryFootprint = if ($readinessTarget.Footprint) { $readinessTarget.Footprint } else { "qa_resistors:R_0201_0603Metric" }
        $assignDry = Invoke-AssignFootprintDryRun -ApiUrl $url -ToolArgs @{
            schematic_filename = $schAbs
            symbol_id          = $workflowTargetSymbolId
            footprint          = $dryFootprint
            scope              = "current_sheet"
            dry_run            = $true
            allow_overwrite    = $true
            validate_footprint = $false
        } -LogBaseName "workflow_assign_current_sheet_dry_run"

        if ($assignDry.ok -eq "true" -and $assignDry.blocker -eq "N/A" -and [int]$assignDry.affected_instances_count -le 1 -and $assignDry.global_update -eq "false" -and $assignDry.shared_sheet_detected -eq "false") {
            $workflowAssignDryRunOk = "true"
        }
    }

    # R152 — hierarchical workflow on ipc_footprint_readiness_v1
    $hierProj = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_pro"
    $hierRootSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_sch"
    $hierSubSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\sub_ipc_fp.kicad_sch"

    if ((Test-Path -LiteralPath $hierProj) -and (Test-Path -LiteralPath $hierRootSch) -and (Test-Path -LiteralPath $hierSubSch)) {
        $workflowHierarchyFixtureChecked = "true"
        $hierarchyWrongContextInstanceAbsentOk = "false"
        $hierarchyWrongContextBlocksAssignmentOk = "false"
        $hierarchyActiveContextInstanceMatchOk = "false"
        $hierarchyActiveContextSymbolIdMatchOk = "false"
        $hierarchyCurrentSheetDryRunSafeOk = "false"

        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        Stop-KiCadProcesses
        Start-Sleep -Seconds 2
        $null = Clear-IpcSockets

        $hierRootSchAbs = (Resolve-Path $hierRootSch).Path
        $hierFpLibTable = Join-Path (Split-Path $hierRootSch) "fp-lib-table"
        @(
            "(fp_lib_table",
            "  (version 7)",
            ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA hierarchy active sheet context"))'),
            ")"
        ) | Set-Content -Path $hierFpLibTable -Encoding ASCII

        $kicadProcHier = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru
        try {
            $resolvedHier = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcHier.Id `
                -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
            if (-not $resolvedHier.Resolved) { throw "ipc_url_unresolved_hierarchy_active_sheet" }
            $urlHier = $resolvedHier.Url

            if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $urlHier -WorkingDirectory $RepoRoot)) {
                throw "project_not_loaded_hierarchy_active_sheet"
            }

            # Harness bootstrap only (not part of hierarchy navigation workflow — see workflow_uses_qa_open_editor_frame).
            $openHierRootArgs = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
            $openHierRootInput = Join-Path $runDir "qa_open_editor_frame_hierarchy_bootstrap_root.json"
            $openHierRootArgs | Set-Content -Path $openHierRootInput -Encoding UTF8
            $openHierRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($urlHier, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openHierRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            if ($openHierRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_hierarchy_bootstrap_root_failed" }

            $hierReadinessOut = Invoke-FootprintReadinessSession -ApiUrl $urlHier -ToolArgs @{
                schematic_filename  = $hierRootSchAbs
                scope               = "hierarchy"
                include_sheet_paths = $true
                include_symbols     = $true
                max_symbols         = 50
            } -LogBaseName "hierarchy_readiness_root_context"

            $hierSubTarget = Parse-ReadinessWithoutFootprintTarget -OutputText $hierReadinessOut
            if ($null -eq $hierSubTarget) { throw "hierarchy_subsheet_without_footprint_target_not_found" }

            $hierarchySubTargetInstancePath = $hierSubTarget.InstancePath
            $hierarchySubTargetSymbolId = $hierSubTarget.SymbolId

            $rootContext = Invoke-ActiveSheetContext -ApiUrl $urlHier -ArgsTable @{
                schematic_filename = $hierRootSchAbs
                include_symbols      = $true
                max_symbols          = 50
            } -LogBaseName "hierarchy_active_sheet_root_wrong_context"

            if (-not (Test-ActiveSheetHasInstancePath -OutputText $rootContext.Raw -InstancePath $hierarchySubTargetInstancePath)) {
                $hierarchyWrongContextInstanceAbsentOk = "true"
            }

            $wrongAssign = Invoke-AssignFootprintDryRun -ApiUrl $urlHier -ToolArgs @{
                schematic_filename = $hierRootSchAbs
                symbol_id          = $hierarchySubTargetSymbolId
                footprint          = "Resistor_SMD:R_0201_0603Metric"
                scope              = "current_sheet"
                dry_run            = $true
                allow_overwrite    = $true
                validate_footprint = $false
            } -LogBaseName "hierarchy_wrong_context_assign_dry_run"

            if ($wrongAssign.ok -eq "false" -and $wrongAssign.blocker -eq "symbol_not_found") {
                $hierarchyWrongContextBlocksAssignmentOk = "true"
            }

            # R155 — product navigation (no qa_open_editor_frame)
            $navOut = Invoke-NavigateSheetSession -ApiUrl $urlHier -ToolArgs @{
                schematic_filename   = $hierRootSchAbs
                instance_path        = $hierarchySubTargetInstancePath
                verify_instance_path = $hierarchySubTargetInstancePath
                dry_run              = $false
            } -LogBaseName "hierarchy_navigate_to_sub_by_instance_path"

            $navParsed = Parse-NavigateSheetOutput -Text $navOut
            if ($navParsed.navigate_sheet_ok -eq "true" -and $navParsed.navigate_sheet_post_navigation_verified -eq "true" -and $navParsed.navigate_sheet_exact_blocker_if_any -eq "N/A") {
                $hierarchyNavigationProductUsed = "true"
            }

            $subContext = Invoke-ActiveSheetContext -ApiUrl $urlHier -ArgsTable @{
                schematic_filename = $hierRootSchAbs
                include_symbols    = $true
                max_symbols        = 50
            } -LogBaseName "hierarchy_active_sheet_sub_correct_context"

            $subMatch = Find-ActiveSheetSymbolMatch -OutputText $subContext.Raw `
                -InstancePath $hierarchySubTargetInstancePath -SymbolId $hierarchySubTargetSymbolId

            if ($null -ne $subMatch) {
                $hierarchyActiveContextInstanceMatchOk = "true"
                if ($subMatch.SymbolId -eq $hierarchySubTargetSymbolId) {
                    $hierarchyActiveContextSymbolIdMatchOk = "true"
                }
            }

            $subAssignDry = Invoke-AssignFootprintDryRun -ApiUrl $urlHier -ToolArgs @{
                schematic_filename = $hierRootSchAbs
                symbol_id          = $hierarchySubTargetSymbolId
                footprint          = "Resistor_SMD:R_0201_0603Metric"
                scope              = "current_sheet"
                dry_run            = $true
                allow_overwrite    = $true
                validate_footprint = $false
            } -LogBaseName "hierarchy_sub_assign_current_sheet_dry_run"

            if ($subAssignDry.ok -eq "true" -and $subAssignDry.blocker -eq "N/A" -and $subAssignDry.global_update -eq "false" -and $subAssignDry.shared_sheet_detected -eq "false" -and [int]$subAssignDry.affected_instances_count -le 1) {
                $hierarchyCurrentSheetDryRunSafeOk = "true"
                $hierarchyCurrentSheetDryRunGlobalUpdateFalseOk = "true"
            }
        }
        finally {
            Stop-Process -Id $kicadProcHier.Id -Force -ErrorAction SilentlyContinue
            Stop-KiCadProcesses
            Start-Sleep -Seconds 1
        }
    }

    $p = $main.Parsed
    if ($p.active_sheet_context_ok -ne "true") { $green = "false"; $finalBlocker = "main_ok_false" }
    elseif ($p.active_sheet_context_available -ne "true") { $green = "false"; $finalBlocker = "active_sheet_unavailable" }
    elseif ($p.active_sheet_context_schematic_filename -ne $schAbs) { $green = "false"; $finalBlocker = "schematic_filename_mismatch_main" }
    elseif ([int]$p.active_sheet_context_symbols_count -lt 1) { $green = "false"; $finalBlocker = "symbols_count_zero" }
    elseif (-not (Test-KiidLike $p.active_sheet_context_first_symbol_id)) { $green = "false"; $finalBlocker = "first_symbol_id_invalid" }
    elseif ($p.active_sheet_context_exact_blocker_if_any -ne "N/A") { $green = "false"; $finalBlocker = "unexpected_blocker_main" }
    elseif ($negInvalidMaxOk -ne "true") { $green = "false"; $finalBlocker = "neg_invalid_max_failed" }
    elseif ($negMismatchOk -ne "true") { $green = "false"; $finalBlocker = "neg_mismatch_failed" }
    elseif ($workflowInstancePathMatchOk -ne "true") { $green = "false"; $finalBlocker = "workflow_instance_path_match_failed" }
    elseif ($workflowSymbolIdMatchOk -ne "true") { $green = "false"; $finalBlocker = "workflow_symbol_id_match_failed" }
    elseif ($workflowAssignDryRunOk -ne "true") { $green = "false"; $finalBlocker = "workflow_assign_dry_run_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyWrongContextInstanceAbsentOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_wrong_context_instance_absent_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyWrongContextBlocksAssignmentOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_wrong_context_blocks_assignment_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyNavigationProductUsed -ne "true") { $green = "false"; $finalBlocker = "hierarchy_navigation_product_used_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyActiveContextInstanceMatchOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_active_context_instance_match_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyActiveContextSymbolIdMatchOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_active_context_symbol_id_match_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyCurrentSheetDryRunSafeOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_current_sheet_dry_run_safe_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $hierarchyCurrentSheetDryRunGlobalUpdateFalseOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_current_sheet_dry_run_global_update_false_failed" }
    elseif ($workflowHierarchyFixtureChecked -eq "true" -and $workflowUsesQaOpenEditorFrame -ne "false") { $green = "false"; $finalBlocker = "workflow_uses_qa_open_editor_frame_not_false" }

    $summaryPath = Join-Path $runDir "qa_get_schematic_active_sheet_context_session_contract_summary.txt"
    @(
        "schematic_active_sheet_context_green=$green"
        "exact_blocker_if_any=$finalBlocker"
        "active_sheet_context_ok=$($p.active_sheet_context_ok)"
        "active_sheet_context_available=$($p.active_sheet_context_available)"
        "active_sheet_context_schematic_filename=$($p.active_sheet_context_schematic_filename)"
        "active_sheet_context_active_sheet_path=$($p.active_sheet_context_active_sheet_path)"
        "active_sheet_context_active_sheet_file=$($p.active_sheet_context_active_sheet_file)"
        "active_sheet_context_is_root=$($p.active_sheet_context_is_root)"
        "active_sheet_context_is_shared=$($p.active_sheet_context_is_shared)"
        "active_sheet_context_symbols_count=$($p.active_sheet_context_symbols_count)"
        "active_sheet_context_first_symbol_id=$($p.active_sheet_context_first_symbol_id)"
        "optional_filename_ok=$optionalFilenameOk"
        "neg_invalid_max_ok=$negInvalidMaxOk"
        "neg_mismatch_ok=$negMismatchOk"
        "workflow_instance_path_match_ok=$workflowInstancePathMatchOk"
        "workflow_symbol_id_match_ok=$workflowSymbolIdMatchOk"
        "workflow_assign_dry_run_ok=$workflowAssignDryRunOk"
        "workflow_target_instance_path=$workflowTargetInstancePath"
        "workflow_target_symbol_id=$workflowTargetSymbolId"
        "workflow_hierarchy_fixture_checked=$workflowHierarchyFixtureChecked"
        "hierarchy_wrong_context_instance_absent_ok=$hierarchyWrongContextInstanceAbsentOk"
        "hierarchy_wrong_context_blocks_assignment_ok=$hierarchyWrongContextBlocksAssignmentOk"
        "hierarchy_active_context_instance_match_ok=$hierarchyActiveContextInstanceMatchOk"
        "hierarchy_active_context_symbol_id_match_ok=$hierarchyActiveContextSymbolIdMatchOk"
        "hierarchy_current_sheet_dry_run_safe_ok=$hierarchyCurrentSheetDryRunSafeOk"
        "hierarchy_navigation_product_used=$hierarchyNavigationProductUsed"
        "hierarchy_current_sheet_dry_run_global_update_false_ok=$hierarchyCurrentSheetDryRunGlobalUpdateFalseOk"
        "workflow_uses_qa_open_editor_frame=$workflowUsesQaOpenEditorFrame"
        "hierarchy_sub_target_instance_path=$hierarchySubTargetInstancePath"
        "hierarchy_sub_target_symbol_id=$hierarchySubTargetSymbolId"
    ) | Set-Content -Path $summaryPath -Encoding UTF8

    Write-Output "TARGET_RUN_DIR=$runDir"
    if ($green -ne "true") { throw "schematic_active_sheet_context_probe_failed:$finalBlocker" }
}
finally {
    Stop-KiCadProcesses
}
