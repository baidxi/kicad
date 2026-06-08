param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

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
    if (!(Test-Path -LiteralPath $src)) { throw "Missing ipc_drc_v1 fixture at $src" }
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
$netA = "QA_DRC_NET_A"
$netB = "QA_DRC_NET_B"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project" }
if (!(Test-Path $pcb)) { throw "Missing board" }

$pcbAbs = (Resolve-Path $pcb).Path
$wrongPcb = Join-Path $BuildDir "ipc-clearance-v1-proj\ipc_clearance_v1.kicad_pcb"

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-drc-violations-context-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$script:ProbeIpcRetryExtraTotal = 0

function Parse-DrcViolationsOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    if ($Text -match 'drc_ok=(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    if ($Text -match 'exact_blocker_if_any=([^\r\n]+)') { $blocker = $Matches[1].Trim() }
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?number_value:\s*([-0-9]+(?:\.[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        if ($Text -match ($Key + '=([-0-9]+)')) { return [double]$Matches[1] }
        return $null
    }
    $parseStr = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,1500}?string_value:\s*"([^"]*)"')) {
            return $Matches[1]
        }
        if ($Text -match ($Key + '=([^\r\n]+)')) { return $Matches[1].Trim() }
        return ""
    }
    $parseBool = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,800}?bool_value:\s*(true|false)')) { return $Matches[1] }
        if ($Text -match ($Key + '=(true|false)')) { return $Matches[1] }
        return ""
    }
    return @{
        status_ok         = ($Text -match 'status=1')
        ok                = $ok
        blocker           = $blocker
        is_official_drc   = (& $parseBool "is_official_drc")
        drc_was_run       = (& $parseBool "drc_was_run")
        drc_source        = (& $parseStr "drc_source")
        violations_count  = (& $parseNum "violations_count")
        returned_count    = (& $parseNum "returned_count")
        truncated         = (& $parseBool "truncated")
        raw               = $Text
    }
}

function Write-ContractSummary {
    param(
        [string]$Green,
        [string]$ExistingOk,
        [string]$RunDrcOk,
        [string]$OfficialOk,
        [string]$WasRunOk,
        [string]$ClearanceOk,
        [string]$NetsOk,
        [string]$ClearanceOnlyTypesOk,
        [string]$MaxLimitOk,
        [string]$NegMissingOk,
        [string]$NegMismatchOk,
        [string]$NegInvalidMaxOk,
        [string]$NegInvalidLayerOk,
        [string]$Blocker,
        [string]$IpcRetryExtraTotal = "0"
    )
    @(
        "pcb_drc_violations_context_green=$Green"
        "existing_markers_mode_ok=$ExistingOk"
        "run_drc_mode_ok=$RunDrcOk"
        "official_drc_flag_ok=$OfficialOk"
        "drc_was_run_ok=$WasRunOk"
        "clearance_violation_found_ok=$ClearanceOk"
        "expected_nets_found_ok=$NetsOk"
        "clearance_only_types_ok=$ClearanceOnlyTypesOk"
        "max_violations_limit_ok=$MaxLimitOk"
        "negative_missing_board_filename_ok=$NegMissingOk"
        "negative_board_filename_mismatch_ok=$NegMismatchOk"
        "negative_invalid_max_violations_ok=$NegInvalidMaxOk"
        "negative_invalid_layer_ok=$NegInvalidLayerOk"
        "ipc_smoke_ipc_retry_extra_total=$IpcRetryExtraTotal"
        "exact_blocker_if_any=$Blocker"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_drc_violations_context_summary.txt") -Encoding UTF8
}

function Invoke-DrcViolationsSession {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "get_pcb_drc_violations_context_session", ("@" + $jsonPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return Parse-DrcViolationsOutput -Text $out
}

$existingOk = "false"
$runDrcOk = "false"
$officialOk = "false"
$wasRunOk = "false"
$clearanceOk = "false"
$netsOk = "false"
$clearanceOnlyTypesOk = "false"
$maxLimitOk = "false"
$negMissingOk = "false"
$negMismatchOk = "false"
$negInvalidMaxOk = "false"
$negInvalidLayerOk = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }
    Start-Sleep -Seconds 3

    $tExisting = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        run_drc        = $false
    } -LogBaseName "drc_existing_markers"
    if ($tExisting.status_ok -and $tExisting.ok -eq "true" -and $tExisting.blocker -eq "N/A" `
            -and $tExisting.drc_source -eq "existing_markers" -and $tExisting.is_official_drc -eq "false" `
            -and $tExisting.drc_was_run -eq "false") {
        $existingOk = "true"
    }

    $tRun = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        run_drc        = $true
        max_violations = 50
    } -LogBaseName "drc_engine_run"
    $hasClearance = ($tRun.raw -match 'first_violation_type=clearance') -or ($tRun.raw -match 'drc_violation_types=[^\r\n]*clearance') -or ($tRun.raw -match 'string_value:\s*"clearance"')
    $hasNetA = ($tRun.raw -match [regex]::Escape($netA)) -or ($tRun.raw -match 'drc_violation_net_names=[^\r\n]*' + [regex]::Escape($netA))
    $hasNetB = ($tRun.raw -match [regex]::Escape($netB)) -or ($tRun.raw -match 'drc_violation_net_names=[^\r\n]*' + [regex]::Escape($netB))
    if ($tRun.status_ok -and $tRun.ok -eq "true" -and $tRun.blocker -eq "N/A" `
            -and $tRun.is_official_drc -eq "true" -and $tRun.drc_was_run -eq "true" `
            -and $tRun.drc_source -eq "engine_run" `
            -and $null -ne $tRun.violations_count -and $tRun.violations_count -ge 1 `
            -and $null -ne $tRun.returned_count -and $tRun.returned_count -ge 1) {
        $runDrcOk = "true"
        $officialOk = "true"
        $wasRunOk = "true"
    }
    if ($hasClearance) { $clearanceOk = "true" }
    if ($hasNetA -and $hasNetB) { $netsOk = "true" }
    if ($tRun.raw -match 'drc_violation_types=clearance[\r\n]' `
            -and $null -ne $tRun.violations_count -and $tRun.violations_count -eq 1) {
        $clearanceOnlyTypesOk = "true"
    }

    $tLimit = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        run_drc        = $true
        max_violations = 1
    } -LogBaseName "drc_max_violations_1"
    if ($tLimit.status_ok -and $tLimit.ok -eq "true" -and $tLimit.returned_count -eq 1) {
        if ($tLimit.violations_count -gt 1 -and $tLimit.truncated -eq "true") { $maxLimitOk = "true" }
        elseif ($tLimit.violations_count -eq 1 -and $tLimit.truncated -eq "false") { $maxLimitOk = "true" }
    }

    $tNegMissing = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{ run_drc = $true } -LogBaseName "drc_neg_missing_board"
    if ($tNegMissing.ok -eq "false" -and $tNegMissing.blocker -eq "missing_or_invalid_board_filename") { $negMissingOk = "true" }

    if (Test-Path -LiteralPath $wrongPcb) {
        $wrongAbs = (Resolve-Path $wrongPcb).Path
        $tNegMismatch = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
            board_filename = $wrongAbs
            run_drc        = $true
        } -LogBaseName "drc_neg_board_mismatch"
        if ($tNegMismatch.ok -eq "false" -and $tNegMismatch.blocker -eq "board_filename_mismatch") { $negMismatchOk = "true" }
    }
    else {
        $negMismatchOk = "skipped_no_clearance_fixture"
    }

    $tNegMax = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        run_drc        = $true
        max_violations = 0
    } -LogBaseName "drc_neg_invalid_max"
    if ($tNegMax.ok -eq "false" -and $tNegMax.blocker -eq "invalid_max_violations") { $negInvalidMaxOk = "true" }

    $tNegLayer = Invoke-DrcViolationsSession -ApiUrl $url -ArgsTable @{
        board_filename = $pcbAbs
        run_drc        = $false
        layer          = "NOT_A_REAL_LAYER"
    } -LogBaseName "drc_neg_invalid_layer"
    if ($tNegLayer.ok -eq "false" -and $tNegLayer.blocker -eq "invalid_layer") { $negInvalidLayerOk = "true" }

    $familyGreen = (
        $existingOk -eq "true" -and $runDrcOk -eq "true" -and $officialOk -eq "true" -and $wasRunOk -eq "true" `
        -and $clearanceOk -eq "true" -and $netsOk -eq "true" -and $clearanceOnlyTypesOk -eq "true" -and $maxLimitOk -eq "true" `
        -and $negMissingOk -eq "true" -and $negInvalidMaxOk -eq "true" -and $negInvalidLayerOk -eq "true" `
        -and ($negMismatchOk -eq "true" -or $negMismatchOk -like "skipped_*")
    )

    if (-not $familyGreen) {
        if ($existingOk -ne "true") { $finalBlocker = "existing_markers_mode_failed" }
        elseif ($runDrcOk -ne "true") { $finalBlocker = "run_drc_mode_failed" }
        elseif ($clearanceOk -ne "true") { $finalBlocker = "clearance_violation_not_found" }
        elseif ($netsOk -ne "true") { $finalBlocker = "expected_nets_not_found" }
        elseif ($clearanceOnlyTypesOk -ne "true") { $finalBlocker = "clearance_only_types_failed" }
        elseif ($maxLimitOk -ne "true") { $finalBlocker = "max_violations_limit_failed" }
        elseif ($negMissingOk -ne "true") { $finalBlocker = "negative_missing_board_filename_failed" }
        elseif ($negMismatchOk -ne "true" -and $negMismatchOk -notlike "skipped_*") { $finalBlocker = "negative_board_mismatch_failed" }
        elseif ($negInvalidMaxOk -ne "true") { $finalBlocker = "negative_invalid_max_violations_failed" }
        elseif ($negInvalidLayerOk -ne "true") { $finalBlocker = "negative_invalid_layer_failed" }
    }

    Write-ContractSummary -Green $(if ($familyGreen) { "true" } else { "false" }) `
        -ExistingOk $existingOk -RunDrcOk $runDrcOk -OfficialOk $officialOk -WasRunOk $wasRunOk `
        -ClearanceOk $clearanceOk -NetsOk $netsOk -ClearanceOnlyTypesOk $clearanceOnlyTypesOk -MaxLimitOk $maxLimitOk `
        -NegMissingOk $negMissingOk -NegMismatchOk $negMismatchOk -NegInvalidMaxOk $negInvalidMaxOk `
        -NegInvalidLayerOk $negInvalidLayerOk -Blocker $finalBlocker `
        -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    if (-not $familyGreen) {
        throw "pcb_drc_violations_context_green=false exact_blocker_if_any=$finalBlocker"
    }
    exit 0
}
catch {
    $msg = $_.Exception.Message
    if ($finalBlocker -eq "N/A" -and $msg) { $finalBlocker = $msg }
    Write-ContractSummary -Green "false" -ExistingOk $existingOk -RunDrcOk $runDrcOk -OfficialOk $officialOk `
        -WasRunOk $wasRunOk -ClearanceOk $clearanceOk -NetsOk $netsOk -ClearanceOnlyTypesOk $clearanceOnlyTypesOk -MaxLimitOk $maxLimitOk `
        -NegMissingOk $negMissingOk -NegMismatchOk $negMismatchOk -NegInvalidMaxOk $negInvalidMaxOk `
        -NegInvalidLayerOk $negInvalidLayerOk -Blocker $finalBlocker `
        -IpcRetryExtraTotal ([string]$script:ProbeIpcRetryExtraTotal)
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
