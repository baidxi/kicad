param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R160 — Targeted probe for schematic_assign_footprint_session scope=hierarchy instance-safe mutation.

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
if (!(Test-Path $fixtureSrc)) { throw "Missing fixture $fixtureSrc" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-assign-footprint-hierarchy-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$fixtureDir = Join-Path $runDir "hierarchy-fixture"
Copy-Item -Path $fixtureSrc -Destination $fixtureDir -Recurse -Force

$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$fpLibTable = Join-Path $fixtureDir "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R160 hierarchy assign"))'),
    ")"
) | Set-Content -Path $fpLibTable -Encoding ASCII

$proj = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_pro"
$rootSch = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_sch"
$rootSchAbs = (Resolve-Path $rootSch).Path
$sharedSymbolId = "c1000005-0005-4005-8005-000000000005"
$spikeFootprint = "Resistor_SMD:R_0201_0603Metric"
$emptyFootprint = ""

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
        $row = @{ symbol_id = ""; instance_path = ""; footprint = "" }
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') { $row.symbol_id = $Matches[1] }
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') { $row.instance_path = $Matches[1] }
        if ($chunk -match 'key:\s+"footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $row.footprint = $Matches[1] }
        if ($row.symbol_id) { $rows += $row }
    }
    return $rows
}

function Parse-AssignFootprintResult {
    param([string]$OutputText)
    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if ($OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $changed = "false"
    if ($OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)') { $changed = $Matches[1] }
    $blocker = "absent"
    if ($OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $scope = "absent"
    if ($OutputText -match 'key:\s+"scope"[\s\S]*?string_value:\s*"([^"]*)"') { $scope = $Matches[1] }
    $globalUpdate = "absent"
    if ($OutputText -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*(true|false)') { $globalUpdate = $Matches[1] }
    $instanceSafeWriteUsed = "absent"
    if ($OutputText -match 'key:\s+"instance_safe_write_used"[\s\S]*?bool_value:\s*(true|false)') { $instanceSafeWriteUsed = $Matches[1] }
    $oldFootprint = ""
    if ($OutputText -match 'key:\s+"old_footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $oldFootprint = $Matches[1] }
    return @{
        status_ok = $statusOk; ok = $ok; changed = $changed; blocker = $blocker
        scope = $scope; global_update = $globalUpdate
        instance_safe_write_used = $instanceSafeWriteUsed; old_footprint = $oldFootprint
    }
}

function Invoke-McpTool {
    param([string]$ApiUrl, [string]$ToolName, [hashtable]$ToolArgs, [string]$LogBaseName)
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ToolArgs | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", $ToolName, ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return $out
}

function Invoke-Assign {
    param([string]$Url, [hashtable]$ToolArgs, [string]$LogName)
    $out = Invoke-McpTool -ApiUrl $Url -ToolName "schematic_assign_footprint_session" -ToolArgs $ToolArgs -LogBaseName $LogName
    return Parse-AssignFootprintResult -OutputText $out
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
    return @{ ok = ($out -match 'status=1'); rows = (Parse-ReadinessSymbolRows -OutputText $out) }
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
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r160_hierarchy" }
    $url = $resolved.Url
    for ($i = 0; $i -lt 60; $i++) {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        if ($pingRes.Text -match 'status=1') { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r160_hierarchy"
    }
    return $url
}

$hierarchyDryRunOk = "false"
$hierarchyAssignInstanceSafeOk = "false"
$hierarchyAssignInstanceAChangedOk = "false"
$hierarchyAssignInstanceBUnchangedOk = "false"
$hierarchyAssignGlobalUpdateFalseOk = "false"
$hierarchyRestoreOk = "false"
$familyGreen = "false"
$blocker = "N/A"
$instanceA = ""
$instanceB = ""

Stop-KiCadProcesses
Start-Sleep -Seconds 1

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru
try {
    $url = Connect-KiCadIpc -Process $kicadProc

    $openRootInput = Join-Path $runDir "qa_open_editor_frame.json"
    (@{ type = "sch"; filename = $rootSchAbs } | ConvertTo-Json -Compress) | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }

    $before = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_before"
    if (-not $before.ok) { throw "readiness_before_failed" }

    $sharedRows = @($before.rows | Where-Object { $_.symbol_id -eq $sharedSymbolId })
    if ($sharedRows.Count -lt 2) { throw "expected_2_shared_instances" }

    $instanceA = $sharedRows[0].instance_path
    $instanceB = $sharedRows[1].instance_path
    $instanceABefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceA
    $instanceBBefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceB

    $dryRun = Invoke-Assign -Url $url -LogName "hierarchy_dry_run" -ToolArgs @{
        schematic_filename = $rootSchAbs
        symbol_id          = $sharedSymbolId
        instance_path      = $instanceA
        footprint          = $spikeFootprint
        scope              = "hierarchy"
        dry_run            = $true
        allow_overwrite      = $true
        validate_footprint = $false
    }
    if ($dryRun.status_ok -and $dryRun.ok -eq "true" -and $dryRun.changed -eq "true" `
            -and $dryRun.scope -eq "hierarchy" -and $dryRun.global_update -eq "false" `
            -and $dryRun.blocker -eq "N/A") {
        $hierarchyDryRunOk = "true"
    }

    $afterDryRun = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_dry_run"
    $dryRunA = Get-RowFootprint -Rows $afterDryRun.rows -InstancePath $instanceA
    $dryRunB = Get-RowFootprint -Rows $afterDryRun.rows -InstancePath $instanceB
    if ($dryRunA -ne $instanceABefore -or $dryRunB -ne $instanceBBefore) {
        $hierarchyDryRunOk = "false"
    }

    $mutation = Invoke-Assign -Url $url -LogName "hierarchy_mutation_a" -ToolArgs @{
        schematic_filename = $rootSchAbs
        symbol_id          = $sharedSymbolId
        instance_path      = $instanceA
        footprint          = $spikeFootprint
        scope              = "hierarchy"
        dry_run            = $false
        allow_overwrite    = $true
        validate_footprint = $false
        expected_current_footprint = $emptyFootprint
    }
    if ($mutation.status_ok -and $mutation.ok -eq "true" -and $mutation.changed -eq "true" `
            -and $mutation.scope -eq "hierarchy" -and $mutation.global_update -eq "false" `
            -and $mutation.instance_safe_write_used -eq "true" -and $mutation.blocker -eq "N/A") {
        $hierarchyAssignInstanceSafeOk = "true"
        $hierarchyAssignGlobalUpdateFalseOk = "true"
    }

    $afterMut = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_mutation"
    $instanceAAfter = Get-RowFootprint -Rows $afterMut.rows -InstancePath $instanceA
    $instanceBAfter = Get-RowFootprint -Rows $afterMut.rows -InstancePath $instanceB
    if ($instanceAAfter -eq $spikeFootprint) { $hierarchyAssignInstanceAChangedOk = "true" }
    if ($instanceBAfter -eq $instanceBBefore) { $hierarchyAssignInstanceBUnchangedOk = "true" }

    $restore = Invoke-Assign -Url $url -LogName "hierarchy_restore_a" -ToolArgs @{
        schematic_filename = $rootSchAbs
        symbol_id          = $sharedSymbolId
        instance_path      = $instanceA
        footprint          = $emptyFootprint
        scope              = "hierarchy"
        dry_run            = $false
        allow_overwrite    = $true
        validate_footprint = $false
        expected_current_footprint = $spikeFootprint
    }
    $afterRestore = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_restore"
    $restoreA = Get-RowFootprint -Rows $afterRestore.rows -InstancePath $instanceA
    $restoreB = Get-RowFootprint -Rows $afterRestore.rows -InstancePath $instanceB
    if ($restore.status_ok -and $restore.ok -eq "true" -and $restore.changed -eq "true" `
            -and $restoreA -eq $emptyFootprint -and $restoreB -eq $instanceBBefore) {
        $hierarchyRestoreOk = "true"
    }
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

if ($hierarchyDryRunOk -ne "true") { $blocker = "hierarchy_dry_run_failed" }
elseif ($hierarchyAssignInstanceSafeOk -ne "true") { $blocker = "hierarchy_mutation_failed" }
elseif ($hierarchyAssignInstanceAChangedOk -ne "true") { $blocker = "instance_a_not_changed" }
elseif ($hierarchyAssignInstanceBUnchangedOk -ne "true") { $blocker = "instance_b_not_isolated" }
elseif ($hierarchyAssignGlobalUpdateFalseOk -ne "true") { $blocker = "global_update_not_false" }
elseif ($hierarchyRestoreOk -ne "true") { $blocker = "hierarchy_restore_failed" }
else {
    $familyGreen = "true"
    $blocker = "N/A"
}

$summaryPath = Join-Path $runDir "qa_schematic_assign_footprint_hierarchy_summary.txt"
@(
    "schematic_assign_footprint_hierarchy_green=$familyGreen"
    "hierarchy_dry_run_ok=$hierarchyDryRunOk"
    "hierarchy_assign_instance_safe_ok=$hierarchyAssignInstanceSafeOk"
    "hierarchy_assign_instance_a_changed_ok=$hierarchyAssignInstanceAChangedOk"
    "hierarchy_assign_instance_b_unchanged_ok=$hierarchyAssignInstanceBUnchangedOk"
    "hierarchy_assign_global_update_false_ok=$hierarchyAssignGlobalUpdateFalseOk"
    "hierarchy_restore_ok=$hierarchyRestoreOk"
    "instance_a_path=$instanceA"
    "instance_b_path=$instanceB"
    "exact_blocker_if_any=$blocker"
    "target_run_dir=$runDir"
) | Set-Content -Path $summaryPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$runDir"
Write-Output "SUMMARY_FILE=$summaryPath"
if ($familyGreen -ne "true") { throw "schematic_assign_footprint_hierarchy_green=false exact_blocker_if_any=$blocker" }
