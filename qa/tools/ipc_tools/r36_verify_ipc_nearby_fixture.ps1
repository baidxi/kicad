param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

<#
.SYNOPSIS
  R36 — Vérifie la fixture ipc_nearby_v1 avec les tools MCP existants (sans get_pcb_nearby_items_context_session).

.DESCRIPTION
  Copie qa/data/pcbnew/nearby/ipc_nearby_v1 vers build/ipc-nearby-v1-proj, ouvre le PCB,
  valide qa_open_editor_frame, pcb_get_board_item_by_id_session, get_pcb_item_geometry_session,
  pcb_measure_distance_session (distances centre–centre attendues), get_nets.

.NOTES
  Valeurs nm attendues alignées sur r30_clearance : 2 mm → 200_000_000, 3 mm → 300_000_000,
  70 mm → 7_000_000_000 sur dx_nm (tolérance ±500).
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

function Sync-IpcNearbyV1Fixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )
    $src = Join-Path $RepoRoot "qa\data\pcbnew\nearby\ipc_nearby_v1"
    $dst = Join-Path $BuildDir "ipc-nearby-v1-proj"
    if (!(Test-Path -LiteralPath $src)) {
        throw "Missing ipc_nearby_v1 fixture source at $src"
    }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcNearbyV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-nearby-v1-proj\ipc_nearby_v1.kicad_pro"
$pcb = Join-Path $BuildDir "ipc-nearby-v1-proj\ipc_nearby_v1.kicad_pcb"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"

$kidTgt = "f0e10001-0001-4001-8001-000000000101"
$kidSam = "f0e10002-0002-4002-8002-000000000102"
$kidNin = "f0e10003-0003-4003-8003-000000000103"
$kidXcl = "f0e10004-0004-4004-8004-000000000104"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $pcb)) { throw "Missing board $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-r36-ipc-nearby-fixture")
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

function Test-MeasurePair {
    param(
        [string]$Url,
        [string]$Tag,
        [string]$IdA,
        [string]$IdB,
        [double]$ExpDx,
        [double]$ExpDist,
        [double]$ExpDyAbsMax
    )
    $measureArgs = @{
        board_filename = $pcbAbs
        a              = @{ item_id = $IdA }
        b              = @{ item_id = $IdB }
    } | ConvertTo-Json -Compress -Depth 8
    $measurePath = Join-Path $runDir ("pcb_measure_" + $Tag + ".args.json")
    Set-Content $measurePath $measureArgs -Encoding UTF8
    $mSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $measurePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $mSm.Text | Set-Content (Join-Path $runDir ("pcb_measure_" + $Tag + ".log")) -Encoding UTF8
    $meas = Parse-MeasureDistanceOutput -Text $mSm.Text
    $tol = 500.0
    if (-not $meas.status_ok -or $meas.ok -ne "true") { return "pcb_measure $Tag status/ok" }
    if ($meas.a_source -ne "item_bbox_center" -or $meas.b_source -ne "item_bbox_center") {
        return "pcb_measure $Tag item_bbox_center"
    }
    if ($null -eq $meas.dx_nm -or [math]::Abs($meas.dx_nm - $ExpDx) -gt $tol) {
        return ("pcb_measure $Tag dx_nm expected ~{0} got {1}" -f $ExpDx, $meas.dx_nm)
    }
    if ($null -eq $meas.distance_nm -or [math]::Abs($meas.distance_nm - $ExpDist) -gt $tol) {
        return ("pcb_measure $Tag distance_nm expected ~{0} got {1}" -f $ExpDist, $meas.distance_nm)
    }
    if ($null -eq $meas.dy_nm -or [math]::Abs($meas.dy_nm) -gt $ExpDyAbsMax) {
        return ("pcb_measure $Tag dy_nm expected ~0 got {0}" -f $meas.dy_nm)
    }
    return $null
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

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame failed: $($openSm.Text)" }
    Start-Sleep -Seconds 3

    $failures = @()

    foreach ($pair in @(
            @{ id = $kidTgt; net = "QA_NET_MAIN"; tag = "QTGT" }
            @{ id = $kidSam; net = "QA_NET_MAIN"; tag = "QSAM" }
            @{ id = $kidNin; net = "QA_NET_OTHER"; tag = "QNIN" }
            @{ id = $kidXcl; net = "QA_NET_OTHER"; tag = "QXCL" }
        )) {
        $argsPath = Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.tag + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_item_by_id_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_get_board_item_by_id_" + $pair.tag + ".log")) -Encoding UTF8
        if ($sm.Text -notmatch 'status=1') { $failures += "pcb_get_board_item_by_id $($pair.tag) status" }
        if ($sm.Text -notmatch 'string_value:\s*"pad"') { $failures += "pcb_get_board_item_by_id expected pad $($pair.tag)" }
        $netPat = 'key:\s+"net_name"[\s\S]{0,500}?string_value:\s*"' + [regex]::Escape($pair.net) + '"'
        if ($sm.Text -notmatch $netPat) { $failures += "pcb_get_board_item_by_id net $($pair.tag)" }
    }

    foreach ($pair in @(
            @{ id = $kidTgt; net = "QA_NET_MAIN"; tag = "QTGT" }
            @{ id = $kidNin; net = "QA_NET_OTHER"; tag = "QNIN" }
        )) {
        $argsPath = Join-Path $runDir ("get_pcb_item_geometry_" + $pair.tag + ".args.json")
        (@{ board_filename = $pcbAbs; item_id = $pair.id } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8
        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_item_geometry_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("get_pcb_item_geometry_" + $pair.tag + ".log")) -Encoding UTF8
        if ($sm.Text -notmatch 'status=1') { $failures += "get_pcb_item_geometry $($pair.tag) status" }
        $geomNetPat = 'key:\s+"net_name"[\s\S]{0,500}?string_value:\s*"' + [regex]::Escape($pair.net) + '"'
        if ($sm.Text -notmatch $geomNetPat) { $failures += "get_pcb_item_geometry net $($pair.tag)" }
    }

    $exp2mm = 200000000.0
    $exp3mm = 300000000.0
    $exp70mm = 7000000000.0

    foreach ($t in @(
            @{ tag = "tgt_to_sam"; a = $kidTgt; b = $kidSam; edx = $exp2mm; ed = $exp2mm }
            @{ tag = "tgt_to_nin"; a = $kidTgt; b = $kidNin; edx = $exp3mm; ed = $exp3mm }
            @{ tag = "tgt_to_xcl"; a = $kidTgt; b = $kidXcl; edx = $exp70mm; ed = $exp70mm }
        )) {
        $err = Test-MeasurePair -Url $url -Tag $t.tag -IdA $t.a -IdB $t.b -ExpDx $t.edx -ExpDist $t.ed -ExpDyAbsMax 500.0
        if ($null -ne $err) { $failures += $err }
    }

    $netsArgs = Join-Path $runDir "get_nets.args.json"
    (@{ board_filename = $pcbAbs } | ConvertTo-Json -Compress) | Set-Content $netsArgs -Encoding UTF8
    $netsSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_nets", ("@" + $netsArgs)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $netsSm.Text | Set-Content (Join-Path $runDir "get_nets.log") -Encoding UTF8
    if ($netsSm.Text -notmatch 'status=1') { $failures += "get_nets status" }
    foreach ($nn in @("QA_NET_MAIN", "QA_NET_OTHER")) {
        if ($netsSm.Text -notmatch [regex]::Escape($nn)) { $failures += "get_nets missing $nn" }
    }

    $green = ($failures.Count -eq 0)
    @(
        "r36_ipc_nearby_fixture_ok=$( if ($green) { 'true' } else { 'false' } )"
        "kid_target=$kidTgt"
        "kid_same_net=$kidSam"
        "kid_included=$kidNin"
        "kid_excluded=$kidXcl"
        "target_run_dir=$runDir"
        ""
        "failures:"
        $(if ($failures.Count -eq 0) { "(none)" } else { $failures -join "`n" })
    ) | Set-Content (Join-Path $runDir "r36_ipc_nearby_fixture_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    if (-not $green) {
        throw ("r36_verify_ipc_nearby_fixture failed: " + ($failures -join "; "))
    }
    exit 0
}
catch {
    $_.Exception.Message | Set-Content (Join-Path $runDir "r36_error.txt") -Encoding UTF8
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
