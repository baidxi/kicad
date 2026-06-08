param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R154 — navigate_schematic_sheet_session contract probe (ipc_footprint_readiness_v1).

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

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $hierProj)) { throw "Missing hierarchy project $hierProj" }
if (!(Test-Path $hierRootSch)) { throw "Missing hierarchy schematic $hierRootSch" }

$hierRootSchAbs = (Resolve-Path $hierRootSch).Path
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"
$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-navigate-sheet-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot (Join-Path $BuildDir "ipc-v1-config") -EnableAutomation

function Parse-NavigateSheetOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|absent|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                              = ($Text -match 'status=1')
        navigate_sheet_ok                      = (& $parseLine "navigate_sheet_ok")
        navigate_sheet_dry_run                 = (& $parseLine "navigate_sheet_dry_run")
        navigate_sheet_resolved_sheet_path     = (& $parseLine "navigate_sheet_resolved_sheet_path")
        navigate_sheet_resolved_from           = (& $parseLine "navigate_sheet_resolved_from")
        navigate_sheet_active_sheet_path_before = (& $parseLine "navigate_sheet_active_sheet_path_before")
        navigate_sheet_active_sheet_path_after  = (& $parseLine "navigate_sheet_active_sheet_path_after")
        navigate_sheet_context_changed         = (& $parseLine "navigate_sheet_context_changed")
        navigate_sheet_post_navigation_verified = (& $parseLine "navigate_sheet_post_navigation_verified")
        navigate_sheet_exact_blocker_if_any    = (& $parseLine "navigate_sheet_exact_blocker_if_any")
        navigate_sheet_diagnostics             = (& $parseLine "navigate_sheet_diagnostics")
    }
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
    return @{
        Parsed = Parse-NavigateSheetOutput -Text $out
        Raw    = $out
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
    return $out
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
        $sheetPath = ""
        if ($chunk -match 'key:\s+"sheet_path"[\s\S]*?string_value:\s*"([^"]+)"') {
            $sheetPath = $Matches[1]
        }
        if ($symbolId -and $instancePath) {
            return [pscustomobject]@{
                SymbolId     = $symbolId
                InstancePath = $instancePath
                SheetPath    = $sheetPath
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
    $globalUpdate = "absent"
    if ($OutputText -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*(true|false)') { $globalUpdate = $Matches[1] }
    $sharedSheetDetected = "absent"
    if ($OutputText -match 'key:\s+"shared_sheet_detected"[\s\S]*?bool_value:\s*(true|false)') { $sharedSheetDetected = $Matches[1] }

    return @{
        ok = $ok
        blocker = $blocker
        global_update = $globalUpdate
        shared_sheet_detected = $sharedSheetDetected
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

function Normalize-SheetPathForCompare {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return "" }
    $p = $Path.Trim()
    if ($p -eq "/") { return "/" }
    if (-not $p.EndsWith("/")) { $p = $p + "/" }
    return $p
}

$green = "true"
$finalBlocker = "N/A"

$rootInitialOk = "false"
$dryRunNavigationOk = "false"
$realNavigationOk = "false"
$postNavInstanceMatchOk = "false"
$instancePathNavigationOk = "false"
$postNavAssignDryRunOk = "false"
$returnRootOk = "false"
$negMissingSelectorOk = "false"
$negSheetPathNotFoundOk = "false"
$negInstancePathNotFoundOk = "false"

$targetSheetPath = ""
$targetInstancePath = ""
$targetSymbolId = ""

Stop-KiCadProcesses

$hierFpLibTable = Join-Path (Split-Path $hierRootSch) "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA navigate sheet"))'),
    ")"
) | Set-Content -Path $hierFpLibTable -Encoding ASCII

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru

try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_navigate_sheet" }
    $url = $resolved.Url

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_navigate_sheet"
    }

    $openRootArgs = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_nav_root.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_nav_root_failed" }

    $readinessOut = Invoke-FootprintReadinessSession -ApiUrl $url -ToolArgs @{
        schematic_filename  = $hierRootSchAbs
        scope               = "hierarchy"
        include_sheet_paths = $true
        include_symbols     = $true
        max_symbols         = 50
    } -LogBaseName "nav_readiness_hierarchy"

    $readinessTarget = Parse-ReadinessWithoutFootprintTarget -OutputText $readinessOut
    if ($null -eq $readinessTarget) { throw "nav_readiness_without_footprint_target_not_found" }

    $targetSymbolId = $readinessTarget.SymbolId
    $targetInstancePath = $readinessTarget.InstancePath

    if ($readinessTarget.InstancePath -match '^(.+/)([0-9a-fA-F-]{36})$') {
        $targetSheetPath = Normalize-SheetPathForCompare -Path $Matches[1]
    }
    else {
        throw "nav_target_sheet_path_unparseable"
    }

    $rootContextOut = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $hierRootSchAbs
        include_symbols    = $true
        max_symbols        = 50
    } -LogBaseName "nav_root_active_context"

    if ($rootContextOut -match 'active_sheet_path"[\s\S]*?string_value:\s*"/"' -and $rootContextOut -notmatch ('instance_path"[\s\S]*?string_value:\s*"' + [regex]::Escape($targetInstancePath) + '"')) {
        $rootInitialOk = "true"
    }

    $dryNav = Invoke-NavigateSheetSession -ApiUrl $url -ToolArgs @{
        schematic_filename = $hierRootSchAbs
        sheet_path         = $targetSheetPath
        dry_run            = $true
    } -LogBaseName "nav_dry_run_sub_sheet"

    if ($dryNav.Parsed.navigate_sheet_ok -eq "true" -and $dryNav.Parsed.navigate_sheet_dry_run -eq "true" -and $dryNav.Parsed.navigate_sheet_context_changed -eq "false" -and $dryNav.Parsed.navigate_sheet_active_sheet_path_before -eq "/" -and $dryNav.Parsed.navigate_sheet_active_sheet_path_after -eq "/") {
        $dryRunNavigationOk = "true"
    }

    $rootAfterDry = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $hierRootSchAbs
        include_symbols    = $true
        max_symbols        = 50
    } -LogBaseName "nav_root_after_dry_run_context"

    if ($rootAfterDry -notmatch ('active_sheet_path"[\s\S]*?string_value:\s*"' + [regex]::Escape($targetSheetPath) + '"')) {
        if ($rootAfterDry -match 'active_sheet_path"[\s\S]*?string_value:\s*"/"') {
            # still root after dry-run — expected
        }
        else {
            $dryRunNavigationOk = "false"
        }
    }

    $realNav = Invoke-NavigateSheetSession -ApiUrl $url -ToolArgs @{
        schematic_filename     = $hierRootSchAbs
        sheet_path             = $targetSheetPath
        verify_instance_path   = $targetInstancePath
        dry_run                = $false
    } -LogBaseName "nav_real_sub_sheet"

    $normalizedTargetSheetPath = Normalize-SheetPathForCompare -Path $targetSheetPath
    $normalizedAfter = Normalize-SheetPathForCompare -Path $realNav.Parsed.navigate_sheet_active_sheet_path_after

    if ($realNav.Parsed.navigate_sheet_ok -eq "true" -and $realNav.Parsed.navigate_sheet_dry_run -eq "false" -and $realNav.Parsed.navigate_sheet_context_changed -eq "true" -and $normalizedAfter -eq $normalizedTargetSheetPath -and $realNav.Parsed.navigate_sheet_post_navigation_verified -eq "true") {
        $realNavigationOk = "true"
    }

    $subContextOut = Invoke-ActiveSheetContext -ApiUrl $url -ArgsTable @{
        schematic_filename = $hierRootSchAbs
        include_symbols    = $true
        max_symbols        = 50
    } -LogBaseName "nav_sub_active_context"

    $subSymbols = Parse-ActiveSheetContextSymbols -OutputText $subContextOut
    $instMatch = $subSymbols | Where-Object { $_.InstancePath -eq $targetInstancePath } | Select-Object -First 1
    if ($null -ne $instMatch) {
        $postNavInstanceMatchOk = "true"
    }

    $assignDry = Invoke-AssignFootprintDryRun -ApiUrl $url -ToolArgs @{
        schematic_filename = $hierRootSchAbs
        symbol_id          = $targetSymbolId
        footprint          = "Resistor_SMD:R_0201_0603Metric"
        scope              = "current_sheet"
        dry_run            = $true
        allow_overwrite    = $true
        validate_footprint = $false
    } -LogBaseName "nav_post_assign_dry_run"

    if ($assignDry.ok -eq "true" -and $assignDry.blocker -eq "N/A") {
        if ($assignDry.global_update -eq "false" -and $assignDry.shared_sheet_detected -eq "false") {
            $postNavAssignDryRunOk = "true"
        }
        elseif ($assignDry.global_update -eq "true" -and $assignDry.shared_sheet_detected -eq "true") {
            $postNavAssignDryRunOk = "shared_sheet_global_update_detected"
        }
    }

    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    Stop-KiCadProcesses
    Start-Sleep -Seconds 2
    $null = Clear-IpcSockets

    $kicadProc2 = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru
    try {
        $resolved2 = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc2.Id `
            -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
        if (-not $resolved2.Resolved) { throw "ipc_url_unresolved_navigate_sheet_instance" }
        $url2 = $resolved2.Url

        if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url2 -WorkingDirectory $RepoRoot)) {
            throw "project_not_loaded_navigate_sheet_instance"
        }

        $openRoot2Args = @{ type = "sch"; filename = $hierRootSchAbs } | ConvertTo-Json -Compress
        $openRoot2Input = Join-Path $runDir "qa_open_editor_frame_nav_root2.json"
        $openRoot2Args | Set-Content -Path $openRoot2Input -Encoding UTF8
        $openRootRes2 = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url2, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRoot2Input)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        if ($openRootRes2.Text -notmatch 'status=1') { throw "qa_open_editor_frame_nav_root2_failed" }

        $instNav = Invoke-NavigateSheetSession -ApiUrl $url2 -ToolArgs @{
            schematic_filename = $hierRootSchAbs
            instance_path      = $targetInstancePath
            dry_run            = $false
        } -LogBaseName "nav_real_by_instance_path"

        $instAfter = Normalize-SheetPathForCompare -Path $instNav.Parsed.navigate_sheet_active_sheet_path_after
        if ($instNav.Parsed.navigate_sheet_ok -eq "true" -and $instNav.Parsed.navigate_sheet_resolved_from -eq "instance_path" -and $instAfter -eq $normalizedTargetSheetPath) {
            $instancePathNavigationOk = "true"
        }

        $returnRoot = Invoke-NavigateSheetSession -ApiUrl $url2 -ToolArgs @{
            schematic_filename = $hierRootSchAbs
            sheet_path         = "/"
            dry_run            = $false
        } -LogBaseName "nav_return_root"

        if ($returnRoot.Parsed.navigate_sheet_ok -eq "true" -and $returnRoot.Parsed.navigate_sheet_active_sheet_path_after -eq "/") {
            $returnRootOk = "true"
        }

        $negMissing = Invoke-NavigateSheetSession -ApiUrl $url2 -ToolArgs @{
            schematic_filename = $hierRootSchAbs
            dry_run            = $true
        } -LogBaseName "nav_neg_missing_selector"

        if ($negMissing.Parsed.navigate_sheet_ok -eq "false" -and $negMissing.Parsed.navigate_sheet_exact_blocker_if_any -eq "missing_sheet_selector") {
            $negMissingSelectorOk = "true"
        }

        $negSheet = Invoke-NavigateSheetSession -ApiUrl $url2 -ToolArgs @{
            schematic_filename = $hierRootSchAbs
            sheet_path         = "/00000000-0000-4000-8000-000000000099/"
            dry_run            = $true
        } -LogBaseName "nav_neg_sheet_path_not_found"

        if ($negSheet.Parsed.navigate_sheet_ok -eq "false" -and $negSheet.Parsed.navigate_sheet_exact_blocker_if_any -eq "sheet_path_not_found") {
            $negSheetPathNotFoundOk = "true"
        }

        $negInst = Invoke-NavigateSheetSession -ApiUrl $url2 -ToolArgs @{
            schematic_filename = $hierRootSchAbs
            instance_path      = "/00000000-0000-4000-8000-000000000099/00000000-0000-4000-8000-000000000098"
            dry_run            = $true
        } -LogBaseName "nav_neg_instance_path_not_found"

        if ($negInst.Parsed.navigate_sheet_ok -eq "false" -and $negInst.Parsed.navigate_sheet_exact_blocker_if_any -eq "instance_path_not_found") {
            $negInstancePathNotFoundOk = "true"
        }
    }
    finally {
        Stop-Process -Id $kicadProc2.Id -Force -ErrorAction SilentlyContinue
        Stop-KiCadProcesses
        Start-Sleep -Seconds 1
    }

    if ($rootInitialOk -ne "true") { $green = "false"; $finalBlocker = "root_initial_failed" }
    elseif ($dryRunNavigationOk -ne "true") { $green = "false"; $finalBlocker = "dry_run_navigation_failed" }
    elseif ($realNavigationOk -ne "true") { $green = "false"; $finalBlocker = "real_navigation_failed" }
    elseif ($postNavInstanceMatchOk -ne "true") { $green = "false"; $finalBlocker = "post_nav_instance_match_failed" }
    elseif ($instancePathNavigationOk -ne "true") { $green = "false"; $finalBlocker = "instance_path_navigation_failed" }
    elseif ($postNavAssignDryRunOk -notin @("true", "shared_sheet_global_update_detected")) { $green = "false"; $finalBlocker = "post_nav_assign_dry_run_failed" }
    elseif ($returnRootOk -ne "true") { $green = "false"; $finalBlocker = "return_root_failed" }
    elseif ($negMissingSelectorOk -ne "true") { $green = "false"; $finalBlocker = "neg_missing_selector_failed" }
    elseif ($negSheetPathNotFoundOk -ne "true") { $green = "false"; $finalBlocker = "neg_sheet_path_not_found_failed" }
    elseif ($negInstancePathNotFoundOk -ne "true") { $green = "false"; $finalBlocker = "neg_instance_path_not_found_failed" }

    $summaryPath = Join-Path $runDir "qa_navigate_schematic_sheet_session_contract_summary.txt"
    @(
        "schematic_navigate_sheet_green=$green"
        "exact_blocker_if_any=$finalBlocker"
        "root_initial_ok=$rootInitialOk"
        "dry_run_navigation_ok=$dryRunNavigationOk"
        "real_navigation_ok=$realNavigationOk"
        "post_nav_instance_match_ok=$postNavInstanceMatchOk"
        "instance_path_navigation_ok=$instancePathNavigationOk"
        "post_nav_assign_dry_run_ok=$postNavAssignDryRunOk"
        "return_root_ok=$returnRootOk"
        "neg_missing_selector_ok=$negMissingSelectorOk"
        "neg_sheet_path_not_found_ok=$negSheetPathNotFoundOk"
        "neg_instance_path_not_found_ok=$negInstancePathNotFoundOk"
        "target_sheet_path=$targetSheetPath"
        "target_instance_path=$targetInstancePath"
        "target_symbol_id=$targetSymbolId"
    ) | Set-Content -Path $summaryPath -Encoding UTF8

    Write-Output "TARGET_RUN_DIR=$runDir"
    if ($green -ne "true") { throw "schematic_navigate_sheet_probe_failed:$finalBlocker" }
}
finally {
    Stop-KiCadProcesses
}
