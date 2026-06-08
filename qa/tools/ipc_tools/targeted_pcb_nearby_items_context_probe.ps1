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
$kidQSAM = "f0e10002-0002-4002-8002-000000000102"
$kidQNIN = "f0e10003-0003-4003-8003-000000000103"
$kidQXCL = "f0e10004-0004-4004-8004-000000000104"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-nearby-items-context-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$script:ProbeIpcRetryExtraTotal = 0

@(
    "captured_at=$( Get-Date -Format 'o' )"
    "PWD=$(( Get-Location ).Path)"
    "fixture_src=qa/data/pcbnew/nearby/ipc_nearby_v1"
    ""
) | Set-Content -Path (Join-Path $runDir "harness_runtime_env.txt") -Encoding UTF8

function Parse-NearbyItemsContextOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $trueDrc = ""
    if ($Text -match 'key:\s+"is_true_drc"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trueDrc = $Matches[1] }
    $rc = $null
    if ($Text -match 'key:\s+"result_count"[\s\S]{0,800}?number_value:\s*([-0-9]+(?:\.[0-9]+)?)') { $rc = [double]$Matches[1] }
    $trunc = ""
    if ($Text -match 'key:\s+"truncated"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trunc = $Matches[1] }
    $diag = ""
    if ($Text -match 'key:\s+"diagnostics"[\s\S]{0,1500}?string_value:\s*"([^"]*)"') { $diag = $Matches[1] }
    return @{
        status_ok    = ($Text -match 'status=1')
        ok           = $ok
        blocker      = $blocker
        is_true_drc  = $trueDrc
        result_count = $rc
        truncated    = $trunc
        diagnostics  = $diag
        raw          = $Text
    }
}

function Get-OrderedNearbyItemIds {
    param([string]$Text)
    $ids = [System.Collections.Generic.List[string]]::new()
    # Root-level tool args echo includes key "item_id" (query item); only collect IDs under "items".
    $segment = $Text
    $marker = 'key: "items"'
    $ix = $Text.IndexOf($marker)
    if ($ix -ge 0) {
        $segment = $Text.Substring($ix)
    }
    $rx = [regex]::new('key:\s+"item_id"[\s\S]{0,800}?string_value:\s*"([^"]+)"', 'Singleline')
    foreach ($m in $rx.Matches($segment)) {
        $ids.Add($m.Groups[1].Value) | Out-Null
    }
    return $ids
}

function Write-ContractSummary {
    param(
        [string]$Green,
        [string]$RadiusIncludesOk,
        [string]$RadiusExcludesOk,
        [string]$SameNetOk,
        [string]$MaxItemsOk,
        [string]$NotTrueDrcOk,
        [string]$NegLayerOk,
        [string]$NegInvalidIdOk,
        [string]$NegNotFoundOk,
        [string]$Blocker,
        [string]$IpcRetryExtraTotal = "0"
    )
    @(
        "pcb_nearby_items_context_green=$Green"
        "nearby_radius_includes_expected_ok=$RadiusIncludesOk"
        "nearby_radius_excludes_expected_ok=$RadiusExcludesOk"
        "nearby_include_same_net_ok=$SameNetOk"
        "nearby_max_items_truncation_ok=$MaxItemsOk"
        "nearby_not_true_drc_ok=$NotTrueDrcOk"
        "negative_invalid_layer_ok=$NegLayerOk"
        "negative_invalid_item_id_ok=$NegInvalidIdOk"
        "negative_item_not_found_ok=$NegNotFoundOk"
        "ipc_smoke_ipc_retry_extra_total=$IpcRetryExtraTotal"
        "exact_blocker_if_any=$Blocker"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_nearby_items_context_session_contract_summary.txt") -Encoding UTF8
}

function Invoke-NearbyItemsContextSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_pcb_nearby_items_context_session", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-NearbyItemsContextOutput -Text $out
}

$nearbyRadiusIncludesOk = "false"
$nearbyRadiusExcludesOk = "false"
$nearbySameNetOk = "false"
$nearbyMaxItemsOk = "false"
$nearbyNotTrueDrcOk = "false"
$negInvalidLayerOk = "false"
$negInvalidItemIdOk = "false"
$negItemNotFoundOk = "false"
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
        Write-ContractSummary -Green "false" -RadiusIncludesOk "false" -RadiusExcludesOk "false" -SameNetOk "false" `
            -MaxItemsOk "false" -NotTrueDrcOk "false" -NegLayerOk "false" -NegInvalidIdOk "false" -NegNotFoundOk "false" `
            -Blocker "ipc_url_unresolved" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    $ping = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $ping['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    if ($ping.Text -notmatch 'status=1') {
        Write-ContractSummary -Green "false" -RadiusIncludesOk "false" -RadiusExcludesOk "false" -SameNetOk "false" `
            -MaxItemsOk "false" -NotTrueDrcOk "false" -NegLayerOk "false" -NegInvalidIdOk "false" -NegNotFoundOk "false" `
            -Blocker "ping_failed" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        Write-ContractSummary -Green "false" -RadiusIncludesOk "false" -RadiusExcludesOk "false" -SameNetOk "false" `
            -MaxItemsOk "false" -NotTrueDrcOk "false" -NegLayerOk "false" -NegInvalidIdOk "false" -NegNotFoundOk "false" `
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
        Write-ContractSummary -Green "false" -RadiusIncludesOk "false" -RadiusExcludesOk "false" -SameNetOk "false" `
            -MaxItemsOk "false" -NotTrueDrcOk "false" -NegLayerOk "false" -NegInvalidIdOk "false" -NegNotFoundOk "false" `
            -Blocker "qa_open_editor_frame_failed" -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    Start-Sleep -Seconds 3

    $padsOnly = @("pads")

    # A — radius 400e6 nm, include_same_net=true : QSAM + QNIN, QXCL absent; distances 200e6 / 300e6; order
    $tA = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename    = $pcbAbs
        item_id           = $kidTgt
        radius_nm         = 400000000
        layer             = "BL_F_Cu"
        include_same_net  = $true
        item_types        = $padsOnly
    } -LogBaseName "nearby_ctx_a_radius400_same_net"

    $rawA = $tA.raw
    $idsA = @( Get-OrderedNearbyItemIds -Text $rawA )
    $contractA = $false
    if ($tA.status_ok -and $tA.ok -eq "true" -and $tA.blocker -eq "N/A" -and ($idsA -contains $kidQSAM) -and ($idsA -contains $kidQNIN) `
            -and ($idsA -notcontains $kidQXCL) -and ($rawA -match 'number_value:\s*200000000') -and ($rawA -match 'number_value:\s*300000000')) {
        $idxSam = $idsA.IndexOf($kidQSAM)
        $idxNin = $idsA.IndexOf($kidQNIN)
        if ($idxSam -ge 0 -and $idxNin -ge 0 -and $idxSam -lt $idxNin) {
            $contractA = $true
            $nearbyRadiusIncludesOk = "true"
        }
    }

    # B — radius 250e6 : QSAM only among neighbours
    $tB = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename    = $pcbAbs
        item_id           = $kidTgt
        radius_nm         = 250000000
        layer             = "F_Cu"
        include_same_net  = $true
        item_types        = $padsOnly
    } -LogBaseName "nearby_ctx_b_radius250"

    $rawB = $tB.raw
    $idsB = @( Get-OrderedNearbyItemIds -Text $rawB )
    if ($tB.status_ok -and $tB.ok -eq "true" -and ($idsB -contains $kidQSAM) -and ($idsB -notcontains $kidQNIN) -and ($idsB -notcontains $kidQXCL)) {
        $nearbyRadiusExcludesOk = "true"
    }

    # C — include_same_net=false, radius 400e6 : QSAM excluded, QNIN present
    $tC = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename    = $pcbAbs
        item_id           = $kidTgt
        radius_nm         = 400000000
        layer             = "BL_F_Cu"
        include_same_net  = $false
        item_types        = $padsOnly
    } -LogBaseName "nearby_ctx_c_exclude_same_net"

    $rawC = $tC.raw
    $idsC = @( Get-OrderedNearbyItemIds -Text $rawC )
    if ($tC.status_ok -and $tC.ok -eq "true" -and ($idsC -notcontains $kidQSAM) -and ($idsC -contains $kidQNIN)) {
        $nearbySameNetOk = "true"
    }

    # D — max_items=1, large radius : count 1, truncated, first = QSAM
    $tD = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename    = $pcbAbs
        item_id           = $kidTgt
        radius_nm         = 999999999999
        layer             = "BL_F_Cu"
        max_items         = 1
        include_same_net  = $true
        item_types        = $padsOnly
    } -LogBaseName "nearby_ctx_d_max1"

    $idsD = @( Get-OrderedNearbyItemIds -Text $tD.raw )
    if ($tD.status_ok -and $tD.ok -eq "true" -and $tD.result_count -eq 1 -and $tD.truncated -eq "true" `
            -and $idsD.Count -ge 1 -and $idsD[0] -eq $kidQSAM) {
        $nearbyMaxItemsOk = "true"
    }

    if ($tA.is_true_drc -eq "false" -and $tD.is_true_drc -eq "false") {
        $nearbyNotTrueDrcOk = "true"
    }

    # E — invalid layer
    $tE = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = $kidTgt
        radius_nm      = 400000000
        layer          = "NOT_A_BOARD_LAYER_XYZ"
        item_types     = $padsOnly
    } -LogBaseName "nearby_ctx_e_invalid_layer"

    if ($tE.status_ok -and $tE.ok -eq "false" -and $tE.blocker -eq "invalid_layer") {
        $negInvalidLayerOk = "true"
    }

    # F — invalid item_id
    $tF = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = "not-a-uuid"
        radius_nm      = 400000000
        layer          = "BL_F_Cu"
        item_types     = $padsOnly
    } -LogBaseName "nearby_ctx_f_invalid_id"

    if ($tF.status_ok -and $tF.ok -eq "false" -and $tF.blocker -eq "invalid_item_id") {
        $negInvalidItemIdOk = "true"
    }

    # G — item not found
    $tG = Invoke-NearbyItemsContextSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        item_id        = "f0e10001-0001-4001-8001-00000000abcd"
        radius_nm      = 400000000
        layer          = "BL_F_Cu"
        item_types     = $padsOnly
    } -LogBaseName "nearby_ctx_g_not_found"

    if ($tG.status_ok -and $tG.ok -eq "false" -and $tG.blocker -eq "item_not_found") {
        $negItemNotFoundOk = "true"
    }

    if ($nearbyRadiusIncludesOk -eq "true" -and $nearbyRadiusExcludesOk -eq "true" -and $nearbySameNetOk -eq "true" `
            -and $nearbyMaxItemsOk -eq "true" -and $nearbyNotTrueDrcOk -eq "true" `
            -and $negInvalidLayerOk -eq "true" -and $negInvalidItemIdOk -eq "true" -and $negItemNotFoundOk -eq "true") {
        $overallGreen = "true"
        $finalBlocker = "N/A"
    }
    else {
        if ($nearbyRadiusIncludesOk -ne "true") { $finalBlocker = "nearby_radius_includes_contract_failed" }
        elseif ($nearbyRadiusExcludesOk -ne "true") { $finalBlocker = "nearby_radius_excludes_contract_failed" }
        elseif ($nearbySameNetOk -ne "true") { $finalBlocker = "nearby_include_same_net_contract_failed" }
        elseif ($nearbyMaxItemsOk -ne "true") { $finalBlocker = "nearby_max_items_contract_failed" }
        elseif ($nearbyNotTrueDrcOk -ne "true") { $finalBlocker = "nearby_not_true_drc_contract_failed" }
        elseif ($negInvalidLayerOk -ne "true") { $finalBlocker = "negative_invalid_layer_failed" }
        elseif ($negInvalidItemIdOk -ne "true") { $finalBlocker = "negative_invalid_item_id_failed" }
        elseif ($negItemNotFoundOk -ne "true") { $finalBlocker = "negative_item_not_found_failed" }
        else { $finalBlocker = "contract_diagnostic_unknown" }
    }

    Write-ContractSummary -Green $overallGreen -RadiusIncludesOk $nearbyRadiusIncludesOk -RadiusExcludesOk $nearbyRadiusExcludesOk `
        -SameNetOk $nearbySameNetOk -MaxItemsOk $nearbyMaxItemsOk -NotTrueDrcOk $nearbyNotTrueDrcOk `
        -NegLayerOk $negInvalidLayerOk -NegInvalidIdOk $negInvalidItemIdOk -NegNotFoundOk $negItemNotFoundOk `
        -Blocker $finalBlocker -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)

    Write-Output ("TARGET_RUN_DIR=" + $runDir)

    if ($overallGreen -ne "true") {
        exit 1
    }
    exit 0
}
catch {
    $msg = $_.Exception.Message -replace "[\r\n]+", " "
    Write-ContractSummary -Green "false" -RadiusIncludesOk "false" -RadiusExcludesOk "false" -SameNetOk "false" `
        -MaxItemsOk "false" -NotTrueDrcOk "false" -NegLayerOk "false" -NegInvalidIdOk "false" -NegNotFoundOk "false" `
        -Blocker ("probe_exception: " + $msg) -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    if ($null -ne $kicadProc) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
}
