param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R159 — Spike QA for prototype SetFootprintFieldTextForSheet (QA-only MCP tool).
# Does NOT activate schematic_assign_footprint_session scope=hierarchy mutation.

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r159-footprint-instance-safe-api-spike")
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
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R159 spike"))'),
    ")"
) | Set-Content -Path $fpLibTable -Encoding ASCII

$proj = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_pro"
$rootSch = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_sch"
$rootSchAbs = (Resolve-Path $rootSch).Path
$sharedSymbolId = "c1000005-0005-4005-8005-000000000005"
$spikeFootprint = "Resistor_SMD:R_0201_0603Metric"
$emptyFootprint = ""
$prototypeOption = "A"

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
        $row = @{ symbol_id = ""; instance_path = ""; footprint = ""; reference = "" }
        if ($chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"') { $row.symbol_id = $Matches[1] }
        if ($chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"') { $row.instance_path = $Matches[1] }
        if ($chunk -match 'key:\s+"footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $row.footprint = $Matches[1] }
        if ($chunk -match 'key:\s+"reference"[\s\S]*?string_value:\s*"([^"]+)"') { $row.reference = $Matches[1] }
        if ($row.symbol_id) { $rows += $row }
    }
    return $rows
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
    return @{ ok = ($out -match 'status=1'); rows = (Parse-ReadinessSymbolRows -OutputText $out); raw = $out }
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
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r159" }
    $url = $resolved.Url
    for ($i = 0; $i -lt 60; $i++) {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        if ($pingRes.Text -match 'status=1') { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r159"
    }
    return $url
}

function Invoke-QaSpikeApi {
    param([string]$Url, [hashtable]$SpikeArgs, [string]$LogName)
    $out = Invoke-McpTool -ApiUrl $Url -ToolName "qa_spike_footprint_instance_api" -ToolArgs $SpikeArgs -LogBaseName $LogName
    $ok = "false"
    if ($out -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "absent"
    if ($out -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    return @{ ok = $ok; blocker = $blocker; raw = $out }
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
$instanceSafeApiPrototyped = "false"
$saveReloadTested = "false"
$saveReloadPreserved = "N/A"
$undoRedoTested = "false"
$undoRedoInstanceSafe = "N/A"
$r159Green = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 1

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru

try {
    $url = Connect-KiCadIpc -Process $kicadProc

    $openRootArgs = @{ type = "sch"; filename = $rootSchAbs } | ConvertTo-Json -Compress
    $openRootInput = Join-Path $runDir "qa_open_editor_frame_bootstrap.json"
    $openRootArgs | Set-Content -Path $openRootInput -Encoding UTF8
    $openRootRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes.Text -notmatch 'status=1') { throw "qa_open_editor_frame_bootstrap_failed" }

    $before = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_before"
    if (-not $before.ok) { throw "readiness_before_failed" }

    $sharedRows = @($before.rows | Where-Object { $_.symbol_id -eq $sharedSymbolId })
    if ($sharedRows.Count -lt 2) { throw "expected_2_shared_instances" }

    $instanceA = $sharedRows[0].instance_path
    $instanceB = $sharedRows[1].instance_path
    $instanceABefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceA
    $instanceBBefore = Get-RowFootprint -Rows $before.rows -InstancePath $instanceB

    $writeRes = Invoke-QaSpikeApi -Url $url -LogName "qa_spike_set_footprint_a" -SpikeArgs @{
        action             = "set_footprint_for_sheet"
        schematic_filename = $rootSchAbs
        symbol_id          = $sharedSymbolId
        instance_path      = $instanceA
        footprint          = $spikeFootprint
    }
    if ($writeRes.ok -eq "true") { $instanceSafeApiPrototyped = "true" }
    else { throw "qa_spike_set_footprint_failed:$($writeRes.blocker)" }

    $afterWrite = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_prototype_write"
    if (-not $afterWrite.ok) { throw "readiness_after_write_failed" }

    $instanceAAfter = Get-RowFootprint -Rows $afterWrite.rows -InstancePath $instanceA
    $instanceBAfter = Get-RowFootprint -Rows $afterWrite.rows -InstancePath $instanceB

    if ($instanceAAfter -eq $spikeFootprint) { $instanceAChangedOk = "true" }
    if ($instanceBAfter -eq $instanceBBefore) { $instanceBUnchangedOk = "true" }
    if ($instanceAAfter -eq $spikeFootprint) { $readinessReflectsInstanceWrite = "true" }

    # Undo / redo isolation test
    $undoRedoTested = "true"
    $undoRes = Invoke-QaSpikeApi -Url $url -LogName "qa_spike_undo" -SpikeArgs @{ action = "undo" }
    if ($undoRes.ok -ne "true") { throw "qa_spike_undo_failed:$($undoRes.blocker)" }

    $afterUndo = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_undo"
    $undoA = Get-RowFootprint -Rows $afterUndo.rows -InstancePath $instanceA
    $undoB = Get-RowFootprint -Rows $afterUndo.rows -InstancePath $instanceB

    $redoRes = Invoke-QaSpikeApi -Url $url -LogName "qa_spike_redo" -SpikeArgs @{ action = "redo" }
    if ($redoRes.ok -ne "true") { throw "qa_spike_redo_failed:$($redoRes.blocker)" }

    $afterRedo = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_after_redo"
    $redoA = Get-RowFootprint -Rows $afterRedo.rows -InstancePath $instanceA
    $redoB = Get-RowFootprint -Rows $afterRedo.rows -InstancePath $instanceB

    if ($undoA -eq $emptyFootprint -and $undoB -eq $instanceBBefore -and $redoA -eq $spikeFootprint -and $redoB -eq $instanceBBefore) {
        $undoRedoInstanceSafe = "true"
    }
    else {
        $undoRedoInstanceSafe = "false"
    }

    # Save for reload test
    $saveReloadTested = "true"
    $saveRes = Invoke-QaSpikeApi -Url $url -LogName "qa_spike_save" -SpikeArgs @{ action = "save" }
    if ($saveRes.ok -ne "true") { throw "qa_spike_save_failed:$($saveRes.blocker)" }
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$reloadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru
try {
    $reloadUrl = Connect-KiCadIpc -Process $reloadProc
    $openRootInput2 = Join-Path $runDir "qa_open_editor_frame_reload.json"
    (@{ type = "sch"; filename = $rootSchAbs } | ConvertTo-Json -Compress) | Set-Content -Path $openRootInput2 -Encoding UTF8
    $openRootRes2 = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($reloadUrl, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootInput2)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openRootRes2.Text -match 'status=1') {
        $reloadReadiness = Invoke-ReadinessHierarchy -Url $reloadUrl -LogName "readiness_after_reload"
        if ($reloadReadiness.ok) {
            $reloadA = Get-RowFootprint -Rows $reloadReadiness.rows -InstancePath $instanceA
            $reloadB = Get-RowFootprint -Rows $reloadReadiness.rows -InstancePath $instanceB
            if ($reloadA -eq $spikeFootprint -and $reloadB -eq $instanceBBefore) {
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

if ($instanceAChangedOk -eq "true" -and $instanceBUnchangedOk -eq "true" -and $readinessReflectsInstanceWrite -eq "true") {
    $instanceSafeWriteSupported = "true"
}

if ($instanceSafeApiPrototyped -ne "true") { $finalBlocker = "prototype_api_not_invoked" }
elseif ($instanceAChangedOk -ne "true") { $finalBlocker = "instance_a_not_changed" }
elseif ($instanceBUnchangedOk -ne "true") { $finalBlocker = "instance_b_not_isolated" }
elseif ($readinessReflectsInstanceWrite -ne "true") { $finalBlocker = "readiness_mismatch" }
elseif ($saveReloadPreserved -eq "false") { $finalBlocker = "save_reload_not_preserved" }
elseif ($undoRedoInstanceSafe -eq "false") { $finalBlocker = "undo_redo_not_instance_safe" }
else {
    $r159Green = "true"
    $finalBlocker = "N/A"
}

$summaryPath = Join-Path $runDir "qa_r159_spike_footprint_instance_safe_api_summary.txt"
@(
    "r159_spike_green=$r159Green"
    "instance_safe_api_prototyped=$instanceSafeApiPrototyped"
    "prototype_option=$prototypeOption"
    "instance_safe_write_supported=$instanceSafeWriteSupported"
    "instance_a_path=$instanceA"
    "instance_b_path=$instanceB"
    "instance_a_before=$instanceABefore"
    "instance_a_after=$instanceAAfter"
    "instance_b_before=$instanceBBefore"
    "instance_b_after=$instanceBAfter"
    "instance_a_changed_ok=$instanceAChangedOk"
    "instance_b_unchanged_ok=$instanceBUnchangedOk"
    "readiness_reflects_instance_write=$readinessReflectsInstanceWrite"
    "save_reload_tested=$saveReloadTested"
    "save_reload_preserved=$saveReloadPreserved"
    "undo_redo_tested=$undoRedoTested"
    "undo_redo_instance_safe=$undoRedoInstanceSafe"
    "mcp_hierarchy_mutation_activated=false"
    "exact_blocker_if_any=$finalBlocker"
    "target_run_dir=$runDir"
) | Set-Content -Path $summaryPath -Encoding UTF8

Write-Output "TARGET_RUN_DIR=$runDir"
Write-Output "SUMMARY_FILE=$summaryPath"
if ($r159Green -ne "true") { throw "r159_spike_failed:$finalBlocker" }
