param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

<#
.SYNOPSIS
  R30 — Vérifie la fixture ipc_clearance_v1 avec les tools MCP existants (sans get_pcb_clearance_context_session).

.DESCRIPTION
  Copie qa/data/pcbnew/clearance/ipc_clearance_v1 vers build/ipc-clearance-v1-proj, ouvre le PCB,
  puis valide par smoke : qa_open_editor_frame, pcb_get_board_item_by_id_session,
  get_pcb_item_geometry_session, pcb_measure_distance_session, get_net_classes, get_nets,
  get_board_stackup (best-effort).
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

function Sync-IpcClearanceV1Fixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )
    $src = Join-Path $RepoRoot "qa\data\pcbnew\clearance\ipc_clearance_v1"
    $dst = Join-Path $BuildDir "ipc-clearance-v1-proj"
    if (!(Test-Path -LiteralPath $src)) {
        throw "Missing ipc_clearance_v1 fixture source at $src"
    }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcClearanceV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-clearance-v1-proj\ipc_clearance_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-clearance-v1-proj\ipc_clearance_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

$kidA = "f0d10001-0001-4001-8001-000000000001"
$kidB = "f0d20002-0002-4002-8002-000000000002"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $pcb)) { throw "Missing board $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r30-ipc-clearance-fixture")
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
        blocker     = $(if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $Matches[1] } else { "N/A" })
        distance_nm = & $parseNum "distance_nm"
        dx_nm       = & $parseNum "dx_nm"
        dy_nm       = & $parseNum "dy_nm"
        a_source    = $(if ($Text -match 'key:\s+"a_source"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $Matches[1] } else { "" })
        b_source    = $(if ($Text -match 'key:\s+"b_source"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $Matches[1] } else { "" })
    }
}

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

    # get_net_classes (projet) — avant l'ouverture de l'éditeur PCB ; long timeout (rare blocage UI)
    $ncArgs = Join-Path $runDir "get_net_classes.args.json"
    Set-Content -Path $ncArgs -Value '{}' -Encoding UTF8
    $ncSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_net_classes", ("@" + $ncArgs)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $ncSm.Text | Set-Content (Join-Path $runDir "get_net_classes_after_load.log") -Encoding UTF8

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame failed: $($openSm.Text)" }
    Start-Sleep -Seconds 3

    if ($ncSm.Text -notmatch 'status=1' -or $ncSm.Text -eq "timeout") {
        $ncSm2 = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_net_classes", ("@" + $ncArgs)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $ncSm2.Text | Set-Content (Join-Path $runDir "get_net_classes_after_pcb_open.log") -Encoding UTF8
        $ncSm = $ncSm2
    }

    $failures = @()

    $ncSm.Text | Set-Content (Join-Path $runDir "get_net_classes.log") -Encoding UTF8
    if ($ncSm.Text -notmatch 'status=1') { $failures += "get_net_classes status" }
    foreach ($name in @("QA_NETCLASS_A", "QA_NETCLASS_B")) {
        if ($ncSm.Text -notmatch [regex]::Escape($name)) { $failures += "get_net_classes missing $name" }
    }

    # pcb_get_board_item_by_id_session (A / B)
    foreach ($pair in @(
            @{ id = $kidA; net = "QA_NET_A" }
            @{ id = $kidB; net = "QA_NET_B" }
        )) {
        $argsPath = Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.net + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_item_by_id_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.net + ".log")) -Encoding UTF8
        if ($sm.Text -notmatch 'status=1') { $failures += "pcb_get_board_item_by_id_session $($pair.id) status" }
        if ($sm.Text -notmatch 'string_value:\s*"pad"') { $failures += "pcb_get_board_item_by_id_session expected item_type pad for $($pair.id)" }
        $netPat = 'key:\s+"net_name"[\s\S]{0,500}?string_value:\s*"' + [regex]::Escape($pair.net) + '"'
        if ($sm.Text -notmatch $netPat) { $failures += "pcb_get_board_item_by_id_session net_name $($pair.net)" }
    }

    # get_pcb_item_geometry_session
    foreach ($pair in @(
            @{ id = $kidA; net = "QA_NET_A" }
            @{ id = $kidB; net = "QA_NET_B" }
        )) {
        $argsPath = Join-Path $runDir ("get_pcb_item_geometry_" + $pair.net + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_item_geometry_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("get_pcb_item_geometry_" + $pair.net + ".log")) -Encoding UTF8
        if ($sm.Text -notmatch 'status=1') { $failures += "get_pcb_item_geometry_session $($pair.id) status" }
        $geomNetPat = 'key:\s+"net_name"[\s\S]{0,500}?string_value:\s*"' + [regex]::Escape($pair.net) + '"'
        if ($sm.Text -notmatch $geomNetPat) { $failures += "get_pcb_item_geometry_session net_name $($pair.net)" }
    }

    # pcb_measure_distance_session — bbox centres pad A vs pad B (15 mm)
    $measureArgs = @{
        board_filename = $pcbAbs
        a              = @{ item_id = $kidA }
        b              = @{ item_id = $kidB }
    } | ConvertTo-Json -Compress -Depth 8
    $measurePath = Join-Path $runDir "pcb_measure_clearance_pads.args.json"
    Set-Content $measurePath $measureArgs -Encoding UTF8
    $mSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $measurePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $mSm.Text | Set-Content (Join-Path $runDir "pcb_measure_clearance_pads.log") -Encoding UTF8
    $meas = Parse-MeasureDistanceOutput -Text $mSm.Text
    if (-not $meas.status_ok -or $meas.ok -ne "true") { $failures += "pcb_measure_distance_session status/ok" }
    if ($meas.a_source -ne "item_bbox_center" -or $meas.b_source -ne "item_bbox_center") {
        $failures += "pcb_measure_distance_session expected item_bbox_center sources"
    }
    # nm reporting uses KiCad IU→nm scale (see diagnostics iu_nm_ratio on successful measure); 15 mm gap → 1_500_000_000 nm here.
    $expDist = 1500000000.0
    if ($null -eq $meas.distance_nm -or [math]::Abs($meas.distance_nm - $expDist) -gt 500.0) {
        $failures += ("pcb_measure_distance_session distance_nm expected ~{0} got {1}" -f $expDist, $meas.distance_nm)
    }
    if ($null -eq $meas.dx_nm -or [math]::Abs($meas.dx_nm - $expDist) -gt 500.0) {
        $failures += ("pcb_measure_distance_session dx_nm expected ~{0} got {1}" -f $expDist, $meas.dx_nm)
    }
    if ($null -eq $meas.dy_nm -or [math]::Abs($meas.dy_nm) -gt 500.0) {
        $failures += ("pcb_measure_distance_session dy_nm expected ~0 got {0}" -f $meas.dy_nm)
    }

    # get_nets (board)
    $netsArgs = Join-Path $runDir "get_nets.args.json"
    (@{ board_filename = $pcbAbs } | ConvertTo-Json -Compress) | Set-Content $netsArgs -Encoding UTF8
    $netsSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_nets", ("@" + $netsArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $netsSm.Text | Set-Content (Join-Path $runDir "get_nets.log") -Encoding UTF8
    if ($netsSm.Text -notmatch 'status=1') { $failures += "get_nets status" }
    foreach ($nn in @("QA_NET_A", "QA_NET_B")) {
        if ($netsSm.Text -notmatch [regex]::Escape($nn)) { $failures += "get_nets missing $nn" }
    }

    # get_board_stackup (best-effort — ne fait pas échouer le script si stub minimal)
    $suArgs = Join-Path $runDir "get_board_stackup.args.json"
    (@{ board_filename = $pcbAbs } | ConvertTo-Json -Compress) | Set-Content $suArgs -Encoding UTF8
    $suSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_board_stackup", ("@" + $suArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $suSm.Text | Set-Content (Join-Path $runDir "get_board_stackup.log") -Encoding UTF8
    $stackupOk = ($suSm.Text -match 'status=1')

    $green = ($failures.Count -eq 0)
    @(
        "r30_ipc_clearance_fixture_ok=$( if ($green) { 'true' } else { 'false' } )"
        "pad_a_kiid=$kidA"
        "pad_b_kiid=$kidB"
        "target_run_dir=$runDir"
        "get_board_stackup_status_ok=$( if ($stackupOk) { 'true' } else { 'false' } )"
        ""
        "failures:"
        $(if ($failures.Count -eq 0) { "(none)" } else { $failures -join "`n" })
    ) | Set-Content (Join-Path $runDir "r30_ipc_clearance_fixture_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    if (-not $green) {
        throw ("r30_verify_ipc_clearance_fixture failed: " + ($failures -join "; "))
    }
    exit 0
}
catch {
    $_.Exception.Message | Set-Content (Join-Path $runDir "r30_error.txt") -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
