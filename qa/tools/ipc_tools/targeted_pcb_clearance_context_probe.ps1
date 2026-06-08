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

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-clearance-context-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$script:ProbeIpcRetryExtraTotal = 0

@(
    "captured_at=$( Get-Date -Format 'o' )"
    "PWD=$(( Get-Location ).Path)"
    "fixture_src=qa/data/pcbnew/clearance/ipc_clearance_v1"
    ""
) | Set-Content -Path (Join-Path $runDir "harness_runtime_env.txt") -Encoding UTF8

function Parse-ClearanceContextOutput {
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
    $trueDrc = ""
    if ($Text -match 'key:\s+"is_true_drc"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trueDrc = $Matches[1] }
    return @{
        status_ok              = ($Text -match 'status=1')
        ok                     = $ok
        blocker                = $blocker
        net_name               = (& $parseStr "net_name")
        netclass_name          = (& $parseStr "netclass_name")
        other_net_name         = (& $parseStr "other_net_name")
        other_netclass_name    = (& $parseStr "other_netclass_name")
        geometric_distance_nm  = (& $parseNum "geometric_distance_nm")
        required_clearance_nm  = (& $parseNum "required_clearance_nm")
        clearance_status       = (& $parseStr "clearance_status")
        geometric_distance_kind = (& $parseStr "geometric_distance_kind")
        is_true_drc            = $trueDrc
        diagnostics            = (& $parseStr "diagnostics")
        raw                    = $Text
    }
}

function Write-ContractSummary {
    param(
        [string]$Green,
        [string]$SingleOk,
        [string]$PairOk,
        [string]$NetclassOk,
        [string]$DistanceOk,
        [string]$NotTrueDrcOk,
        [string]$NegInvalidItemIdOk,
        [string]$NegItemNotFoundOk,
        [string]$NegInvalidLayerOk,
        [string]$Blocker,
        [string]$IpcRetryExtraTotal = "0"
    )
    @(
        "pcb_clearance_context_green=$Green"
        "clearance_single_item_ok=$SingleOk"
        "clearance_pair_ok=$PairOk"
        "clearance_netclass_ok=$NetclassOk"
        "clearance_distance_ok=$DistanceOk"
        "clearance_not_true_drc_ok=$NotTrueDrcOk"
        "negative_invalid_item_id_ok=$NegInvalidItemIdOk"
        "negative_item_not_found_ok=$NegItemNotFoundOk"
        "negative_invalid_layer_ok=$NegInvalidLayerOk"
        "ipc_smoke_ipc_retry_extra_total=$IpcRetryExtraTotal"
        "exact_blocker_if_any=$Blocker"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_clearance_context_session_contract_summary.txt") -Encoding UTF8
}

function Invoke-ClearanceContextSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_pcb_clearance_context_session", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-ClearanceContextOutput -Text $out
}

$clearanceSingleOk = "false"
$clearancePairOk = "false"
$clearanceNetclassOk = "false"
$clearanceDistanceOk = "false"
$clearanceNotTrueDrcOk = "false"
$negInvalidItemIdOk = "false"
$negItemNotFoundOk = "false"
$negInvalidLayerOk = "false"
$overallGreen = "false"
$finalBlocker = "N/A"

$kicadProc = $null

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if ($resolved.Resolved) { $resolved.Url } else { $null }
    if (-not $url) {
        Write-ContractSummary -Green "false" -SingleOk "false" -PairOk "false" -NetclassOk "false" -DistanceOk "false" `
            -NotTrueDrcOk "false" -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "ipc_url_unresolved" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    $ping = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $ping['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    if ($ping.Text -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -SingleOk "false" -PairOk "false" -NetclassOk "false" -DistanceOk "false" `
            -NotTrueDrcOk "false" -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "ping_failed" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        Write-ContractSummary -Green "false" -SingleOk "false" -PairOk "false" -NetclassOk "false" -DistanceOk "false" `
            -NotTrueDrcOk "false" -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "project_not_loaded" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $openSm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $openSm.Text | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if ($openSm.Text -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -SingleOk "false" -PairOk "false" -NetclassOk "false" -DistanceOk "false" `
            -NotTrueDrcOk "false" -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
            -Blocker "qa_open_editor_frame_failed" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    Start-Sleep -Seconds 3

    # Single pad A — net / netclass / not DRC
    $tSingle = Invoke-ClearanceContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = $kidA
        layer          = "BL_F_Cu"
    } -LogBaseName "clearance_ctx_single_a"

    $singleContract = $false
    if ($tSingle.status_ok -and $tSingle.ok -eq "true" -and $tSingle.blocker -eq "N/A" `
            -and $tSingle.net_name -eq "QA_NET_A" -and $tSingle.netclass_name -eq "QA_NETCLASS_A" `
            -and $tSingle.is_true_drc -eq "false") {
        $singleContract = $true
        $clearanceSingleOk = "true"
    }

    if ($tSingle.is_true_drc -eq "false") {
        $clearanceNotTrueDrcOk = "true"
    }

    # Pair A/B — distance bbox-centre (fixture README: ~1.5e9 nm), nets B, nominal max clearance
    $tPair = Invoke-ClearanceContextSession -ApiUrl $url -ArgsTable @{
        board_filename    = $pcbAbs
        item_id           = $kidA
        other_item_id     = $kidB
        layer             = "F_Cu"
    } -LogBaseName "clearance_ctx_pair_ab"

    $expectedGeoNm = 1500000000.0
    $geoTol = 500000.0
    $pairContract = $false
    if ($tPair.status_ok -and $tPair.ok -eq "true" -and $tPair.blocker -eq "N/A" `
            -and $tPair.other_net_name -eq "QA_NET_B" -and $tPair.other_netclass_name -eq "QA_NETCLASS_B" `
            -and $tPair.is_true_drc -eq "false") {
        $pairContract = $true
        $clearancePairOk = "true"
    }

    $distContract = $false
    if ($null -ne $tPair.geometric_distance_nm) {
        $d = [math]::Abs($tPair.geometric_distance_nm - $expectedGeoNm)
        if ($d -le $geoTol -and ($tPair.geometric_distance_kind -match "bbox_center")) {
            $distContract = $true
            $clearanceDistanceOk = "true"
        }
    }

    if ($singleContract -and $pairContract) {
        $clearanceNetclassOk = "true"
    }

    # Negatives
    $nInv = Invoke-ClearanceContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = "not-a-uuid"
        layer          = "BL_F_Cu"
    } -LogBaseName "clearance_ctx_neg_invalid_id"
    if ($nInv.status_ok -and $nInv.ok -eq "false" -and $nInv.blocker -eq "invalid_item_id") {
        $negInvalidItemIdOk = "true"
    }

    $nNf = Invoke-ClearanceContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = "f0d10001-0001-4001-8001-00000000abcd"
        layer          = "BL_F_Cu"
    } -LogBaseName "clearance_ctx_neg_not_found"
    if ($nNf.status_ok -and $nNf.ok -eq "false" -and $nNf.blocker -eq "item_not_found") {
        $negItemNotFoundOk = "true"
    }

    $nLay = Invoke-ClearanceContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = $kidA
        other_item_id  = $kidB
        layer          = "NOT_A_BOARD_LAYER_XYZ"
    } -LogBaseName "clearance_ctx_neg_invalid_layer"
    if ($nLay.status_ok -and $nLay.ok -eq "false" -and $nLay.blocker -eq "invalid_layer") {
        $negInvalidLayerOk = "true"
    }

    if ($clearanceSingleOk -eq "true" -and $clearancePairOk -eq "true" -and $clearanceNetclassOk -eq "true" `
            -and $clearanceDistanceOk -eq "true" -and $clearanceNotTrueDrcOk -eq "true" `
            -and $negInvalidItemIdOk -eq "true" -and $negItemNotFoundOk -eq "true" -and $negInvalidLayerOk -eq "true") {
        $overallGreen = "true"
        $finalBlocker = "N/A"
    }
    else {
        if ($clearanceSingleOk -ne "true") { $finalBlocker = "clearance_single_item_contract_failed" }
        elseif ($clearancePairOk -ne "true") { $finalBlocker = "clearance_pair_contract_failed" }
        elseif ($clearanceNetclassOk -ne "true") { $finalBlocker = "clearance_netclass_contract_failed" }
        elseif ($clearanceDistanceOk -ne "true") { $finalBlocker = "clearance_distance_contract_failed" }
        elseif ($clearanceNotTrueDrcOk -ne "true") { $finalBlocker = "clearance_not_true_drc_contract_failed" }
        elseif ($negInvalidItemIdOk -ne "true") { $finalBlocker = "negative_invalid_item_id_failed" }
        elseif ($negItemNotFoundOk -ne "true") { $finalBlocker = "negative_item_not_found_failed" }
        elseif ($negInvalidLayerOk -ne "true") { $finalBlocker = "negative_invalid_layer_failed" }
        else { $finalBlocker = "contract_diagnostic_unknown" }
    }

    Write-ContractSummary -Green $overallGreen -SingleOk $clearanceSingleOk -PairOk $clearancePairOk `
        -NetclassOk $clearanceNetclassOk -DistanceOk $clearanceDistanceOk -NotTrueDrcOk $clearanceNotTrueDrcOk `
        -NegInvalidItemIdOk $negInvalidItemIdOk -NegItemNotFoundOk $negItemNotFoundOk -NegInvalidLayerOk $negInvalidLayerOk `
        -Blocker $finalBlocker -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)

    Write-Output ("TARGET_RUN_DIR=" + $runDir)

    if ($overallGreen -ne "true") {
        exit 1
    }
    exit 0
}
catch {
    $msg = $_.Exception.Message -replace "[\r\n]+", " "
    Write-ContractSummary -Green "false" -SingleOk "false" -PairOk "false" -NetclassOk "false" -DistanceOk "false" `
        -NotTrueDrcOk "false" -NegInvalidItemIdOk "false" -NegItemNotFoundOk "false" -NegInvalidLayerOk "false" `
        -Blocker ("probe_exception: " + $msg) -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    if ($null -ne $kicadProc) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
}
