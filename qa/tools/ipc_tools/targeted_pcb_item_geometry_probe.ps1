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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-item-geometry-targeted")
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

function Parse-ItemGeometryOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
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
    return @{
        status_ok          = ($Text -match 'status=1')
        ok                 = $ok
        blocker            = $blocker
        item_type          = (& $parseStr "item_type")
        bbox_min_x_nm      = (& $parseNum "bbox_min_x_nm")
        bbox_center_x_nm   = (& $parseNum "bbox_center_x_nm")
        bbox_center_y_nm   = (& $parseNum "bbox_center_y_nm")
        pad_anchor_x_nm    = (& $parseNum "pad_anchor_x_nm")
        pad_anchor_y_nm    = (& $parseNum "pad_anchor_y_nm")
        pad_shape_origin_x_nm = (& $parseNum "pad_shape_origin_x_nm")
        pad_shape_origin_y_nm = (& $parseNum "pad_shape_origin_y_nm")
        layer_used         = (& $parseStr "layer_used")
        diagnostics        = (& $parseStr "diagnostics")
        raw                = $Text
    }
}

function Write-ContractSummary {
    param(
        [string]$Green,
        [string]$GeometryBasicOk,
        [string]$GeometryPadAnchorOk,
        [string]$GeometryPadShapeOriginOk,
        [string]$NegInvalidItemIdOk,
        [string]$NegItemNotFoundOk,
        [string]$NegInvalidLayerOk,
        [string]$Blocker,
        [string]$Note,
        [string]$IpcRetryExtraTotal = "0"
    )
    @(
        "pcb_item_geometry_green=$Green"
        "geometry_basic_ok=$GeometryBasicOk"
        "geometry_pad_anchor_ok=$GeometryPadAnchorOk"
        "geometry_pad_shape_origin_ok=$GeometryPadShapeOriginOk"
        "negative_invalid_item_id_ok=$NegInvalidItemIdOk"
        "negative_item_not_found_ok=$NegItemNotFoundOk"
        "negative_invalid_layer_ok=$NegInvalidLayerOk"
        "ipc_smoke_ipc_retry_extra_total=$IpcRetryExtraTotal"
        "exact_blocker_if_any=$Blocker"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        "case_note=$Note"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_item_geometry_session_contract_summary.txt") -Encoding UTF8
}

function Invoke-ItemGeometrySession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    # Geometry-heavy PCB + pad stack can exceed 120s on cold harness runs after qa_open_editor_frame.
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_pcb_item_geometry_session", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-ItemGeometryOutput -Text $out
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

$geometryBasicOk = "false"
$geometryPadAnchorOk = "false"
$geometryPadShapeOriginOk = "false"
$negInvalidItemIdOk = "false"
$negItemNotFoundOk = "false"
$negInvalidLayerOk = "false"
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
        Write-ContractSummary -Green "false" -GeometryBasicOk "false" -GeometryPadAnchorOk "false" -GeometryPadShapeOriginOk "false" `
            -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "ipc_url_unresolved" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    $ping = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $ping['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    if ($ping.Text -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -GeometryBasicOk "false" -GeometryPadAnchorOk "false" -GeometryPadShapeOriginOk "false" `
            -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "ping_failed" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        Write-ContractSummary -Green "false" -GeometryBasicOk "false" -GeometryPadAnchorOk "false" -GeometryPadShapeOriginOk "false" `
            -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
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
        Write-ContractSummary -Green "false" -GeometryBasicOk "false" -GeometryPadAnchorOk "false" -GeometryPadShapeOriginOk "false" `
            -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "qa_open_editor_frame_failed" -Note "fatal" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    Start-Sleep -Seconds 3

    # A) ipc-v1: generic item (drawing preferred for diversity vs pad tests below).
    $idDrw = Get-FirstItemIdFromBoardItemsSession -ApiUrl $url -LogLabel "item_geom_drw" -ItemTypes @("drawings")
    if (-not [string]::IsNullOrWhiteSpace($idDrw)) {
        $tA = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
            board_filename = $pcbAbs
            item_id        = $idDrw
        } -LogBaseName "item_geom_ipc_v1_drawing"
        $bboxOk = ($null -ne $tA.bbox_min_x_nm) -and ($null -ne $tA.bbox_center_x_nm) -and ($null -ne $tA.bbox_center_y_nm)
        if ($tA.status_ok -and $tA.ok -eq "true" -and $tA.blocker -eq "N/A" -and $bboxOk `
                -and (-not [string]::IsNullOrWhiteSpace($tA.item_type))) {
            $geometryBasicOk = "true"
        }
    }

    # B/C) ipc_geometry_v1 pad fixture (offset pad); negatives reuse same board.
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
            Start-Sleep -Seconds 8

            $tNoLayer = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                item_id        = $geomKid
            } -LogBaseName "item_geom_pad_no_layer"
            $noShapeKeys = ($tNoLayer.raw -notmatch 'key:\s+"pad_shape_origin_x_nm"')
            if ($tNoLayer.status_ok -and $tNoLayer.ok -eq "true" -and $tNoLayer.blocker -eq "N/A" `
                    -and $null -ne $tNoLayer.pad_anchor_x_nm -and $null -ne $tNoLayer.pad_anchor_y_nm `
                    -and $noShapeKeys -and ($tNoLayer.diagnostics -match "pad_shape_origin_skipped")) {
                $geometryPadAnchorOk = "true"
            }

            $tLayer = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                item_id        = $geomKid
                layer          = "BL_F_Cu"
            } -LogBaseName "item_geom_pad_bl_f_cu"
            $differs = $false
            if ($null -ne $tLayer.pad_anchor_x_nm -and $null -ne $tLayer.pad_shape_origin_x_nm) {
                $dx = [math]::Abs($tLayer.pad_anchor_x_nm - $tLayer.pad_shape_origin_x_nm)
                $dy = [math]::Abs($tLayer.pad_anchor_y_nm - $tLayer.pad_shape_origin_y_nm)
                if (($dx + $dy) -gt 0.5) { $differs = $true }
            }
            if ($tLayer.status_ok -and $tLayer.ok -eq "true" -and $tLayer.blocker -eq "N/A" `
                    -and $null -ne $tLayer.pad_shape_origin_x_nm -and $null -ne $tLayer.pad_shape_origin_y_nm `
                    -and ($tLayer.layer_used -eq "BL_F_Cu") -and $differs) {
                $geometryPadShapeOriginOk = "true"
            }

            $nInv = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                item_id        = "not-a-uuid"
            } -LogBaseName "item_geom_neg_invalid_id"
            if ($nInv.status_ok -and $nInv.ok -eq "false" -and $nInv.blocker -eq "invalid_item_id") {
                $negInvalidItemIdOk = "true"
            }

            $nNf = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                item_id        = "f0010001-0001-4001-8001-00000000abcd"
            } -LogBaseName "item_geom_neg_not_found"
            if ($nNf.status_ok -and $nNf.ok -eq "false" -and $nNf.blocker -eq "item_not_found") {
                $negItemNotFoundOk = "true"
            }

            $nLay = Invoke-ItemGeometrySession -ApiUrl $url -ArgsTable @{
                board_filename = $pcbGeomAbs
                item_id        = $geomKid
                layer          = "NOT_A_BOARD_LAYER_XYZ"
            } -LogBaseName "item_geom_neg_invalid_layer"
            if ($nLay.status_ok -and $nLay.ok -eq "false" -and $nLay.blocker -eq "invalid_layer") {
                $negInvalidLayerOk = "true"
            }
        }
    }

    if ($geometryBasicOk -eq "true" -and $geometryPadAnchorOk -eq "true" -and $geometryPadShapeOriginOk -eq "true" `
            -and $negInvalidItemIdOk -eq "true" -and $negItemNotFoundOk -eq "true" -and $negInvalidLayerOk -eq "true") {
        $overallGreen = "true"
        $finalBlocker = "N/A"
    }
    else {
        if ($geometryBasicOk -ne "true") { $finalBlocker = "geometry_basic_contract_failed" }
        elseif ($geometryPadAnchorOk -ne "true") { $finalBlocker = "geometry_pad_anchor_contract_failed" }
        elseif ($geometryPadShapeOriginOk -ne "true") { $finalBlocker = "geometry_pad_shape_origin_contract_failed" }
        elseif ($negInvalidItemIdOk -ne "true") { $finalBlocker = "negative_invalid_item_id_failed" }
        elseif ($negItemNotFoundOk -ne "true") { $finalBlocker = "negative_item_not_found_failed" }
        elseif ($negInvalidLayerOk -ne "true") { $finalBlocker = "negative_invalid_layer_failed" }
        else { $finalBlocker = "contract_diagnostic_unknown" }
    }

    Write-ContractSummary -Green $overallGreen -GeometryBasicOk $geometryBasicOk -GeometryPadAnchorOk $geometryPadAnchorOk `
        -GeometryPadShapeOriginOk $geometryPadShapeOriginOk -NegInvalidItemIdOk $negInvalidItemIdOk `
        -NegItemNotFoundOk $negItemNotFoundOk -NegInvalidLayerOk $negInvalidLayerOk `
        -Blocker $finalBlocker -Note "R26_get_pcb_item_geometry_session_V1" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)

    Write-Output ("TARGET_RUN_DIR=" + $runDir)

    if ($overallGreen -ne "true") {
        exit 1
    }
    exit 0
}
catch {
    $msg = $_.Exception.Message -replace "[\r\n]+", " "
    Write-ContractSummary -Green "false" -GeometryBasicOk "false" -GeometryPadAnchorOk "false" -GeometryPadShapeOriginOk "false" `
        -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
        -Blocker "probe_exception" -Note $msg -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
