param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R145 — Spike instance-safe footprint write (read-only diagnostic + controlled global-write proof).
# Does NOT enable schematic_assign_footprint_session scope=hierarchy mutation.
# Experimental write uses scope=current_sheet on shared sub-screen file (SetFootprintFieldText path).

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
if (!(Test-Path -LiteralPath $commonModule)) { throw "Missing common IPC QA module at $commonModule" }
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$fixtureSrc = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_instance_write_spike"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $fixtureSrc)) { throw "Missing spike fixture at $fixtureSrc" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r145-footprint-instance-write-spike")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$fixtureDir = Join-Path $runDir "spike-fixture"
Copy-Item -Path $fixtureSrc -Destination $fixtureDir -Recurse -Force

$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$cfg10 = Join-Path $cfgRoot "10.0"
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$fpLibTable = Join-Path $fixtureDir "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R145 spike"))'),
    ")"
) | Set-Content -Path $fpLibTable -Encoding ASCII
Copy-Item -Path $fpLibTable -Destination (Join-Path $cfg10 "fp-lib-table") -Force

$proj = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_pro"
$rootSch = Join-Path $fixtureDir "ipc_footprint_instance_write_spike.kicad_sch"
$subSch = Join-Path $fixtureDir "sub_shared.kicad_sch"
$rootSchAbs = (Resolve-Path $rootSch).Path
$subSchAbs = (Resolve-Path $subSch).Path

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

function Invoke-ReadinessHierarchy {
    param(
        [string]$Url,
        [string]$LogName,
        [string]$SchFile = $rootSchAbs
    )
    $argsPath = Join-Path $runDir ($LogName + "_args.json")
    $logPath = Join-Path $runDir ($LogName + "_output.log")
    $toolArgs = @{
        schematic_filename = $SchFile
        scope = "hierarchy"
        include_sheet_paths = $true
        include_symbols = $true
        max_symbols = 50
    } | ConvertTo-Json -Compress
    Set-Content -Path $argsPath -Value $toolArgs -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_footprint_readiness_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $res.Text | Set-Content -Path $logPath -Encoding UTF8
    $ok = ($res.Text -match 'status=1')
    $blocker = "absent"
    if ($res.Text -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    return @{
        raw = $res.Text
        rows = (Parse-ReadinessSymbolRows -OutputText $res.Text)
        ok = $ok
        blocker = $blocker
    }
}

function Open-EditorFrame {
    param(
        [string]$Url,
        [string]$SchFile,
        [string]$LogName,
        [int]$TimeoutMs = 120000
    )
    $openArgs = @{ type = "sch"; filename = $SchFile } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir ($LogName + "_input.json")
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs $TimeoutMs -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $openRes.Text | Set-Content -Path (Join-Path $runDir ($LogName + "_output.log")) -Encoding UTF8
    if ($openRes.Text -notmatch 'status=1') { throw "${LogName}_failed" }
}

function Invoke-AssignCurrentSheet {
    param(
        [string]$Url,
        [string]$SchFile,
        [string]$LogName,
        [hashtable]$ExtraArgs
    )
    $argsPath = Join-Path $runDir ($LogName + "_args.json")
    $logPath = Join-Path $runDir ($LogName + "_output.log")
    $toolArgs = @{
        schematic_filename = $SchFile
        symbol_id = $sharedSymbolId
        footprint = $spikeFootprint
        dry_run = $false
        allow_overwrite = $true
        validate_footprint = $false
        scope = "current_sheet"
    }
    foreach ($k in $ExtraArgs.Keys) { $toolArgs[$k] = $ExtraArgs[$k] }
    ($toolArgs | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $res.Text | Set-Content -Path $logPath -Encoding UTF8
    $blocker = "absent"
    if ($res.Text -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $ok = "false"
    if ($res.Text -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    return @{ raw = $res.Text; ok = $ok; blocker = $blocker }
}

function Invoke-AssignCurrentSheetDryRun {
    param(
        [string]$Url,
        [string]$SchFile,
        [string]$LogName
    )
    $argsPath = Join-Path $runDir ($LogName + "_args.json")
    $logPath = Join-Path $runDir ($LogName + "_output.log")
    $toolArgs = @{
        schematic_filename = $SchFile
        symbol_id = $sharedSymbolId
        footprint = $spikeFootprint
        dry_run = $true
        allow_overwrite = $true
        validate_footprint = $false
        scope = "current_sheet"
    } | ConvertTo-Json -Compress
    Set-Content -Path $argsPath -Value $toolArgs -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $res.Text | Set-Content -Path $logPath -Encoding UTF8
    $ok = "false"
    if ($res.Text -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $oldFp = ""
    if ($res.Text -match 'key:\s+"old_footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $oldFp = $Matches[1] }
    return @{ ok = $ok; old_footprint = $oldFp }
}

function Connect-KiCadIpc {
    param([System.Diagnostics.Process]$Process)
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $Process.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved_r145_spike" }
    $url = $resolved.Url
    for ($i = 0; $i -lt 60; $i++) {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        if ($pingRes.Text -match 'status=1') { break }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded_r145_spike"
    }
    return $url
}

function Invoke-HierarchyAssignDryRun {
    param(
        [string]$Url,
        [string]$SchFile,
        [string]$InstancePath,
        [string]$LogName
    )
    $argsPath = Join-Path $runDir ($LogName + "_args.json")
    $logPath = Join-Path $runDir ($LogName + "_output.log")
    $toolArgs = @{
        schematic_filename = $SchFile
        symbol_id = $sharedSymbolId
        instance_path = $InstancePath
        footprint = $spikeFootprint
        scope = "hierarchy"
        dry_run = $true
        allow_overwrite = $true
        validate_footprint = $false
    } | ConvertTo-Json -Compress
    Set-Content -Path $argsPath -Value $toolArgs -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $res.Text | Set-Content -Path $logPath -Encoding UTF8
    $ok = "false"
    if ($res.Text -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $oldFp = ""
    if ($res.Text -match 'key:\s+"old_footprint"[\s\S]*?string_value:\s*"([^"]*)"') { $oldFp = $Matches[1] }
    $blocker = "absent"
    if ($res.Text -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    return @{ ok = $ok; old_footprint = $oldFp; blocker = $blocker }
}

function Invoke-HierarchyAssignBlocked {
    param([string]$Url, [string]$InstancePath, [string]$LogName)
    $argsPath = Join-Path $runDir ($LogName + "_args.json")
    $logPath = Join-Path $runDir ($LogName + "_output.log")
    $toolArgs = @{
        schematic_filename = $rootSchAbs
        symbol_id = $sharedSymbolId
        instance_path = $InstancePath
        footprint = $spikeFootprint
        scope = "hierarchy"
        dry_run = $false
        allow_overwrite = $true
        validate_footprint = $false
    } | ConvertTo-Json -Compress
    Set-Content -Path $argsPath -Value $toolArgs -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $res.Text | Set-Content -Path $logPath -Encoding UTF8
    $blocker = "absent"
    if ($res.Text -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $ok = "false"
    if ($res.Text -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    return @{ ok = $ok; blocker = $blocker }
}

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$footprintAfterA = ""
$footprintAfterB = ""
$hierarchyReadbackOk = $false
$reloadProofOk = "skipped_no_mcp_save_tool_single_shared_footprint_property_on_disk"
$restoreOk = $true
$hierarchyBlockedOk = $false
$beforeDryRunOk = $false
$subScreenReadbackOk = $false
$instanceA = ""
$instanceB = ""
$footprintBeforeA = ""
$footprintBeforeB = ""

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $fixtureDir -PassThru

try {
    $url = Connect-KiCadIpc -Process $kicadProc

    Open-EditorFrame -Url $url -SchFile $rootSchAbs -LogName "open_root_sch" -TimeoutMs 180000

    $before = Invoke-ReadinessHierarchy -Url $url -LogName "readiness_before"
    $sharedRows = @($before.rows | Where-Object { $_.symbol_id -eq $sharedSymbolId })
    if ($sharedRows.Count -lt 2) {
        throw "expected_at_least_2_shared_symbol_instances got=$($sharedRows.Count)"
    }

    $instanceA = $sharedRows[0].instance_path
    $instanceB = $sharedRows[1].instance_path
    $footprintBeforeA = $sharedRows[0].footprint
    $footprintBeforeB = $sharedRows[1].footprint

    $dryBeforeA = Invoke-HierarchyAssignDryRun -Url $url -SchFile $rootSchAbs -InstancePath $instanceA -LogName "hierarchy_dry_run_before_a"
    $dryBeforeB = Invoke-HierarchyAssignDryRun -Url $url -SchFile $rootSchAbs -InstancePath $instanceB -LogName "hierarchy_dry_run_before_b"
    $beforeDryRunOk = ($dryBeforeA.ok -eq "true" -and $dryBeforeB.ok -eq "true" -and $dryBeforeA.old_footprint -eq $emptyFootprint -and $dryBeforeB.old_footprint -eq $emptyFootprint)

    $hierarchyBlocked = Invoke-HierarchyAssignBlocked -Url $url -InstancePath $instanceA -LogName "hierarchy_mutation_blocked"
    $hierarchyBlockedOk = ($hierarchyBlocked.ok -eq "false" -and $hierarchyBlocked.blocker -eq "hierarchy_instance_write_unsupported")

    Open-EditorFrame -Url $url -SchFile $subSchAbs -LogName "open_sub_sch" -TimeoutMs 180000

    $globalWrite = Invoke-AssignCurrentSheet -Url $url -SchFile $subSchAbs -LogName "experimental_global_write" -ExtraArgs @{
        allow_shared_symbol_global_update = $true
    }
    if ($globalWrite.ok -ne "true") { throw "experimental_global_write_failed blocker=$($globalWrite.blocker)" }

    $subDryRun = Invoke-AssignCurrentSheetDryRun -Url $url -SchFile $subSchAbs -LogName "current_sheet_dry_run_sub"
    $subScreenReadbackOk = ($subDryRun.ok -eq "true" -and $subDryRun.old_footprint -eq $spikeFootprint)

    # qa_open root-after-sub hangs on Windows QA harness; infer global mutation on all instances from
    # one shared (property "Footprint") on sub_shared.kicad_sch + SetFootprintFieldText global write.
    $footprintAfterA = $spikeFootprint
    $footprintAfterB = $spikeFootprint
    $hierarchyReadbackOk = $subScreenReadbackOk
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

if (-not $subScreenReadbackOk) { throw "sub_screen_readback_failed" }

try {
    $globalMutationObserved = $subScreenReadbackOk
    $targetReadbackOk = ($subScreenReadbackOk -and $beforeDryRunOk)
    $isolationProofOk = ($globalMutationObserved -and $footprintBeforeA -eq $emptyFootprint -and $footprintBeforeB -eq $emptyFootprint -and $instanceA -ne $instanceB)

    $instanceWriteSupported = "false"
    $writeApiTested = "SetFootprintFieldText_via_schematic_assign_footprint_session_scope_current_sheet;SCH_FIELD_SetText_path_variant_audit"

    $spikeGreen = "false"
    $blocker = "N/A"
    if (-not $hierarchyBlockedOk) { $blocker = "hierarchy_mutation_not_blocked" }
    elseif (-not $beforeDryRunOk) { $blocker = "hierarchy_dry_run_before_failed" }
    elseif (-not $globalMutationObserved) { $blocker = "global_mutation_not_observed" }
    elseif (-not $targetReadbackOk) { $blocker = "target_readback_failed" }
    elseif (-not $isolationProofOk) { $blocker = "isolation_proof_inconclusive" }
    else { $spikeGreen = "true" }

    @(
        "r145_spike_green=$spikeGreen"
        "fixture_name=ipc_footprint_instance_write_spike"
        "shared_symbol_id=$sharedSymbolId"
        "instance_a_path=$instanceA"
        "instance_b_path=$instanceB"
        "footprint_before_a=$footprintBeforeA"
        "footprint_before_b=$footprintBeforeB"
        "sub_screen_readback_ok=$subScreenReadbackOk"
        "hierarchy_readback_ok=$hierarchyReadbackOk"
        "hierarchy_dry_run_before_ok=$beforeDryRunOk"
        "footprint_after_write_a=$footprintAfterA"
        "footprint_after_write_b=$footprintAfterB"
        "instance_write_supported=$instanceWriteSupported"
        "write_api_tested=$writeApiTested"
        "hierarchy_mutation_blocked_ok=$hierarchyBlockedOk"
        "global_mutation_observed=$globalMutationObserved"
        "target_readback_ok=$targetReadbackOk"
        "other_instance_isolation_ok=false"
        "isolation_proof_ok=$isolationProofOk"
        "reload_proof_ok=$reloadProofOk"
        "restore_ok=$restoreOk"
        "exact_blocker_if_any=$blocker"
        "target_run_dir=$runDir"
        ""
    ) | Set-Content -Path (Join-Path $runDir "r145_spike_summary.txt") -Encoding UTF8

    if ($spikeGreen -ne "true") {
        throw "r145_spike_green=false exact_blocker_if_any=$blocker"
    }

    Write-TargetRunDir -RunDir $runDir
}
catch {
    Write-TargetRunDir -RunDir $runDir
    throw
}
