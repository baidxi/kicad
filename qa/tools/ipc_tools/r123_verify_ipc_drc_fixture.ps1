param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

<#
.SYNOPSIS
  R123 — Vérifie la fixture ipc_drc_v1 avec les tools MCP existants (sans get_pcb_drc_violations_context_session).

.DESCRIPTION
  Copie qa/data/pcbnew/drc/ipc_drc_v1 vers build/ipc-drc-v1-proj, ouvre le PCB,
  valide qa_open_editor_frame, pcb_get_board_items_session, pcb_get_board_item_by_id_session,
  get_pcb_item_geometry_session, pcb_measure_distance_session, get_pcb_clearance_context_session,
  get_pcb_nearby_items_context_session, get_net_classes, get_nets.

  Ne prétend pas valider une DRC officielle — prépare la fixture pour R124+.
#>

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
    if (!(Test-Path -LiteralPath $src)) {
        throw "Missing ipc_drc_v1 fixture source at $src"
    }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcDrcV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-drc-v1-proj\ipc_drc_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

$kidA = "f0d1a001-0001-4001-8001-0000000000a1"
$kidB = "f0d1b002-0002-4002-8002-0000000000b2"
$netA = "QA_DRC_NET_A"
$netB = "QA_DRC_NET_B"
$expDistNm = 115000000.0
$expNominalClearanceNm = 30000000.0
$nearbyRadiusNm = 150000000

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $pcb)) { throw "Missing board $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r123-ipc-drc-fixture")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

function Parse-MeasureDistanceOutput {
    param([string]$Text)
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?number_value:\s*([-0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    return @{
        status_ok   = ($Text -match 'status=1')
        ok          = $(if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $Matches[1] } else { "" })
        distance_nm = & $parseNum "distance_nm"
        dx_nm       = & $parseNum "dx_nm"
        dy_nm       = & $parseNum "dy_nm"
    }
}

function Parse-ClearanceContextOutput {
    param([string]$Text)
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?number_value:\s*([-0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    $parseStr = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?string_value:\s*"([^"]*)"')) {
            return $Matches[1]
        }
        return ""
    }
    $trueDrc = ""
    if ($Text -match 'key:\s+"is_true_drc"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trueDrc = $Matches[1] }
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    return @{
        status_ok             = ($Text -match 'status=1')
        ok                    = $ok
        net_name              = (& $parseStr "net_name")
        other_net_name        = (& $parseStr "other_net_name")
        netclass_name         = (& $parseStr "netclass_name")
        other_netclass_name   = (& $parseStr "other_netclass_name")
        required_clearance_nm = (& $parseNum "required_clearance_nm")
        is_true_drc           = $trueDrc
    }
}

function Get-OrderedNearbyItemIds {
    param([string]$Text)
    $ids = [System.Collections.Generic.List[string]]::new()
    $segment = $Text
    $ix = $Text.IndexOf('key: "items"')
    if ($ix -ge 0) { $segment = $Text.Substring($ix) }
    $rx = [regex]::new('key:\s+"item_id"[\s\S]{0,800}?string_value:\s*"([^"]+)"', 'Singleline')
    foreach ($m in $rx.Matches($segment)) { $ids.Add($m.Groups[1].Value) | Out-Null }
    return $ids
}

$failures = @()
$measuredDistanceNm = $null
$nominalClearanceNm = $null

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "IPC URL unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $openSm.Text | Set-Content (Join-Path $runDir "qa_open_editor_frame.log") -Encoding UTF8
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame failed" }
    Start-Sleep -Seconds 3

    # pcb_get_board_items_session
    $biArgs = Join-Path $runDir "pcb_get_board_items.args.json"
    (@{ board_filename = $pcbAbs; max_items = 50 } | ConvertTo-Json -Compress) | Set-Content $biArgs -Encoding UTF8
    $biSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $biArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $biSm.Text | Set-Content (Join-Path $runDir "pcb_get_board_items.log") -Encoding UTF8
    if ($biSm.Text -notmatch 'status=1') { $failures += "pcb_get_board_items_session status" }
    if ($biSm.Text -notmatch 'key:\s+"pad_count"[\s\S]{0,800}?number_value:\s*2') { $failures += "pcb_get_board_items_session pad_count expected 2" }

    foreach ($pair in @(
            @{ id = $kidA; net = $netA }
            @{ id = $kidB; net = $netB }
        )) {
        $argsPath = Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.net + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_item_by_id_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.net + ".log")) -Encoding UTF8
        if ($sm.Text -notmatch 'status=1') { $failures += "pcb_get_board_item_by_id $($pair.id) status" }
        $netPat = 'key:\s+"net_name"[\s\S]{0,500}?string_value:\s*"' + [regex]::Escape($pair.net) + '"'
        if ($sm.Text -notmatch $netPat) { $failures += "pcb_get_board_item_by_id net_name $($pair.net)" }

        $geomPath = Join-Path $runDir ("get_pcb_item_geometry_" + $pair.net + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $geomPath -Encoding UTF8
        $gSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_item_geometry_session", ("@" + $geomPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $gSm.Text | Set-Content (Join-Path $runDir ("get_pcb_item_geometry_" + $pair.net + ".log")) -Encoding UTF8
        if ($gSm.Text -notmatch 'status=1') { $failures += "get_pcb_item_geometry $($pair.id) status" }
    }

    # pcb_measure_distance_session — centre–centre 1.15 mm
    $measurePath = Join-Path $runDir "pcb_measure_drc_pads.args.json"
    (@{
        board_filename = $pcbAbs
        a              = @{ item_id = $kidA }
        b              = @{ item_id = $kidB }
    } | ConvertTo-Json -Compress -Depth 8) | Set-Content $measurePath -Encoding UTF8
    $mSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $measurePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $mSm.Text | Set-Content (Join-Path $runDir "pcb_measure_drc_pads.log") -Encoding UTF8
    $meas = Parse-MeasureDistanceOutput -Text $mSm.Text
    if (-not $meas.status_ok -or $meas.ok -ne "true") { $failures += "pcb_measure_distance status/ok" }
    if ($null -eq $meas.distance_nm -or [math]::Abs($meas.distance_nm - $expDistNm) -gt 500.0) {
        $failures += ("pcb_measure_distance distance_nm expected ~{0} got {1}" -f $expDistNm, $meas.distance_nm)
    }
    $measuredDistanceNm = $meas.distance_nm

    # get_pcb_clearance_context_session — single pad A
    $ccSinglePath = Join-Path $runDir "clearance_single_a.args.json"
    (@{ board_filename = $pcbAbs; item_id = $kidA } | ConvertTo-Json -Compress) | Set-Content $ccSinglePath -Encoding UTF8
    $ccSingleSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_clearance_context_session", ("@" + $ccSinglePath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $ccSingleSm.Text | Set-Content (Join-Path $runDir "clearance_single_a.log") -Encoding UTF8
    $ccSingle = Parse-ClearanceContextOutput -Text $ccSingleSm.Text
    if (-not $ccSingle.status_ok -or $ccSingle.ok -ne "true") { $failures += "clearance single status/ok" }
    if ($ccSingle.net_name -ne $netA) { $failures += "clearance single net_name" }
    if ($ccSingle.netclass_name -ne "QA_DRC_NETCLASS_A") { $failures += "clearance single netclass_name" }
    if ($ccSingle.is_true_drc -ne "false") { $failures += "clearance single is_true_drc must be false" }

    # get_pcb_clearance_context_session — pair A/B
    $ccPairPath = Join-Path $runDir "clearance_pair_ab.args.json"
    (@{ board_filename = $pcbAbs; item_id = $kidA; other_item_id = $kidB } | ConvertTo-Json -Compress) | Set-Content $ccPairPath -Encoding UTF8
    $ccPairSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_clearance_context_session", ("@" + $ccPairPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $ccPairSm.Text | Set-Content (Join-Path $runDir "clearance_pair_ab.log") -Encoding UTF8
    $ccPair = Parse-ClearanceContextOutput -Text $ccPairSm.Text
    if (-not $ccPair.status_ok -or $ccPair.ok -ne "true") { $failures += "clearance pair status/ok" }
    if ($ccPair.net_name -ne $netA -or $ccPair.other_net_name -ne $netB) { $failures += "clearance pair net names" }
    if ($ccPair.is_true_drc -ne "false") { $failures += "clearance pair is_true_drc must be false" }
    if ($null -eq $ccPair.required_clearance_nm -or [math]::Abs($ccPair.required_clearance_nm - $expNominalClearanceNm) -gt 500.0) {
        $failures += ("clearance pair required_clearance_nm expected ~{0} got {1}" -f $expNominalClearanceNm, $ccPair.required_clearance_nm)
    }
    $nominalClearanceNm = $ccPair.required_clearance_nm

    # get_pcb_nearby_items_context_session — pad A radius includes pad B
    $nbPath = Join-Path $runDir "nearby_from_a.args.json"
    (@{
        board_filename  = $pcbAbs
        item_id         = $kidA
        radius_nm       = $nearbyRadiusNm
        max_items       = 10
        include_same_net = $true
    } | ConvertTo-Json -Compress) | Set-Content $nbPath -Encoding UTF8
    $nbSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_nearby_items_context_session", ("@" + $nbPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $nbSm.Text | Set-Content (Join-Path $runDir "nearby_from_a.log") -Encoding UTF8
    if ($nbSm.Text -notmatch 'status=1') { $failures += "nearby status" }
    if ($nbSm.Text -notmatch 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*true') { $failures += "nearby ok" }
    if ($nbSm.Text -notmatch 'key:\s+"is_true_drc"[\s\S]{0,800}?bool_value:\s*false') { $failures += "nearby is_true_drc" }
    $nearbyIds = Get-OrderedNearbyItemIds -Text $nbSm.Text
    if (-not ($nearbyIds -contains $kidB)) { $failures += "nearby missing pad B in radius" }

    # get_net_classes / get_nets (best-effort project nets)
    $ncArgs = Join-Path $runDir "get_net_classes.args.json"
    Set-Content -Path $ncArgs -Value '{}' -Encoding UTF8
    $ncSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_net_classes", ("@" + $ncArgs)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $ncSm.Text | Set-Content (Join-Path $runDir "get_net_classes.log") -Encoding UTF8
    foreach ($nc in @("QA_DRC_NETCLASS_A", "QA_DRC_NETCLASS_B")) {
        if ($ncSm.Text -notmatch [regex]::Escape($nc)) { $failures += "get_net_classes missing $nc" }
    }

    $netsArgs = Join-Path $runDir "get_nets.args.json"
    (@{ board_filename = $pcbAbs } | ConvertTo-Json -Compress) | Set-Content $netsArgs -Encoding UTF8
    $netsSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_nets", ("@" + $netsArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $netsSm.Text | Set-Content (Join-Path $runDir "get_nets.log") -Encoding UTF8
    foreach ($nn in @($netA, $netB)) {
        if ($netsSm.Text -notmatch [regex]::Escape($nn)) { $failures += "get_nets missing $nn" }
    }

    $green = ($failures.Count -eq 0)
    $blocker = if ($green) { "N/A" } else { $failures[0] }

    @(
        "ipc_drc_v1_fixture_ok=$( if ($green) { 'true' } else { 'false' } )"
        "pad_a_kiid=$kidA"
        "pad_b_kiid=$kidB"
        "net_a=$netA"
        "net_b=$netB"
        "measured_center_distance_nm=$measuredDistanceNm"
        "expected_center_distance_nm=$expDistNm"
        "edge_to_edge_gap_mm=0.15"
        "expected_nominal_clearance_nm=$nominalClearanceNm"
        "expected_drc_violation_type=clearance"
        "expected_drc_violation_note=edge_gap_0.15mm_lt_nominal_0.30mm_for_future_run_drc"
        "exact_blocker_if_any=$blocker"
        "target_run_dir=$runDir"
        ""
        "failures:"
        $(if ($failures.Count -eq 0) { "(none)" } else { $failures -join "`n" })
    ) | Set-Content (Join-Path $runDir "r123_ipc_drc_v1_fixture_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    if (-not $green) {
        throw ("r123_verify_ipc_drc_fixture failed: " + ($failures -join "; "))
    }
    exit 0
}
catch {
    $_.Exception.Message | Set-Content (Join-Path $runDir "r123_error.txt") -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
