param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R156 — End-to-end example: Navigable SCH Footprint Workflow (product navigation, dry-run only).
# Fixture: qa/data/eeschema/hierarchy/ipc_footprint_readiness_v1/
# Does NOT use qa_open_editor_frame for hierarchical navigation.
# Does NOT perform real footprint mutation or scope=hierarchy mutation.

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

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $hierProj)) { throw "Missing hierarchy project $hierProj" }
if (!(Test-Path $hierRootSch)) { throw "Missing hierarchy schematic $hierRootSch" }

$hierRootSchAbs = (Resolve-Path $hierRootSch).Path
$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r156-navigable-sch-footprint-workflow")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot (Join-Path $BuildDir "ipc-v1-config") -EnableAutomation

$hierFpLibTable = Join-Path (Split-Path $hierRootSch) "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R156 navigable workflow example"))'),
    ")"
) | Set-Content -Path $hierFpLibTable -Encoding ASCII

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
        $sheetPath = ""
        if ($chunk -match 'key:\s+"sheet_path"[\s\S]*?string_value:\s*"([^"]+)"') {
            $sheetPath = $Matches[1]
        }
        $reference = ""
        if ($chunk -match 'key:\s+"reference"[\s\S]*?string_value:\s*"([^"]+)"') {
            $reference = $Matches[1]
        }
        $footprint = ""
        if ($chunk -match 'key:\s+"footprint"[\s\S]*?string_value:\s*"([^"]*)"') {
            $footprint = $Matches[1]
        }
        if ($symbolId -and $instancePath) {
            return [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
                SheetPath    = $sheetPath
                Reference    = $reference
                Footprint    = $footprint
            }
        }
    }
    return $null
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

function Normalize-SheetPathForCompare {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    $p = $Path.Trim()
    if ($p -eq "/") { return "/" }
    if (-not $p.EndsWith("/")) { $p = $p + "/" }
    return $p
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
    if ($Text -match ($Key + '=(true|false)')) { return $Matches[1] }
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]*?bool_value:\s*(true|false)')) { return $Matches[1] }
    return "false"
}

function Parse-StringField {
    param([string]$Text, [string]$Key)
    if ($Text -match ($Key + '=([^\r\n]+)')) { return $Matches[1].Trim() }
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]*?string_value:\s*"([^"]*)"')) { return $Matches[1] }
    return ""
}

function Parse-IntField {
    param([string]$Text, [string]$Key)
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]*?number_value:\s*([0-9]+)')) { return $Matches[1] }
    return "absent"
}

$readinessHierarchyOk = "false"
$navigateOk = "false"
$postNavigationVerified = "false"
$activeSheetContextOk = "false"
$instancePathMatchOk = "false"
$symbolIdMatchOk = "false"
$assignDryRunOk = "false"
$assignDryRunGlobalUpdateFalseOk = "false"
$assignDryRunSharedSheetFalseOk = "false"
$workflowUsesQaOpenEditorFrameForNavigation = "false"
$mutationPerformed = "false"
$agentDecision = "do_not_mutate"
$finalBlocker = "N/A"
$r156Green = "false"

$targetInstancePath = ""
$targetSheetPath = ""
$targetSymbolId = ""
$targetReference = ""

Stop-KiCadProcesses
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru

try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r156" }
    $url = $resolved.Url

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r156"
    }

    # A — Bootstrap SCH editor root (QA harness only; not hierarchical navigation).
    $openRootArgs = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_bootstrap_root.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_bootstrap_root_failed" }

    # B — Readiness hierarchy
    $readinessOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_footprint_readiness_session" -ToolArgs @{
        schematic_filename  = $hierRootSchAbs
        scope               = "hierarchy"
        include_sheet_paths = $true
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $false
    } -LogBaseName "r156_readiness_hierarchy"

    if ($readinessOut -notmatch 'status=1') { throw "r156_readiness_hierarchy_failed" }

    $target = Parse-ReadinessWithoutFootprintTarget -OutputText $readinessOut
    if ($null -eq $target) { throw "r156_readiness_target_not_found" }

    $targetInstancePath = $target.InstancePath
    $targetSymbolId = $target.SymbolId
    $targetReference = $target.Reference
    # Derive UUID sheet path from instance_path (navigate probe pattern); readiness sheet_path may be human-readable.
    if ($targetInstancePath -match '^(.+/)([0-9a-fA-F-]{36})$') {
        $targetSheetPath = Normalize-SheetPathForCompare -Path $Matches[1]
    }
    else {
        throw "r156_target_sheet_path_unparseable"
    }
    $readinessHierarchyOk = "true"

    # C — Product navigation (NOT qa_open_editor_frame)
    $navOut = Invoke-McpTool -ApiUrl $url -ToolName "navigate_schematic_sheet_session" -ToolArgs @{
        schematic_filename   = $hierRootSchAbs
        instance_path        = $targetInstancePath
        verify_instance_path = $targetInstancePath
        dry_run              = $false
    } -LogBaseName "r156_navigate_by_instance_path"

    if ($navOut -match 'status=1') {
        $navOk = Parse-BoolField -Text $navOut -Key "navigate_sheet_ok"
        if ($navOk -eq "false") { $navOk = Parse-BoolField -Text $navOut -Key "ok" }
        $postNav = Parse-BoolField -Text $navOut -Key "navigate_sheet_post_navigation_verified"
        if ($postNav -eq "false") { $postNav = Parse-BoolField -Text $navOut -Key "post_navigation_verified" }
        $resolvedPath = Parse-StringField -Text $navOut -Key "navigate_sheet_resolved_sheet_path"
        if ([string]::IsNullOrWhiteSpace($resolvedPath)) {
            $resolvedPath = Parse-StringField -Text $navOut -Key "resolved_sheet_path"
        }
        $activeAfter = Parse-StringField -Text $navOut -Key "navigate_sheet_active_sheet_path_after"
        if ([string]::IsNullOrWhiteSpace($activeAfter)) {
            $activeAfter = Parse-StringField -Text $navOut -Key "active_sheet_path_after"
        }

        if ($navOk -eq "true" -and $postNav -eq "true") {
            $normalizedTarget = Normalize-SheetPathForCompare -Path $targetSheetPath
            $normalizedAfter = Normalize-SheetPathForCompare -Path $activeAfter
            $normalizedResolved = Normalize-SheetPathForCompare -Path $resolvedPath
            if ($normalizedAfter -eq $normalizedTarget -or $normalizedResolved -eq $normalizedTarget) {
                $navigateOk = "true"
                $postNavigationVerified = "true"
            }
        }
    }

    # D — Active sheet context
    $contextOut = Invoke-McpTool -ApiUrl $url -ToolName "get_schematic_active_sheet_context_session" -ToolArgs @{
        schematic_filename = $hierRootSchAbs
        include_symbols    = $true
        max_symbols        = 50
    } -LogBaseName "r156_active_sheet_context"

    if ($contextOut -match 'status=1') {
        $ctxOk = Parse-BoolField -Text $contextOut -Key "active_sheet_context_ok"
        if ($ctxOk -eq "false") { $ctxOk = Parse-BoolField -Text $contextOut -Key "ok" }
        if ($ctxOk -eq "true") { $activeSheetContextOk = "true" }

        $symbols = Parse-ActiveSheetContextSymbols -OutputText $contextOut
        $match = $symbols | Where-Object { $_.InstancePath -eq $targetInstancePath } | Select-Object -First 1
        if ($null -ne $match) {
            $instancePathMatchOk = "true"
            if ($match.SymbolId -eq $targetSymbolId) {
                $symbolIdMatchOk = "true"
            }
        }
    }

    # E — Assign dry-run current_sheet (no real mutation)
    $assignOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_assign_footprint_session" -ToolArgs @{
        schematic_filename = $hierRootSchAbs
        symbol_id          = $targetSymbolId
        footprint          = $testFootprint
        scope              = "current_sheet"
        dry_run            = $true
        allow_overwrite    = $true
        validate_footprint = $false
    } -LogBaseName "r156_assign_dry_run_current_sheet"

    if ($assignOut -match 'status=1') {
        $assignOk = Parse-BoolField -Text $assignOut -Key "ok"
        $blocker = Parse-StringField -Text $assignOut -Key "exact_blocker_if_any"
        $globalUpdate = Parse-BoolField -Text $assignOut -Key "global_update"
        $sharedSheet = Parse-BoolField -Text $assignOut -Key "shared_sheet_detected"
        $affectedCount = Parse-IntField -Text $assignOut -Key "affected_instances_count"

        if ($assignOk -eq "true" -and $blocker -eq "N/A" -and $globalUpdate -eq "false" -and $sharedSheet -eq "false" -and [int]$affectedCount -le 1) {
            $assignDryRunOk = "true"
            $assignDryRunGlobalUpdateFalseOk = "true"
            $assignDryRunSharedSheetFalseOk = "true"
        }
    }

    # F — Agent decision (dry-run only; no mutation in R156)
    if ($readinessHierarchyOk -eq "true" -and $navigateOk -eq "true" -and $postNavigationVerified -eq "true" `
            -and $activeSheetContextOk -eq "true" -and $instancePathMatchOk -eq "true" -and $symbolIdMatchOk -eq "true" `
            -and $assignDryRunOk -eq "true") {
        $agentDecision = "mutation_allowed_after_human_or_policy_confirmation"
    }
    else {
        $agentDecision = "do_not_mutate"
    }

    if ($readinessHierarchyOk -ne "true") { $finalBlocker = "readiness_hierarchy_failed" }
    elseif ($navigateOk -ne "true") { $finalBlocker = "navigate_failed" }
    elseif ($postNavigationVerified -ne "true") { $finalBlocker = "post_navigation_verified_failed" }
    elseif ($activeSheetContextOk -ne "true") { $finalBlocker = "active_sheet_context_failed" }
    elseif ($instancePathMatchOk -ne "true") { $finalBlocker = "instance_path_match_failed" }
    elseif ($symbolIdMatchOk -ne "true") { $finalBlocker = "symbol_id_match_failed" }
    elseif ($assignDryRunOk -ne "true") { $finalBlocker = "assign_dry_run_failed" }
    else { $r156Green = "true" }

    $summaryPath = Join-Path $runDir "qa_r156_navigable_sch_footprint_workflow_summary.txt"
    @(
        "r156_workflow_green=$r156Green"
        "readiness_hierarchy_ok=$readinessHierarchyOk"
        "target_instance_path=$targetInstancePath"
        "target_sheet_path=$targetSheetPath"
        "target_symbol_id=$targetSymbolId"
        "target_reference=$targetReference"
        "navigate_ok=$navigateOk"
        "post_navigation_verified=$postNavigationVerified"
        "active_sheet_context_ok=$activeSheetContextOk"
        "instance_path_match_ok=$instancePathMatchOk"
        "symbol_id_match_ok=$symbolIdMatchOk"
        "assign_dry_run_ok=$assignDryRunOk"
        "assign_dry_run_global_update_false_ok=$assignDryRunGlobalUpdateFalseOk"
        "assign_dry_run_shared_sheet_false_ok=$assignDryRunSharedSheetFalseOk"
        "workflow_uses_qa_open_editor_frame_for_navigation=$workflowUsesQaOpenEditorFrameForNavigation"
        "mutation_performed=$mutationPerformed"
        "agent_decision=$agentDecision"
        "exact_blocker_if_any=$finalBlocker"
    ) | Set-Content -Path $summaryPath -Encoding UTF8

    Write-Output "TARGET_RUN_DIR=$runDir"
    Write-Output "SUMMARY_FILE=$summaryPath"
    if ($r156Green -ne "true") { throw "r156_workflow_failed:$finalBlocker" }
}
finally {
    Stop-KiCadProcesses
}
