param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
if (!(Test-Path -LiteralPath $commonModule)) { throw "Missing common IPC QA module at $commonModule" }
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj     = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro"
$pcb      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pcb"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-measure-distance-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

function Sync-IpcGeometryV1Fixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$BuildDir
    )
    $src = Join-Path $RepoRoot "qa\data\pcbnew\geometry\ipc_geometry_v1"
    $dst = Join-Path $BuildDir "ipc-geometry-v1-proj"
    if (!(Test-Path -LiteralPath $src)) {
        throw "Missing ipc_geometry_v1 fixture source at $src"
    }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $src "*") -Destination $dst -Recurse -Force
}

Sync-IpcGeometryV1Fixture -RepoRoot $RepoRoot -BuildDir $BuildDir

$script:ProbeIpcRetryExtraTotal = 0

@(
    "captured_at=$( Get-Date -Format 'o' )"
    "PWD=$(( Get-Location ).Path)"
    "PATH=$env:PATH"
    "KICAD_RUN_FROM_BUILD_DIR=$env:KICAD_RUN_FROM_BUILD_DIR"
    "KICAD_CONFIG_HOME=$env:KICAD_CONFIG_HOME"
    "KICAD_IPC_AUTOMATION=$env:KICAD_IPC_AUTOMATION"
    ""
) | Set-Content -Path (Join-Path $runDir "harness_runtime_env.txt") -Encoding UTF8

function Get-ProtoStringField {
    param([string]$Text, [string]$Key)
    if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?string_value:\s*"([^"]*)"')) {
        return $Matches[1]
    }
    return ""
}

function Parse-MeasureDistanceOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $bf = ""
    if ($Text -match 'key:\s+"board_filename"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $bf = $Matches[1] }
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?number_value:\s*([-0-9]+(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    return @{
        status_ok       = ($Text -match 'status=1')
        ok              = $ok
        blocker         = $blocker
        board_filename  = $bf
        distance_nm     = & $parseNum "distance_nm"
        dx_nm           = & $parseNum "dx_nm"
        dy_nm           = & $parseNum "dy_nm"
        a_x_nm          = & $parseNum "a_x_nm"
        a_y_nm          = & $parseNum "a_y_nm"
        b_x_nm          = & $parseNum "b_x_nm"
        b_y_nm          = & $parseNum "b_y_nm"
        a_source        = (Get-ProtoStringField -Text $Text -Key "a_source")
        b_source        = (Get-ProtoStringField -Text $Text -Key "b_source")
        a_item_id       = (Get-ProtoStringField -Text $Text -Key "a_item_id")
        b_item_id       = (Get-ProtoStringField -Text $Text -Key "b_item_id")
        raw             = $Text
    }
}

function Write-ContractSummary {
    param(
        [string]$Green,
        [string]$NominalOk,
        [string]$ZeroOk,
        [string]$SignedOk,
        [string]$NegMissingAOk,
        [string]$ItemItemOk,
        [string]$PadAnchorOk,
        [string]$NegPadAnchorNonPadOk,
        [string]$NegInvalidItemPositionOk,
        [string]$MeasurePadShapeOriginOk,
        [string]$MeasurePadAnchorToShapeOriginOk,
        [string]$NegPadShapeOriginMissingLayerOk,
        [string]$NegPadShapeOriginNonPadOk,
        [string]$NegInvalidLayerOk,
        [string]$MeasurePadShapeOriginAliasFCuOk,
        [string]$Blocker,
        [string]$Note,
        [string]$IpcRetryExtraTotal = "0"
    )
    @(
        "pcb_measure_distance_green=$Green"
        "measure_nominal_ok=$NominalOk"
        "measure_zero_distance_ok=$ZeroOk"
        "measure_signed_swap_ok=$SignedOk"
        "negative_missing_point_a_ok=$NegMissingAOk"
        "measure_item_to_item_ok=$ItemItemOk"
        "measure_pad_anchor_ok=$PadAnchorOk"
        "negative_pad_anchor_non_pad_ok=$NegPadAnchorNonPadOk"
        "negative_invalid_item_position_ok=$NegInvalidItemPositionOk"
        "measure_pad_shape_origin_ok=$MeasurePadShapeOriginOk"
        "measure_pad_anchor_to_shape_origin_ok=$MeasurePadAnchorToShapeOriginOk"
        "negative_pad_shape_origin_missing_layer_ok=$NegPadShapeOriginMissingLayerOk"
        "negative_pad_shape_origin_non_pad_ok=$NegPadShapeOriginNonPadOk"
        "negative_invalid_layer_ok=$NegInvalidLayerOk"
        "measure_pad_shape_origin_alias_f_cu_ok=$MeasurePadShapeOriginAliasFCuOk"
        "ipc_smoke_ipc_retry_extra_total=$IpcRetryExtraTotal"
        "exact_blocker_if_any=$Blocker"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        "case_note=$Note"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_measure_distance_session_contract_summary.txt") -Encoding UTF8
}

function Test-PcbMeasureNmClose {
    param( $Left, $Right, [double]$Eps = 0.5 )
    if ( $null -eq $Left -or $null -eq $Right ) { return $false }
    return [math]::Abs( [double]$Left - [double]$Right ) -lt $Eps
}

function Invoke-MeasureSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-MeasureDistanceOutput -Text $out
}

function Get-FirstItemIdFromBoardItemsSession {
    param(
        [string]$ApiUrl,
        [string]$LogLabel,
        [string[]]$ItemTypes,
        [string]$BoardFilenameOverride = ""
    )
    $argsPath = Join-Path $runDir ("pcb_get_board_items_session." + $LogLabel + ".args.json")
    $bfUse = if ($BoardFilenameOverride) { $BoardFilenameOverride } else { $pcbAbs }
    $payload = @{ board_filename = $bfUse; max_items = 1 }
    if ($null -ne $ItemTypes -and $ItemTypes.Count -gt 0) {
        $payload.item_types = $ItemTypes
    }
    ($payload | ConvertTo-Json -Compress -Depth 6) | Set-Content -Path $argsPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $txt = $sm.Text
    $txt | Set-Content -Path (Join-Path $runDir ("pcb_get_board_items_session." + $LogLabel + ".log")) -Encoding UTF8
    foreach ($line in ($txt -split "`r?`n")) {
        if ($line -match '^first_item_id=(.+)$') {
            return $Matches[1].Trim()
        }
    }
    return $null
}

function Get-TwoDistinctItemIdsViaBoardItemsSession {
    param([string]$ApiUrl)
    # qa_ipc_smoke prints `first_item_id=` from the raw tool result even when items_summary is omitted.
    # Pad + board drawing keeps the probe fast and deterministic on ipc-v1 (R15: FP+child pad is valid but
    # footprint bbox can be heavier on very dense designs; legacy smoke timeouts were largely stdout volume).
    $idPad = Get-FirstItemIdFromBoardItemsSession -ApiUrl $ApiUrl -LogLabel "probe_pad" -ItemTypes @("pads")
    $idDrw = Get-FirstItemIdFromBoardItemsSession -ApiUrl $ApiUrl -LogLabel "probe_drw" -ItemTypes @("drawings")
    if ([string]::IsNullOrWhiteSpace($idPad) -or [string]::IsNullOrWhiteSpace($idDrw)) {
        return $null
    }
    if ($idPad -eq $idDrw) {
        return $null
    }
    return @{ IdA = $idPad; IdB = $idDrw }
}

$nominalOk = "false"
$zeroOk = "false"
$signedOk = "false"
$itemItemOk = "false"
$padAnchorOk = "false"
$negPadAnchorNonPadOk = "false"
$negInvalidItemPositionOk = "false"
$measurePadShapeOriginOk = "false"
$measurePadAnchorToShapeOriginOk = "false"
$negPadShapeOriginMissingLayerOk = "false"
$negPadShapeOriginNonPadOk = "false"
$negInvalidLayerOk = "false"
$measurePadShapeOriginAliasFCuOk = "false"
$negMissingAOk = "false"
$overallGreen = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if ($resolved.Resolved) { $resolved.Url } else { $null }
    if (-not $url) {
        Write-ContractSummary -Green "false" -NominalOk "false" -ZeroOk "false" -SignedOk "false" -NegMissingAOk "false" -ItemItemOk "false" `
            -PadAnchorOk "false" -NegPadAnchorNonPadOk "false" -NegInvalidItemPositionOk "false" `
            -MeasurePadShapeOriginOk "false" -MeasurePadAnchorToShapeOriginOk "false" `
            -NegPadShapeOriginMissingLayerOk "false" -NegPadShapeOriginNonPadOk "false" -NegInvalidLayerOk "false" `
            -MeasurePadShapeOriginAliasFCuOk "false" `
            -Blocker "ipc_url_unresolved" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    $ping = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $ping['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    if ($ping.Text -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -NominalOk "false" -ZeroOk "false" -SignedOk "false" -NegMissingAOk "false" -ItemItemOk "false" `
            -PadAnchorOk "false" -NegPadAnchorNonPadOk "false" -NegInvalidItemPositionOk "false" `
            -MeasurePadShapeOriginOk "false" -MeasurePadAnchorToShapeOriginOk "false" `
            -NegPadShapeOriginMissingLayerOk "false" -NegPadShapeOriginNonPadOk "false" -NegInvalidLayerOk "false" `
            -MeasurePadShapeOriginAliasFCuOk "false" `
            -Blocker "ping_failed" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        Write-ContractSummary -Green "false" -NominalOk "false" -ZeroOk "false" -SignedOk "false" -NegMissingAOk "false" -ItemItemOk "false" `
            -PadAnchorOk "false" -NegPadAnchorNonPadOk "false" -NegInvalidItemPositionOk "false" `
            -MeasurePadShapeOriginOk "false" -MeasurePadAnchorToShapeOriginOk "false" `
            -NegPadShapeOriginMissingLayerOk "false" -NegPadShapeOriginNonPadOk "false" -NegInvalidLayerOk "false" `
            -MeasurePadShapeOriginAliasFCuOk "false" `
            -Blocker "project_not_loaded" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $openSm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $openOut = $openSm.Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if ($openOut -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -NominalOk "false" -ZeroOk "false" -SignedOk "false" -NegMissingAOk "false" -ItemItemOk "false" `
            -PadAnchorOk "false" -NegPadAnchorNonPadOk "false" -NegInvalidItemPositionOk "false" `
            -MeasurePadShapeOriginOk "false" -MeasurePadAnchorToShapeOriginOk "false" `
            -NegPadShapeOriginMissingLayerOk "false" -NegPadShapeOriginNonPadOk "false" -NegInvalidLayerOk "false" `
            -MeasurePadShapeOriginAliasFCuOk "false" `
            -Blocker "qa_open_editor_frame_failed" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    Start-Sleep -Seconds 3

    # 1) Nominal 3-4-5 triangle in nm (IU grid compatible).
    $p1 = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        a              = @{ x_nm = 0; y_nm = 0 }
        b              = @{ x_nm = 3000000; y_nm = 4000000 }
    } -LogBaseName "pcb_measure_nominal_345"
    if ($p1.status_ok -and $p1.ok -eq "true" -and $p1.blocker -eq "N/A" `
            -and $p1.a_source -eq "point" -and $p1.b_source -eq "point" `
            -and $null -ne $p1.dx_nm -and [math]::Abs($p1.dx_nm - 3000000) -lt 0.5 `
            -and $null -ne $p1.dy_nm -and [math]::Abs($p1.dy_nm - 4000000) -lt 0.5 `
            -and $null -ne $p1.distance_nm -and [math]::Abs($p1.distance_nm - 5000000) -lt 0.5) {
        $nominalOk = "true"
    }

    # 2) Zero distance (A = B).
    $p2 = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        a              = @{ x_nm = 1000000; y_nm = 2000000 }
        b              = @{ x_nm = 1000000; y_nm = 2000000 }
    } -LogBaseName "pcb_measure_zero_distance"
    if ($p2.status_ok -and $p2.ok -eq "true" -and $p2.blocker -eq "N/A" `
            -and $null -ne $p2.dx_nm -and [math]::Abs($p2.dx_nm) -lt 0.5 `
            -and $null -ne $p2.dy_nm -and [math]::Abs($p2.dy_nm) -lt 0.5 `
            -and $null -ne $p2.distance_nm -and [math]::Abs($p2.distance_nm) -lt 0.5) {
        $zeroOk = "true"
    }

    # 3) Signed dx/dy (swap endpoints vs nominal).
    $p3 = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        a              = @{ x_nm = 3000000; y_nm = 4000000 }
        b              = @{ x_nm = 0; y_nm = 0 }
    } -LogBaseName "pcb_measure_signed_swap"
    if ($p3.status_ok -and $p3.ok -eq "true" -and $p3.blocker -eq "N/A" `
            -and $null -ne $p3.dx_nm -and [math]::Abs($p3.dx_nm + 3000000) -lt 0.5 `
            -and $null -ne $p3.dy_nm -and [math]::Abs($p3.dy_nm + 4000000) -lt 0.5 `
            -and $null -ne $p3.distance_nm -and [math]::Abs($p3.distance_nm - 5000000) -lt 0.5) {
        $signedOk = "true"
    }

    # 4) Item-to-item (bbox centres) via KIIDs from pcb_get_board_items_session + smoke `first_item_id`.
    $idPair = Get-TwoDistinctItemIdsViaBoardItemsSession -ApiUrl $url
    if ($null -ne $idPair) {
        $idA = $idPair.IdA
        $idB = $idPair.IdB
        $p5 = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            a              = @{ item_id = $idA }
            b              = @{ item_id = $idB }
        } -LogBaseName "pcb_measure_item_to_item"
        $dxExp = $null
        $dyExp = $null
        if ($null -ne $p5.b_x_nm -and $null -ne $p5.a_x_nm) { $dxExp = $p5.b_x_nm - $p5.a_x_nm }
        if ($null -ne $p5.b_y_nm -and $null -ne $p5.a_y_nm) { $dyExp = $p5.b_y_nm - $p5.a_y_nm }
        $distExp = $null
        if ($null -ne $dxExp -and $null -ne $dyExp) {
            $distExp = [math]::Sqrt($dxExp * $dxExp + $dyExp * $dyExp)
        }
        $distOk = $false
        if ($null -ne $p5.distance_nm -and $null -ne $distExp) {
            $distOk = ([math]::Abs($p5.distance_nm - $distExp) -lt 1.0)
        }
        if ($p5.status_ok -and $p5.ok -eq "true" -and $p5.blocker -eq "N/A" `
                -and $p5.a_source -eq "item_bbox_center" -and $p5.b_source -eq "item_bbox_center" `
                -and $p5.a_item_id -eq $idA -and $p5.b_item_id -eq $idB `
                -and $null -ne $dxExp -and $null -ne $p5.dx_nm -and ([math]::Abs($p5.dx_nm - $dxExp) -lt 1.0) `
                -and $null -ne $dyExp -and $null -ne $p5.dy_nm -and ([math]::Abs($p5.dy_nm - $dyExp) -lt 1.0) `
                -and $distOk) {
            $itemItemOk = "true"
        }
    }

    # Pad vs drawing ids for R18 (reuse pair when available; else probe lists).
    $idPadForAnchor = $null
    $idDrwForNeg = $null
    if ($null -ne $idPair) {
        $idPadForAnchor = $idPair.IdA
        $idDrwForNeg = $idPair.IdB
    }
    else {
        $idPadForAnchor = Get-FirstItemIdFromBoardItemsSession -ApiUrl $url -LogLabel "probe_pad_r18" -ItemTypes @("pads")
        $idDrwForNeg = Get-FirstItemIdFromBoardItemsSession -ApiUrl $url -LogLabel "probe_drw_r18" -ItemTypes @("drawings")
    }

    # 5) Pad anchor: same pad twice → distance ~0; round-trip point ↔ pad_anchor.
    if (-not [string]::IsNullOrWhiteSpace($idPadForAnchor)) {
        $pPaSelf = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            a              = @{ item_id = $idPadForAnchor; position = "pad_anchor" }
            b              = @{ item_id = $idPadForAnchor; position = "pad_anchor" }
        } -LogBaseName "pcb_measure_pad_anchor_self"
        $selfRoundOk = $false
        if ($pPaSelf.status_ok -and $pPaSelf.ok -eq "true" -and $pPaSelf.blocker -eq "N/A" `
                -and $pPaSelf.a_source -eq "item_pad_anchor" -and $pPaSelf.b_source -eq "item_pad_anchor" `
                -and $null -ne $pPaSelf.distance_nm -and ([math]::Abs($pPaSelf.distance_nm) -lt 1.0)) {
            $selfRoundOk = $true
        }

        $pPaVsOrigin = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            a              = @{ item_id = $idPadForAnchor; position = "pad_anchor" }
            b              = @{ x_nm = 0; y_nm = 0 }
        } -LogBaseName "pcb_measure_pad_anchor_vs_origin"
        $tripOk = $false
        if ($selfRoundOk -and $pPaVsOrigin.status_ok -and $pPaVsOrigin.ok -eq "true" `
                -and $null -ne $pPaVsOrigin.a_x_nm -and $null -ne $pPaVsOrigin.a_y_nm) {
            $ax = $pPaVsOrigin.a_x_nm
            $ay = $pPaVsOrigin.a_y_nm
            $pRound = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbAbs
                a              = @{ x_nm = $ax; y_nm = $ay }
                b              = @{ item_id = $idPadForAnchor; position = "pad_anchor" }
            } -LogBaseName "pcb_measure_pad_anchor_roundtrip"
            if ($pRound.status_ok -and $pRound.ok -eq "true" -and $pRound.blocker -eq "N/A" `
                    -and $pRound.a_source -eq "point" -and $pRound.b_source -eq "item_pad_anchor" `
                    -and $null -ne $pRound.distance_nm -and ([math]::Abs($pRound.distance_nm) -lt 1.0)) {
                $tripOk = $true
            }
        }

        if ($selfRoundOk -and $tripOk) {
            $padAnchorOk = "true"
        }
    }

    # 6) Negative: pad_anchor on non-pad item (drawing).
    if (-not [string]::IsNullOrWhiteSpace($idDrwForNeg)) {
        $pNegNp = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            a              = @{ item_id = $idDrwForNeg; position = "pad_anchor" }
            b              = @{ x_nm = 0; y_nm = 0 }
        } -LogBaseName "pcb_measure_neg_pad_anchor_non_pad"
        if ($pNegNp.status_ok -and $pNegNp.ok -eq "false" -and $pNegNp.blocker -eq "pad_anchor_requires_pad_a") {
            $negPadAnchorNonPadOk = "true"
        }
    }

    # 7) Negative: unknown position string on item endpoint.
    if (-not [string]::IsNullOrWhiteSpace($idPadForAnchor)) {
        $pNegIv = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            a              = @{ item_id = $idPadForAnchor; position = "not_a_valid_position_enum" }
            b              = @{ x_nm = 0; y_nm = 0 }
        } -LogBaseName "pcb_measure_neg_invalid_item_position"
        if ($pNegIv.status_ok -and $pNegIv.ok -eq "false" -and $pNegIv.blocker -eq "invalid_item_position_a") {
            $negInvalidItemPositionOk = "true"
        }
    }

    # --- R22: ipc_geometry_v1 board — pad_shape_origin (fixture synced from qa/data at probe start).
    $pcbGeomPath = Join-Path $BuildDir "ipc-geometry-v1-proj\ipc_geometry_v1.kicad_pcb"
    $geomKid = "f0010001-0001-4001-8001-000000000001"
    if (Test-Path -LiteralPath $pcbGeomPath) {
        $pcbGeomAbs = (Resolve-Path $pcbGeomPath).Path
        $openGeomArgs = @{ type = "pcb"; filename = $pcbGeomAbs } | ConvertTo-Json -Compress
        $openGeomInput = Join-Path $runDir "qa_open_editor_frame_ipc_geometry.json"
        Set-Content -Path $openGeomInput -Value $openGeomArgs -Encoding UTF8
        $openGeomSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openGeomInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $rax = $openGeomSm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
        $openGeomSm.Text | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_ipc_geometry.log") -Encoding UTF8

        if ($openGeomSm.Text -match 'status=1') {
            Start-Sleep -Seconds 2

            # A) pad_anchor self (offset pad).
            $gSa = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_anchor" }
                b              = @{ item_id = $geomKid; position = "pad_anchor" }
            } -LogBaseName "ipc_geom_pad_anchor_self"

            # B) pad_shape_origin BL_F_Cu self (BoardLayer protobuf name; KiCad token F_Cu also accepted by handler).
            $gSb = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "BL_F_Cu" }
                b              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "BL_F_Cu" }
            } -LogBaseName "ipc_geom_pad_shape_origin_self"

            # C) pad_anchor -> pad_shape_origin (non-zero separation).
            $gSc = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_anchor" }
                b              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "BL_F_Cu" }
            } -LogBaseName "ipc_geom_anchor_to_shape_origin"

            $distFromComps = $null
            if ($null -ne $gSc.dx_nm -and $null -ne $gSc.dy_nm) {
                $distFromComps = [math]::Sqrt($gSc.dx_nm * $gSc.dx_nm + $gSc.dy_nm * $gSc.dy_nm)
            }
            $hypotOk = $false
            if ($null -ne $gSc.distance_nm -and $null -ne $distFromComps) {
                $hypotOk = ([math]::Abs($gSc.distance_nm - $distFromComps) -lt 1.0)
            }

            $geomPadAnchorSelfOk = (
                $gSa.status_ok -and $gSa.ok -eq "true" -and $gSa.blocker -eq "N/A" `
                    -and $null -ne $gSa.distance_nm -and ([math]::Abs($gSa.distance_nm) -lt 1.0))

            if ($gSb.status_ok -and $gSb.ok -eq "true" -and $gSb.blocker -eq "N/A" `
                    -and $gSb.a_source -eq "item_pad_shape_origin" -and $gSb.b_source -eq "item_pad_shape_origin" `
                    -and $null -ne $gSb.distance_nm -and ([math]::Abs($gSb.distance_nm) -lt 1.0)) {
                $measurePadShapeOriginOk = "true"
            }

            # R49: layer alias "F_Cu" must match BL_F_Cu reference (ParseBoardLayerFromValue).
            $gSbFcu = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "F_Cu" }
                b              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "F_Cu" }
            } -LogBaseName "ipc_geom_pad_shape_origin_self_f_cu"
            if ($measurePadShapeOriginOk -eq "true" `
                    -and $gSbFcu.status_ok -and $gSbFcu.ok -eq "true" -and $gSbFcu.blocker -eq "N/A" `
                    -and $gSbFcu.a_source -eq "item_pad_shape_origin" -and $gSbFcu.b_source -eq "item_pad_shape_origin" `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.distance_nm -Right $gSb.distance_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.dx_nm -Right $gSb.dx_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.dy_nm -Right $gSb.dy_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.a_x_nm -Right $gSb.a_x_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.a_y_nm -Right $gSb.a_y_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.b_x_nm -Right $gSb.b_x_nm) `
                    -and (Test-PcbMeasureNmClose -Left $gSbFcu.b_y_nm -Right $gSb.b_y_nm)) {
                $measurePadShapeOriginAliasFCuOk = "true"
            }

            if ($geomPadAnchorSelfOk -and $gSc.status_ok -and $gSc.ok -eq "true" -and $gSc.blocker -eq "N/A" `
                    -and $gSc.a_source -eq "item_pad_anchor" -and $gSc.b_source -eq "item_pad_shape_origin" `
                    -and $null -ne $gSc.distance_nm -and ($gSc.distance_nm -gt 0.5) `
                    -and $hypotOk) {
                $measurePadAnchorToShapeOriginOk = "true"
            }

            # D) pad_shape_origin sans layer.
            $gNd = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_shape_origin" }
                b              = @{ x_nm = 0; y_nm = 0 }
            } -LogBaseName "ipc_geom_neg_pad_shape_missing_layer"
            if ($gNd.status_ok -and $gNd.ok -eq "false" -and $gNd.blocker -eq "pad_shape_origin_requires_layer_a") {
                $negPadShapeOriginMissingLayerOk = "true"
            }

            # E) pad_shape_origin sur drawing (Edge).
            $idDrwGeom = Get-FirstItemIdFromBoardItemsSession -ApiUrl $url -LogLabel "geom_drw" -ItemTypes @("drawings") -BoardFilenameOverride $pcbGeomAbs
            if (-not [string]::IsNullOrWhiteSpace($idDrwGeom)) {
                $gNe = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                    board_filename = $pcbGeomAbs
                    a              = @{ item_id = $idDrwGeom; position = "pad_shape_origin"; layer = "BL_F_Cu" }
                    b              = @{ x_nm = 0; y_nm = 0 }
                } -LogBaseName "ipc_geom_neg_pad_shape_non_pad"
                if ($gNe.status_ok -and $gNe.ok -eq "false" -and $gNe.blocker -eq "pad_shape_origin_requires_pad_a") {
                    $negPadShapeOriginNonPadOk = "true"
                }
            }

            # F) layer invalide.
            $gNf = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                a              = @{ item_id = $geomKid; position = "pad_shape_origin"; layer = "NOT_A_BOARD_LAYER_XYZ" }
                b              = @{ x_nm = 0; y_nm = 0 }
            } -LogBaseName "ipc_geom_neg_invalid_layer"
            if ($gNf.status_ok -and $gNf.ok -eq "false" -and $gNf.blocker -eq "invalid_layer_a") {
                $negInvalidLayerOk = "true"
            }
        }
    }

    # 8) Negative contract: missing endpoint `a` (V1 blocker kept).
    $p4 = Invoke-MeasureSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        b              = @{ x_nm = 0; y_nm = 0 }
    } -LogBaseName "pcb_measure_missing_point_a"
    if ($p4.status_ok -and $p4.ok -eq "false" -and $p4.blocker -eq "missing_point_a") {
        $negMissingAOk = "true"
    }

    if ($nominalOk -eq "true" -and $zeroOk -eq "true" -and $signedOk -eq "true" -and $negMissingAOk -eq "true" `
            -and $itemItemOk -eq "true" -and $padAnchorOk -eq "true" -and $negPadAnchorNonPadOk -eq "true" `
            -and $negInvalidItemPositionOk -eq "true" `
            -and $measurePadShapeOriginOk -eq "true" -and $measurePadAnchorToShapeOriginOk -eq "true" `
            -and $negPadShapeOriginMissingLayerOk -eq "true" -and $negPadShapeOriginNonPadOk -eq "true" `
            -and $negInvalidLayerOk -eq "true" -and $measurePadShapeOriginAliasFCuOk -eq "true") {
        $overallGreen = "true"
        $finalBlocker = "N/A"
    }
    else {
        if ($nominalOk -ne "true") { $finalBlocker = "nominal_contract_failed" }
        elseif ($zeroOk -ne "true") { $finalBlocker = "zero_distance_contract_failed" }
        elseif ($signedOk -ne "true") { $finalBlocker = "signed_swap_contract_failed" }
        elseif ($itemItemOk -ne "true") { $finalBlocker = "item_to_item_contract_failed" }
        elseif ($padAnchorOk -ne "true") { $finalBlocker = "pad_anchor_contract_failed" }
        elseif ($negPadAnchorNonPadOk -ne "true") { $finalBlocker = "negative_pad_anchor_non_pad_failed" }
        elseif ($negInvalidItemPositionOk -ne "true") { $finalBlocker = "negative_invalid_item_position_failed" }
        elseif ($measurePadShapeOriginOk -ne "true") { $finalBlocker = "measure_pad_shape_origin_contract_failed" }
        elseif ($measurePadAnchorToShapeOriginOk -ne "true") { $finalBlocker = "measure_pad_anchor_to_shape_origin_contract_failed" }
        elseif ($negPadShapeOriginMissingLayerOk -ne "true") { $finalBlocker = "negative_pad_shape_origin_missing_layer_failed" }
        elseif ($negPadShapeOriginNonPadOk -ne "true") { $finalBlocker = "negative_pad_shape_origin_non_pad_failed" }
        elseif ($negInvalidLayerOk -ne "true") { $finalBlocker = "negative_invalid_layer_failed" }
        elseif ($measurePadShapeOriginAliasFCuOk -ne "true") { $finalBlocker = "measure_pad_shape_origin_alias_f_cu_failed" }
        elseif ($negMissingAOk -ne "true") { $finalBlocker = "negative_missing_point_a_contract_failed" }
        else { $finalBlocker = "contract_diagnostic_unknown" }
    }

    Write-ContractSummary -Green $overallGreen -NominalOk $nominalOk -ZeroOk $zeroOk -SignedOk $signedOk `
        -NegMissingAOk $negMissingAOk -ItemItemOk $itemItemOk `
        -PadAnchorOk $padAnchorOk -NegPadAnchorNonPadOk $negPadAnchorNonPadOk -NegInvalidItemPositionOk $negInvalidItemPositionOk `
        -MeasurePadShapeOriginOk $measurePadShapeOriginOk -MeasurePadAnchorToShapeOriginOk $measurePadAnchorToShapeOriginOk `
        -NegPadShapeOriginMissingLayerOk $negPadShapeOriginMissingLayerOk -NegPadShapeOriginNonPadOk $negPadShapeOriginNonPadOk `
        -NegInvalidLayerOk $negInvalidLayerOk -MeasurePadShapeOriginAliasFCuOk $measurePadShapeOriginAliasFCuOk `
        -Blocker $finalBlocker `
        -Note "R23_PCB_Geometry_v1_2_pad_shape_origin" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)

    Write-Output ("TARGET_RUN_DIR=" + $runDir)

    if ($overallGreen -ne "true") {
        exit 1
    }
    exit 0
}
catch {
    $msg = $_.Exception.Message -replace "[\r\n]+", " "
    Write-ContractSummary -Green "false" -NominalOk "false" -ZeroOk "false" -SignedOk "false" -NegMissingAOk "false" -ItemItemOk "false" `
        -PadAnchorOk "false" -NegPadAnchorNonPadOk "false" -NegInvalidItemPositionOk "false" `
        -MeasurePadShapeOriginOk "false" -MeasurePadAnchorToShapeOriginOk "false" `
        -NegPadShapeOriginMissingLayerOk "false" -NegPadShapeOriginNonPadOk "false" -NegInvalidLayerOk "false" `
        -MeasurePadShapeOriginAliasFCuOk "false" `
        -Blocker "probe_exception" -Note $msg -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
