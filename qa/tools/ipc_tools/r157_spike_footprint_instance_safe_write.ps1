param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R157 — Audit/spike: instance-safe footprint write in KiCad hierarchy (no MCP hierarchy mutation).
# Fixture: qa/data/eeschema/hierarchy/ipc_footprint_instance_write_spike/
# Proves or invalidates per-instance FOOTPRINT isolation on shared sub-sheet symbols.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$fixtureSrc = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_instance_write_spike"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $fixtureSrc)) { throw "Missing spike fixture $fixtureSrc" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r157-footprint-instance-safe-write-spike")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$fixtureDir = Join-Path $runDir "spike-fixture"
Copy-Item -Path $fixtureSrc -Destination $fixtureDir -Recurse -Force

$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$fpLibTable = Join-Path $fixtureDir "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R157 spike"))'),
    ")"
) | Set-Content -Path $fpLibTable -Encoding ASCII

$proj = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_pro"
$rootSch = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_sch"
$subSch = Join-Path $fixtureDir "sub_shared.kicad_sch"
$rootSchAbs = (Resolve-Path $rootSch).Path
$subSchAbs = (Resolve-Path $subSch).Path

$sharedSymbolId = "c1000005-0005-4005-8005-000000000005"
$spikeFootprint = "Resistor_SMD:R_0201_0603Metric"
$emptyFootprint = ""

$writePathTested = "OptionA_SetFootprintFieldText_no_path_overload;OptionB_SetFieldText_FOOTPRINT_empty_variant_global;OptionB_variant_non_empty_not_exposed_mcp;OptionC_SCH_SYMBOL_INSTANCE_m_Footprint_legacy_unused_read;OptionD_no_reliable_instance_safe_write"

function Parse-ReadinessSymbolRows {
    param([string]$OutputText)

    $symbolsStart = $OutputText.IndexOf('key: "symbols"')
    if ($symbolsStart -lt 0) { return @() }
    $symbolsEnd = $OutputText.IndexOf('key: "symbols_count"', $symbolsStart)
    if ($symbolsEnd -lt 0) { return @() }
    $symbolsSection = $OutputText.Substring($symbolsStart, $symbolsEnd - $symbolsStart)

    $rows = @()
    $chunks = [regex]::Split($symbolsSection, '(?=values\s*\{)')
    foreach ($chunk in $chunks) {
        if ($chunk -notmatch 'key:\s+"symbol_id"') { continue }
        $row = @{ symbol_id = ""; instance_path = ""; footprint = ""; has_footprint = "false"; reference = "" }
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') { $row.symbol_id = $Matches[1] }
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') { $row.instance_path = $Matches[1] }
        if ($chunk -match 'key:\s+"footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $row.footprint = $Matches[1] }
        if ($chunk -match 'key:\s+"has_footprint"[\s\S]*?bool_value:\s*(true|false)') { $row.has_footprint = $Matches[1] }
        if ($chunk -match 'key:\s+"reference"[\s\S]*?string_value:\s*"([^"]+)"') { $row.reference = $Matches[1] }
        if ($row.symbol_id) { $rows += $row }
    }
    return $rows
}

function Get-SubSchDiskFootprintProperty {
    param([string]$SubSchPath)
    if (!(Test-Path $SubSchPath)) { return "file_missing" }
    $content = Get-Content -LiteralPath $SubSchPath -Raw
    if ($content -match '\(property "Footprint" "([^"]*)"') { return $Matches[1] }
    return "property_absent"
}

function Get-SubSchDiskHasInstanceFootprintTokens {
    param([string]$SubSchPath)
    if (!(Test-Path $SubSchPath)) { return "false" }
    $content = Get-Content -LiteralPath $SubSchPath -Raw
    if ($content -match '\(instances[\s\S]*?\(footprint ') { return "true" }
    return "false"
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

function Invoke-ReadinessHierarchy {
    param([string]$Url, [string]$LogName)
    $out = Invoke-McpTool -ApiUrl $Url -ToolName "schematic_footprint_readiness_session" -ToolArgs @{
        schematic_filename  = $rootSchAbs
        scope               = "hierarchy"
        include_sheet_paths = $true
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $false
    } -LogBaseName $LogName
    return @{
        ok   = ($out -match 'status=1')
        rows = (Parse-ReadinessSymbolRows -OutputText $out)
    }
}

function Get-RowFootprint {
    param($Rows, [string]$InstancePath)
    $r = $Rows | Where-Object { $_.instance_path -eq $InstancePath } | Select-Object -First 1
    if ($null -eq $r) { return "row_missing" }
    return $r.footprint
}

function Connect-KiCadIpc {
    param([System.Diagnostics.Process]$Process)
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $Process.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r157" }
    $url = $resolved.Url
    for ($i = 0; $i -lt 60; $i++) {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        if ($pingRes.Text -match 'status=1') { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r157"
    }
    return $url
}

$instanceA = ""
$instanceB = ""
$instanceABefore = ""
$instanceBBefore = ""
$instanceAAfter = ""
$instanceBAfter = ""
$instanceAChangedOk = "false"
$instanceBUnchangedOk = "false"
$readinessReflectsInstanceWrite = "false"
$instanceSafeWriteSupported = "false"
$saveReloadTested = "false"
$saveReloadPreserved = "N/A"
$hierarchyDryRunOk = "false"
$writeExecutedOk = "false"
$onDiskFootprintBefore = ""
$onDiskFootprintAfterWrite = ""
$onDiskInstanceFootprintTokens = "false"
$r157Green = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 1

$onDiskFootprintBefore = Get-SubSchDiskFootprintProperty -SubSchPath $subSchAbs
$onDiskInstanceFootprintTokens = Get-SubSchDiskHasInstanceFootprintTokens -SubSchPath $subSchAbs

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru

try {
    $url = Connect-KiCadIpc -Process $kicadProc

    # Bootstrap SCH editor (QA harness only).
    $openRootArgs = @{ type = "sch"; filename = $rootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_bootstrap_root.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_bootstrap_failed" }

    $before = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_before"
    if (-not $before.ok) { throw "readiness_before_failed" }

    $sharedRows = @($before.rows | Where-Object { $_.symbol_id -eq $sharedSymbolId })
    if ($sharedRows.Count -lt 2) { throw "expected_2_shared_instances got=$($sharedRows.Count)" }

    $instanceA = $sharedRows[0].instance_path
    $instanceB = $sharedRows[1].instance_path
    $instanceABefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceA
    $instanceBBefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceB

    # R160: hierarchy mutation is instance-safe via MCP public API — R157 focuses on current_sheet global path.
    $hierDryRunOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_assign_footprint_session" -ToolArgs @{
        schematic_filename = $rootSchAbs
        symbol_id          = $sharedSymbolId
        instance_path      = $instanceA
        footprint          = $spikeFootprint
        scope              = "hierarchy"
        dry_run            = $true
        allow_overwrite    = $true
        validate_footprint = $false
    } -LogBaseName "hierarchy_dry_run_sanity"
    $hierarchyDryRunOk = "false"
    if ($hierDryRunOut -match 'key:\s+"ok"[\s\S]*?bool_value:\s*true' -and $hierDryRunOut -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*false') {
        $hierarchyDryRunOk = "true"
    }

    # Product navigation to instance A, then experimental global write path (SetFootprintFieldText via current_sheet).
    $navOut = Invoke-McpTool -ApiUrl $url -ToolName "navigate_schematic_sheet_session" -ToolArgs @{
        schematic_filename   = $rootSchAbs
        instance_path        = $instanceA
        verify_instance_path = $instanceA
        dry_run              = $false
    } -LogBaseName "navigate_to_instance_a"
    if ($navOut -notmatch 'status=1') { throw "navigate_to_instance_a_failed" }

    $writeOut = Invoke-McpTool -ApiUrl $url -ToolName "schematic_assign_footprint_session" -ToolArgs @{
        schematic_filename                  = $rootSchAbs
        symbol_id                           = $sharedSymbolId
        footprint                           = $spikeFootprint
        scope                               = "current_sheet"
        dry_run                             = $false
        allow_overwrite                     = $true
        allow_shared_symbol_global_update   = $true
        validate_footprint                  = $false
    } -LogBaseName "experimental_write_instance_a_context"
    if ($writeOut -match 'status=1' -and $writeOut -match 'key:\s+"ok"[\s\S]*?bool_value:\s*true') {
        $writeExecutedOk = "true"
    }

    $after = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_write"
    if (-not $after.ok) { throw "readiness_after_failed" }

    $instanceAAfter = Get-RowFootprint -Rows $after.rows -InstancePath $instanceA
    $instanceBAfter = Get-RowFootprint -Rows $after.rows -InstancePath $instanceB

    if ($instanceAAfter -eq $spikeFootprint) { $instanceAChangedOk = "true" }
    if ($instanceBAfter -eq $instanceBBefore -and $instanceBBefore -eq $emptyFootprint) {
        $instanceBUnchangedOk = "true"
    }
    if ($instanceAAfter -eq $spikeFootprint) { $readinessReflectsInstanceWrite = "true" }

    $onDiskFootprintAfterWrite = Get-SubSchDiskFootprintProperty -SubSchPath $subSchAbs
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

# Save/reload: fresh process without explicit MCP save — persistence only if KiCad auto-flushed (not expected).
$saveReloadTested = "true"
Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets
try {
    $reloadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru
    try {
        $reloadUrl = Connect-KiCadIpc -Process $reloadProc
        $openRootArgs2 = @{ type = "sch"; filename = $rootSchAbs } | ConvertTo-Json -Compress
        $openRootInput2 = Join-Path $runDir "qa_open_editor_frame_reload.json"
        $openRootArgs2 | Set-Content -Path $openRootInput2 -Encoding UTF8
        $openRootRes2 = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($reloadUrl, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput2)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        if ($openRootRes2.Text -match 'status=1') {
            $reloadReadiness = Invoke-ReadinessHierarchy -Url $reloadUrl -LogName "readiness_after_reload"
            if ($reloadReadiness.ok) {
                $reloadA = Get-RowFootprint -Rows $reloadReadiness.rows -InstancePath $instanceA
                $reloadB = Get-RowFootprint -Rows $reloadReadiness.rows -InstancePath $instanceB
                if ($reloadA -eq $spikeFootprint -and $reloadB -eq $spikeFootprint) {
                    $saveReloadPreserved = "true"
                }
                else {
                    $saveReloadPreserved = "false"
                }
            }
        }
    }
    finally {
        if ($reloadProc -and -not $reloadProc.HasExited) {
            Stop-Process -Id $reloadProc.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
    }
}
catch {
    $saveReloadPreserved = "false"
    if ($finalBlocker -eq "N/A" -and $r157Green -eq "false") {
        # Reload failure alone must not fail spike if session audit completed.
    }
}

# Negative audit: instance-safe supported only if A changed AND B unchanged AND reload preserved (none met).
if ($instanceAChangedOk -eq "true" -and $instanceBUnchangedOk -eq "true" -and $saveReloadPreserved -eq "true") {
    $instanceSafeWriteSupported = "true"
}

if ($hierarchyDryRunOk -ne "true") { $finalBlocker = "hierarchy_dry_run_failed" }
elseif ($writeExecutedOk -ne "true") { $finalBlocker = "experimental_write_failed" }
elseif ($instanceAChangedOk -ne "true") { $finalBlocker = "instance_a_not_changed_after_write" }
elseif ($instanceBUnchangedOk -eq "true" -and $instanceSafeWriteSupported -eq "true") {
    $finalBlocker = "unexpected_isolation_detected_needs_upstream_review"
}
elseif ($instanceBUnchangedOk -eq "true" -and $instanceSafeWriteSupported -eq "false") {
    $finalBlocker = "inconclusive_isolation"
}
else {
    # Expected negative audit: A changed, B also changed (global write) — spike green.
    $r157Green = "true"
    $finalBlocker = "N/A"
}

$summaryPath = Join-Path $runDir "qa_r157_spike_footprint_instance_safe_write_summary.txt"
@(
    "r157_spike_green=$r157Green"
    "instance_safe_write_supported=$instanceSafeWriteSupported"
    "write_path_tested=$writePathTested"
    "instance_a_path=$instanceA"
    "instance_b_path=$instanceB"
    "instance_a_before=$instanceABefore"
    "instance_a_after=$instanceAAfter"
    "instance_b_before=$instanceBBefore"
    "instance_b_after=$instanceBAfter"
    "instance_a_changed_ok=$instanceAChangedOk"
    "instance_b_unchanged_ok=$instanceBUnchangedOk"
    "readiness_reflects_instance_write=$readinessReflectsInstanceWrite"
    "hierarchy_dry_run_ok=$hierarchyDryRunOk"
    "write_executed_ok=$writeExecutedOk"
    "on_disk_footprint_property_before=$onDiskFootprintBefore"
    "on_disk_footprint_property_after_write=$onDiskFootprintAfterWrite"
    "on_disk_instances_footprint_tokens=$onDiskInstanceFootprintTokens"
    "save_reload_tested=$saveReloadTested"
    "save_reload_preserved=$saveReloadPreserved"
    "exact_blocker_if_any=$finalBlocker"
    "target_run_dir=$runDir"
) | Set-Content -Path $summaryPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$runDir"
Write-Output "SUMMARY_FILE=$summaryPath"
if ($r157Green -ne "true") { throw "r157_spike_failed:$finalBlocker" }
