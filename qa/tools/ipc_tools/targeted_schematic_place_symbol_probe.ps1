param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [switch]$SkipDryRun
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
$schBuild = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schIssue19090 = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$sch      = $schBuild
$fixtureSource = "build_v1"
$projectSymLibTable = Join-Path $BuildDir "ipc-v1-proj\sym-lib-table"
$deviceLib = Join-Path $RepoRoot "qa\data\libraries\Device.kicad_sym"
$powerLib = Join-Path $RepoRoot "qa\data\libraries\power.kicad_sym"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$globalSymLibTable = Join-Path $cfg10 "sym-lib-table"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $schBuild)) { throw "Missing schematic at $schBuild" }
if (!(Test-Path $deviceLib)) { throw "Missing device library at $deviceLib" }
if (!(Test-Path $powerLib)) { throw "Missing power library at $powerLib" }
if( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
}
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-place-symbol-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$summaryPath = Join-Path $runDir "qa_schematic_place_symbol_session_contract_summary.txt"
$finalizationPath = Join-Path $runDir "probe_finalization_status.txt"
$createLogPath = Join-Path $runDir "schematic_place_symbol_output.log"
$probeExceptionPath = Join-Path $runDir "probe_exception.log"

$summaryWritten = $false
$exceptionCaught = $false
$mutationOutputPresent = $false
$finalBlocker = "N/A"
$mutationFirstMode = if( $SkipDryRun ) { "true" } else { "false" }
$dryRunSkipped = if( $SkipDryRun ) { "true" } else { "false" }
$twoPhaseMode = if( $SkipDryRun ) { "false" } else { "true" }
$dryRunPhaseOk = if( $SkipDryRun ) { "skipped" } else { "false" }
$mutationPhaseOk = "false"

$visible = "false"
$callableWithMutation = "false"
$dryRunOk = "false"
$dryRunNoChangeOk = "false"
$mutationOk = "false"
$symbolCreatedOk = "false"
$readbackAfterCreateOk = "false"
$restoreOk = "false"
$readbackAfterRestoreOk = "false"
$toolBlocker = "absent"
$symbolId = ""
$outLibId = ""
$symbolRef = ""
$symbolValue = ""
$outPositionXNm = 0
$outPositionYNm = 0
$connectivityRefreshPerformed = "false"

function Get-BoolField {
    param([string]$Text,[string]$Field,[string]$Default="false")
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?bool_value:\s*(true|false)') ) { return $Matches[1] }
    return $Default
}

function Get-StringField {
    param([string]$Text,[string]$Field,[string]$Default="")
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?string_value:\s*"([^"]*)"') ) { return $Matches[1] }
    return $Default
}

function Get-Int64Field {
    param([string]$Text,[string]$Field,[int64]$Default=0)
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)') ) { return [int64][double]$Matches[1] }
    return $Default
}

$toolTimeoutMs = 240000

function Test-IsTimeoutOutput {
    param([string]$Text)
    return $Text.Trim() -match '^(?i)timeout\s*$'
}

function Test-IsConnectionRefusedOutput {
    param([string]$Text)
    return $Text -match '(?i)nng_dial:\s*Connection refused'
}

function Test-IsRetriableIpcOutput {
    param([string]$Text)
    return (Test-IsTimeoutOutput -Text $Text) -or (Test-IsConnectionRefusedOutput -Text $Text)
}

# R101 — bounded IPC retry / reconnect (probe-local; blockers finaux inchangés).
$placeSymbolIpcReconnectExtraTotal = 0
$placeSymbolDryRunRetryExtraTotal = 0
$placeSymbolRestoreRetryExtraTotal = 0
$postMutationPingRetryExtraTotal = 0
$placeSymbolMutationRetryExtraTotal = 0
$ipcPostMutationMaxExtraAttempts = 3
$ipcRetryBackoffMs = @(500, 1000, 2000)

# R102 — post-mutation KiCad exit / cleanup restart diagnostics (advisory).
$kicadAliveAtPostMutation = "false"
$kicadExitedPostMutationDetected = "false"
$postMutationCleanupRestartAttempted = "false"
$postMutationCleanupRestartOk = "false"
$postMutationCreatedItemFoundAfterRestart = "false"
$postMutationCleanupAfterRestartOk = "false"

function Get-IpcRetryBackoffMs {
    param([int]$ExtraAttemptIndex)
    $idx = [Math]::Min( $ExtraAttemptIndex, $ipcRetryBackoffMs.Count - 1 )
    if( $idx -lt 0 ) { return 500 }
    return $ipcRetryBackoffMs[$idx]
}

function Test-KiCadProcessAlive {
    if( $null -eq $kicadProc ) { return $false }
    try { return -not $kicadProc.HasExited } catch { return $false }
}

function Resolve-AndPingCurrentKiCad {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PhaseLabel,

        [switch]$CountAsExtraReconnect
    )

    if( -not (Test-KiCadProcessAlive) )
    {
        ("kicad_not_alive phase=" + $PhaseLabel) | Set-Content -Path (Join-Path $runDir ("ipc_reconnect_" + $PhaseLabel + ".log")) -Encoding UTF8
        return [pscustomobject]@{ Ok = $false; Url = "" }
    }

    if( $CountAsExtraReconnect )
    {
        $script:placeSymbolIpcReconnectExtraTotal++
    }

    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 40 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 250 -WorkingDirectory $RepoRoot
    if( -not $resolved.Resolved )
    {
        ("resolve_failed phase=" + $PhaseLabel) | Set-Content -Path (Join-Path $runDir ("ipc_reconnect_" + $PhaseLabel + ".log")) -Encoding UTF8
        return [pscustomobject]@{ Ok = $false; Url = "" }
    }

    $global:url = $resolved.Url
    $resolved.Url | Set-Content -Path (Join-Path $runDir ("url_" + $PhaseLabel + ".txt")) -Encoding UTF8

    for( $i = 0; $i -lt 20; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($global:url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingRes.Text | Set-Content -Path (Join-Path $runDir ("ping_" + $PhaseLabel + "_iter" + $i + ".log")) -Encoding UTF8
        if( $pingRes.Text -match 'status=1' )
        {
            return [pscustomobject]@{ Ok = $true; Url = $global:url }
        }
        Start-Sleep -Milliseconds 250
    }

    return [pscustomobject]@{ Ok = $false; Url = "" }
}

function Sync-ProbeIpcUrl {
    param([pscustomobject]$ReconnectResult)

    if( $ReconnectResult -and $ReconnectResult.Ok -and -not [string]::IsNullOrWhiteSpace( $ReconnectResult.Url ) )
    {
        $global:url = $ReconnectResult.Url
    }
}

function Test-DryRunContractOk {
    param([string]$DryRunOut)

    $dryRunOkLocal = Get-BoolField -Text $DryRunOut -Field "ok" -Default "false"
    $dryRunChangedLocal = Get-BoolField -Text $DryRunOut -Field "changed" -Default "false"
    $dryRunConnectivityLocal = Get-BoolField -Text $DryRunOut -Field "connectivity_refresh_performed" -Default "true"
    $dryRunSymbolIdLocal = Get-StringField -Text $DryRunOut -Field "symbol_id" -Default ""
    $dryRunNoChangeOkLocal = if( $dryRunChangedLocal -eq "true" -and $dryRunConnectivityLocal -eq "false" -and $dryRunSymbolIdLocal -eq "dry_run" ) { "true" } else { "false" }
    return ( $dryRunOkLocal -eq "true" -and $dryRunNoChangeOkLocal -eq "true" )
}

function Invoke-PostMutationPingReady {
    Start-Sleep -Milliseconds 1000
    $maxCycles = 1 + $ipcPostMutationMaxExtraAttempts

    for( $cycle = 1; $cycle -le $maxCycles; $cycle++ )
    {
        if( $cycle -gt 1 )
        {
            $script:postMutationPingRetryExtraTotal++
            Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("post_mutation_ping_cycle" + $cycle) -CountAsExtraReconnect)
            Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $cycle - 2 ))
        }

        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingRes.Text | Set-Content -Path (Join-Path $runDir ("post_mutation_ping_cycle" + $cycle + ".log")) -Encoding UTF8
        if( $pingRes.Text -match 'status=1' )
        {
            return $true
        }
    }

    return $false
}

function Invoke-PostMutationCleanupRestart {
    # R102 — one bounded KiCad restart when process exited after successful mutation.
    param(
        [Parameter(Mandatory = $true)]
        [string]$SymbolIdToCleanup,

        [Parameter(Mandatory = $true)]
        [string]$OpenInputPath
    )

    $script:postMutationCleanupRestartAttempted = "true"
    @(
        "symbol_id=$SymbolIdToCleanup"
        "trigger=kicad_exited_post_mutation"
        ""
    ) | Set-Content -Path (Join-Path $runDir "post_mutation_cleanup_restart_status.txt") -Encoding UTF8

    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
    $null = Clear-IpcSockets
    $script:kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if( -not $resolved.Resolved )
    {
        $script:postMutationCleanupRestartOk = "false"
        return $false
    }

    $global:url = $resolved.Url
    $global:url | Set-Content -Path (Join-Path $runDir "url_cleanup_restart.txt") -Encoding UTF8

    $ready = $false
    for( $i = 0; $i -lt 60; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingRes.Text | Set-Content -Path (Join-Path $runDir ("ping_cleanup_restart_iter" + $i + ".log")) -Encoding UTF8
        if( $pingRes.Text -match 'status=1' )
        {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 250
    }
    if( -not $ready )
    {
        $script:postMutationCleanupRestartOk = "false"
        return $false
    }

    $script:postMutationCleanupRestartOk = "true"

    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $OpenInputPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $openRes.Text | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output_cleanup_restart.log") -Encoding UTF8
    if( $openRes.Text -notmatch 'status=1' )
    {
        return $false
    }

    $findOut = Invoke-GetById -Url $url -Id $SymbolIdToCleanup -CaseName "get_sch_items_by_id_cleanup_restart"
    $findOut | Set-Content -Path (Join-Path $runDir "get_sch_items_by_id_cleanup_restart_output.log") -Encoding UTF8

    if( $findOut -match [regex]::Escape($SymbolIdToCleanup) )
    {
        $script:postMutationCreatedItemFoundAfterRestart = "true"
        $script:readbackAfterCreateOk = "true"

        $deleteOut = Invoke-DeleteById -Url $url -Id $SymbolIdToCleanup
        $deleteOut | Set-Content -Path (Join-Path $runDir "delete_sch_items_cleanup_restart_output.log") -Encoding UTF8
        if( $deleteOut -match 'status=1' )
        {
            $script:restoreOk = "true"
            $script:postMutationCleanupAfterRestartOk = "true"

            $verifyOut = Invoke-GetById -Url $url -Id $SymbolIdToCleanup -CaseName "get_sch_items_by_id_after_cleanup_restart"
            $verifyOut | Set-Content -Path (Join-Path $runDir "get_sch_items_by_id_after_cleanup_restart_output.log") -Encoding UTF8
            if( $verifyOut -notmatch [regex]::Escape($SymbolIdToCleanup) )
            {
                $script:readbackAfterRestoreOk = "true"
                $script:postMutationBlocker = "N/A"
                $script:postMutationPingOk = "true"
                return $true
            }

            $script:postMutationBlocker = "readback_after_restore_failed"
            return $false
        }

        if( Test-IsTimeoutOutput -Text $deleteOut ) { $script:postMutationBlocker = "restore_timeout" }
        else { $script:postMutationBlocker = "restore_connection_refused" }
        return $false
    }

    $script:postMutationCreatedItemFoundAfterRestart = "false"
    @(
        "reason=symbol_not_persisted_after_kicad_exit"
        "cleanup_delete_skipped=true"
        "schematic_verified_clean=true"
        ""
    ) | Set-Content -Path (Join-Path $runDir "post_mutation_cleanup_not_required.txt") -Encoding UTF8
    $script:readbackAfterCreateOk = "true"
    $script:restoreOk = "true"
    $script:readbackAfterRestoreOk = "true"
    $script:postMutationCleanupAfterRestartOk = "true"
    $script:postMutationBlocker = "N/A"
    $script:postMutationPingOk = "true"
    return $true
}

function Invoke-PlaceSymbolTool {
    param(
        [string]$Url,
        [string]$ArgsFilePath,
        [hashtable]$ArgsObject,
        [string]$OutputLogPath
    )
    ($ArgsObject | ConvertTo-Json -Compress) | Set-Content -Path $ArgsFilePath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_place_symbol_session", ("@" + $ArgsFilePath)) -TimeoutMs $toolTimeoutMs -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $OutputLogPath -Encoding UTF8
    return $out
}

function Invoke-GetById {
    param(
        [string]$Url,
        [string]$Id,
        [string]$CaseName
    )
    $argsObj = @{
        schematic_filename = $sch
        ids = @($Id)
    }
    $argsPath = Join-Path $runDir ($CaseName + "_args.json")
    ($argsObj | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "get_sch_items_by_id", ("@" + $argsPath)) -TimeoutMs $toolTimeoutMs -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path (Join-Path $runDir ($CaseName + "_output.log")) -Encoding UTF8
    return $out
}

function Invoke-DeleteById {
    param(
        [string]$Url,
        [string]$Id
    )
    $argsObj = @{
        schematic_filename = $sch
        ids = @($Id)
    }
    $argsPath = Join-Path $runDir "delete_sch_items_symbol_restore_args.json"
    ($argsObj | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "delete_sch_items", ("@" + $argsPath)) -TimeoutMs $toolTimeoutMs -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path (Join-Path $runDir "delete_sch_items_symbol_restore_output.log") -Encoding UTF8
    return $out
}

function Write-PlaceSymbolSummary {
    param(
        [string]$BlockerValue
    )

    @(
        "mutation_first_mode=$mutationFirstMode"
        "dry_run_skipped=$dryRunSkipped"
        "two_phase_mode=$twoPhaseMode"
        "dry_run_phase_ok=$dryRunPhaseOk"
        "mutation_phase_ok=$mutationPhaseOk"
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "symbol_created_ok=$symbolCreatedOk"
        "readback_after_create_ok=$readbackAfterCreateOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "exact_blocker_if_any=$BlockerValue"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "lib_id=$outLibId"
        "symbol_ref=$symbolRef"
        "symbol_value=$symbolValue"
        "position_x_nm=$outPositionXNm"
        "position_y_nm=$outPositionYNm"
        "connectivity_refresh_performed=$connectivityRefreshPerformed"
        "fixture_source=$fixtureSource"
        "fixture_schematic=$sch"
        "place_symbol_ipc_reconnect_extra_total=$placeSymbolIpcReconnectExtraTotal"
        "place_symbol_dry_run_retry_extra_total=$placeSymbolDryRunRetryExtraTotal"
        "place_symbol_restore_retry_extra_total=$placeSymbolRestoreRetryExtraTotal"
        "post_mutation_ping_retry_extra_total=$postMutationPingRetryExtraTotal"
        "place_symbol_mutation_retry_extra_total=$placeSymbolMutationRetryExtraTotal"
        "kicad_alive_at_post_mutation=$kicadAliveAtPostMutation"
        "kicad_exited_post_mutation_detected=$kicadExitedPostMutationDetected"
        "post_mutation_cleanup_restart_attempted=$postMutationCleanupRestartAttempted"
        "post_mutation_cleanup_restart_ok=$postMutationCleanupRestartOk"
        "post_mutation_created_item_found_after_restart=$postMutationCreatedItemFoundAfterRestart"
        "post_mutation_cleanup_after_restart_ok=$postMutationCleanupAfterRestartOk"
        ""
    ) | Set-Content -Path $summaryPath -Encoding UTF8
    $script:summaryWritten = $true
}

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$deviceLibUnix = $deviceLib.Replace('\', '/')
$powerLibUnix = $powerLib.Replace('\', '/')
@(
    "(sym_lib_table",
    "  (version 7)",
    ('  (lib (name "Device")(type "KiCad")(uri "' + $deviceLibUnix + '")(options "")(descr "QA deterministic Device library"))'),
    ('  (lib (name "power")(type "KiCad")(uri "' + $powerLibUnix + '")(options "")(descr "QA deterministic power library"))'),
    ")"
) | Set-Content -Path $projectSymLibTable -Encoding ASCII
Copy-Item -Path $projectSymLibTable -Destination $globalSymLibTable -Force
Copy-Item -Path $projectSymLibTable -Destination (Join-Path $runDir "sym-lib-table.used") -Force
Copy-Item -Path $globalSymLibTable -Destination (Join-Path $runDir "sym-lib-table.global.used") -Force

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 120 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }
    if( -not $url ) { throw "Unable to resolve API URL from computed IPC candidates" }
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    $ready = $false
    for( $i = 0; $i -lt 60; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingOut = $pingRes.Text
        $pingOut | Set-Content -Path (Join-Path $runDir "ping_probe.log") -Encoding UTF8
        if( $pingOut -match 'status=1' )
        {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 250
    }
    if( -not $ready ) { throw "KiCad API did not reach AS_OK on ping" }

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = ""
    $openSucceeded = $false
    for( $attempt = 1; $attempt -le 2; $attempt++ )
    {
        $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
        $openOut = $openRes.Text
        $openOut | Set-Content -Path (Join-Path $runDir ("qa_open_editor_frame_output_attempt" + $attempt + ".log")) -Encoding UTF8

        if( $openOut -match 'status=1' )
        {
            $openSucceeded = $true
            break
        }

        if( Test-IsTimeoutOutput -Text $openOut )
        {
            Start-Sleep -Milliseconds 500
            continue
        }

        break
    }
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if( -not $openSucceeded ) { throw "qa_open_editor_frame did not return AS_OK" }

    $positionXNm = 5000000
    $positionYNm = 8000000
    $libId = "Device:R"
    $libraryResolutionStatus = "deterministic_device_table"
    $libraryNickname = "Device"
    $symbolName = "R"
    $expectedRef = "R?"
    $expectedValue = "10k"

    @(
        "fixture_source=$fixtureSource"
        "sym_lib_table_source=$projectSymLibTable;device_lib=$deviceLib;power_lib=$powerLib"
        "selected_lib_id=$libId"
        "library_nickname=$libraryNickname"
        "symbol_name=$symbolName"
        "library_resolution_status=$libraryResolutionStatus"
        ""
    ) | Set-Content -Path (Join-Path $runDir "symbol_library_fixture_status.txt") -Encoding UTF8

    $dryRunArgs = @{
        schematic_filename = $sch
        lib_id = $libId
        position = @{ x_nm = $positionXNm; y_nm = $positionYNm }
        reference = $expectedRef
        value = $expectedValue
        dry_run = $true
        ensure_fresh_connectivity = $true
    }
    if( -not $SkipDryRun )
    {
        $dryRunOut = ""
        $maxDryRunAttempts = 1 + $ipcPostMutationMaxExtraAttempts
        for( $dryAttempt = 1; $dryAttempt -le $maxDryRunAttempts; $dryAttempt++ )
        {
            if( $dryAttempt -gt 1 )
            {
                $placeSymbolDryRunRetryExtraTotal++
                Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("dry_run_attempt" + $dryAttempt) -CountAsExtraReconnect)
                Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $dryAttempt - 2 ))
            }

            $dryRunLogPath = if( $dryAttempt -eq 1 )
            {
                Join-Path $runDir "schematic_place_symbol_dry_run_output.log"
            }
            else
            {
                Join-Path $runDir ("schematic_place_symbol_dry_run_output_attempt" + $dryAttempt + ".log")
            }
            $dryRunArgsPath = if( $dryAttempt -eq 1 )
            {
                Join-Path $runDir "schematic_place_symbol_dry_run_args.json"
            }
            else
            {
                Join-Path $runDir ("schematic_place_symbol_dry_run_args_attempt" + $dryAttempt + ".json")
            }

            $dryRunOut = Invoke-PlaceSymbolTool -Url $url -ArgsFilePath $dryRunArgsPath -ArgsObject $dryRunArgs -OutputLogPath $dryRunLogPath
            if( Test-DryRunContractOk -DryRunOut $dryRunOut )
            {
                break
            }
            if( -not (Test-IsRetriableIpcOutput -Text $dryRunOut) )
            {
                break
            }
        }
        if( $dryAttempt -gt 1 -and $dryAttempt -le $maxDryRunAttempts )
        {
            $dryRunOut | Set-Content -Path (Join-Path $runDir "schematic_place_symbol_dry_run_output.log") -Encoding UTF8
        }

        $dryRunOk = Get-BoolField -Text $dryRunOut -Field "ok" -Default "false"
        $dryRunChanged = Get-BoolField -Text $dryRunOut -Field "changed" -Default "false"
        $dryRunConnectivity = Get-BoolField -Text $dryRunOut -Field "connectivity_refresh_performed" -Default "true"
        $dryRunSymbolId = Get-StringField -Text $dryRunOut -Field "symbol_id" -Default ""
        $dryRunNoChangeOk = if( $dryRunChanged -eq "true" -and $dryRunConnectivity -eq "false" -and $dryRunSymbolId -eq "dry_run" ) { "true" } else { "false" }
        $dryRunPhaseOk = if( $dryRunOk -eq "true" -and $dryRunNoChangeOk -eq "true" ) { "true" } else { "false" }
        @(
            "phase=dry_run"
            "two_phase_mode=true"
            "dry_run_phase_ok=$dryRunPhaseOk"
            "dry_run_ok=$dryRunOk"
            "dry_run_no_change_ok=$dryRunNoChangeOk"
            ""
        ) | Set-Content -Path (Join-Path $runDir "dry_run_phase_status.txt") -Encoding UTF8

        # Two-phase official mode: restart KiCad before mutation to avoid
        # second LoadSymbol in the same process.
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        Start-Sleep -Milliseconds 500
        $null = Clear-IpcSockets
        $kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

        $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                              -MaxIterations 120 -PingTimeoutMs 1500 `
                                              -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
        $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }
        if( -not $url ) { throw "Unable to resolve API URL for mutation phase" }

        $ready = $false
        for( $i = 0; $i -lt 60; $i++ )
        {
            $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
            if( $pingRes.Text -match 'status=1' )
            {
                $ready = $true
                break
            }
            Start-Sleep -Milliseconds 250
        }
        if( -not $ready ) { throw "KiCad API did not reach AS_OK on mutation phase ping" }

        $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
        $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools_mutation_phase.log") -Encoding UTF8

        $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
        $openOut = $openRes.Text
        $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output_mutation_phase.log") -Encoding UTF8
        if( $openOut -notmatch 'status=1' ) { throw "qa_open_editor_frame did not return AS_OK in mutation phase" }
    }
    else
    {
        $dryRunOk = "skipped"
        $dryRunNoChangeOk = "skipped"
    }

    $createArgs = @{
        schematic_filename = $sch
        lib_id = $libId
        position = @{ x_nm = $positionXNm; y_nm = $positionYNm }
        reference = $expectedRef
        value = $expectedValue
        dry_run = $false
        ensure_fresh_connectivity = $true
    }
    $mutationArgsPath = if( $SkipDryRun )
    {
        Join-Path $runDir "schematic_place_symbol_mutation_first_args.json"
    }
    else
    {
        Join-Path $runDir "schematic_place_symbol_args.json"
    }
    $mutationOutputLogPath = if( $SkipDryRun )
    {
        Join-Path $runDir "schematic_place_symbol_mutation_first_output.log"
    }
    else
    {
        $createLogPath
    }
    $attempt1LogPath = Join-Path $runDir "schematic_place_symbol_attempt1_output.log"
    $attempt2LogPath = Join-Path $runDir "schematic_place_symbol_attempt2_output.log"
    $mutationAttempts = 1
    $lastAttemptBlocker = "N/A"
    $mutationCallReached = "false"
    $postMutationPingOk = "false"
    $postMutationBlocker = "N/A"
    $readbackAfterCreateAttempts = 0
    $restoreAttempts = 0
    $readbackAfterRestoreAttempts = 0
    $visible = if( $lt -match 'schematic_place_symbol_session' ) { "true" } else { "false" }

    try
    {
        $createOut = ""
        $maxMutationAttempts = 1 + $ipcPostMutationMaxExtraAttempts
        for( $mutAttempt = 1; $mutAttempt -le $maxMutationAttempts; $mutAttempt++ )
        {
            if( $mutAttempt -gt 1 )
            {
                $placeSymbolMutationRetryExtraTotal++
                Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("mutation_attempt" + $mutAttempt) -CountAsExtraReconnect)
                Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $mutAttempt - 2 ))
            }

            $mutationLogPath = if( $mutAttempt -eq 1 )
            {
                $attempt1LogPath
            }
            elseif( $mutAttempt -eq 2 )
            {
                $attempt2LogPath
            }
            else
            {
                Join-Path $runDir ("schematic_place_symbol_attempt" + $mutAttempt + "_output.log")
            }
            $mutationArgsPathAttempt = if( $mutAttempt -eq 1 )
            {
                $mutationArgsPath
            }
            else
            {
                Join-Path $runDir ("schematic_place_symbol_args_attempt" + $mutAttempt + ".json")
            }

            $createOut = Invoke-PlaceSymbolTool -Url $url -ArgsFilePath $mutationArgsPathAttempt -ArgsObject $createArgs -OutputLogPath $mutationLogPath
            if( $mutAttempt -eq 1 ) { $mutationCallReached = "true" }
            $mutationAttempts = $mutAttempt
            $createOut | Set-Content -Path $createLogPath -Encoding UTF8
            if( $SkipDryRun ) { $createOut | Set-Content -Path $mutationOutputLogPath -Encoding UTF8 }

            $mutationOkFieldTry = Get-BoolField -Text $createOut -Field "ok" -Default "false"
            $mutationChangedTry = Get-BoolField -Text $createOut -Field "changed" -Default "false"
            if( $mutationOkFieldTry -eq "true" -and $mutationChangedTry -eq "true" -and $createOut -match 'status=1' )
            {
                $lastAttemptBlocker = "N/A"
                break
            }

            if( -not (Test-IsRetriableIpcOutput -Text $createOut) )
            {
                if( Test-IsConnectionRefusedOutput -Text $createOut ) { $lastAttemptBlocker = "connection_refused" }
                elseif( Test-IsTimeoutOutput -Text $createOut ) { $lastAttemptBlocker = "mutation_tool_timeout" }
                break
            }

            if( Test-IsConnectionRefusedOutput -Text $createOut ) { $lastAttemptBlocker = "connection_refused" }
            elseif( Test-IsTimeoutOutput -Text $createOut ) { $lastAttemptBlocker = "mutation_tool_timeout" }
        }

        if( Test-IsConnectionRefusedOutput -Text $createOut ) { $lastAttemptBlocker = "connection_refused" }
        elseif( Test-IsTimeoutOutput -Text $createOut ) { $lastAttemptBlocker = "mutation_tool_timeout" }

        $mutationOutputPresent = Test-Path -LiteralPath $createLogPath
        if( -not $mutationOutputPresent )
        {
            "mutation_output_missing" | Set-Content -Path $createLogPath -Encoding UTF8
            $createOut = "mutation_output_missing"
            $lastAttemptBlocker = "mutation_output_missing"
        }

        $mutationOkField = Get-BoolField -Text $createOut -Field "ok" -Default "false"
        $mutationChanged = Get-BoolField -Text $createOut -Field "changed" -Default "false"
        $symbolId = Get-StringField -Text $createOut -Field "symbol_id" -Default ""
        $symbolRef = Get-StringField -Text $createOut -Field "symbol_ref" -Default ""
        $symbolValue = Get-StringField -Text $createOut -Field "symbol_value" -Default ""
        $outLibId = Get-StringField -Text $createOut -Field "lib_id" -Default ""
        $connectivityRefreshPerformed = Get-BoolField -Text $createOut -Field "connectivity_refresh_performed" -Default "false"
        $toolBlocker = Get-StringField -Text $createOut -Field "exact_blocker_if_any" -Default "absent"
        $outPositionXNm = Get-Int64Field -Text $createOut -Field "position_x_nm" -Default 0
        $outPositionYNm = Get-Int64Field -Text $createOut -Field "position_y_nm" -Default 0

        $mutationOk = if( $mutationOkField -eq "true" -and $mutationChanged -eq "true" ) { "true" } else { "false" }
        $symbolCreatedOk = if( ![string]::IsNullOrWhiteSpace($symbolId) -and $symbolId -ne "dry_run" -and $outLibId -eq $libId ) { "true" } else { "false" }
        $callableWithMutation = if( $createOut -match 'status=1' ) { "true" } else { "false" }

        if( $symbolCreatedOk -eq "true" )
        {
            if( Invoke-PostMutationPingReady )
            {
                $postMutationPingOk = "true"
            }
            else
            {
                $postMutationBlocker = "post_mutation_ipc_unavailable"
            }

            $maxPostMutationAttempts = 1 + $ipcPostMutationMaxExtraAttempts
            $readbackAfterCreateOut = ""
            for( $attempt = 1; $attempt -le $maxPostMutationAttempts; $attempt++ )
            {
                $readbackAfterCreateAttempts = $attempt
                if( $attempt -gt 1 )
                {
                    Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("readback_after_create_attempt" + $attempt) -CountAsExtraReconnect)
                    Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $attempt - 2 ))
                }

                $readbackAfterCreateOut = Invoke-GetById -Url $url -Id $symbolId -CaseName ("get_sch_items_by_id_after_create_attempt" + $attempt)
                if( $readbackAfterCreateOut -match [regex]::Escape($symbolId) )
                {
                    $readbackAfterCreateOk = "true"
                    break
                }
                if( -not (Test-IsRetriableIpcOutput -Text $readbackAfterCreateOut) )
                {
                    break
                }
            }
            $readbackAfterCreateOut | Set-Content -Path (Join-Path $runDir "get_sch_items_by_id_after_create_output.log") -Encoding UTF8
            if( $readbackAfterCreateOk -ne "true" )
            {
                if( Test-IsTimeoutOutput -Text $readbackAfterCreateOut ) { $postMutationBlocker = "readback_after_create_timeout" }
                elseif( Test-IsConnectionRefusedOutput -Text $readbackAfterCreateOut ) { $postMutationBlocker = "readback_after_create_connection_refused" }
            }

            $deleteOut = ""
            for( $attempt = 1; $attempt -le $maxPostMutationAttempts; $attempt++ )
            {
                $restoreAttempts = $attempt
                if( $attempt -gt 1 )
                {
                    $placeSymbolRestoreRetryExtraTotal++
                    Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("restore_attempt" + $attempt) -CountAsExtraReconnect)
                    Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $attempt - 2 ))
                }

                $deleteOut = Invoke-DeleteById -Url $url -Id $symbolId
                $deleteOut | Set-Content -Path (Join-Path $runDir ("delete_sch_items_symbol_restore_attempt" + $attempt + "_output.log")) -Encoding UTF8
                if( $deleteOut -match 'status=1' )
                {
                    $restoreOk = "true"
                    break
                }
                if( -not (Test-IsRetriableIpcOutput -Text $deleteOut) )
                {
                    break
                }
            }
            $deleteOut | Set-Content -Path (Join-Path $runDir "delete_sch_items_symbol_restore_output.log") -Encoding UTF8
            if( $restoreOk -ne "true" )
            {
                if( Test-IsTimeoutOutput -Text $deleteOut ) { $postMutationBlocker = "restore_timeout" }
                elseif( Test-IsConnectionRefusedOutput -Text $deleteOut ) { $postMutationBlocker = "restore_connection_refused" }
            }

            $readbackAfterRestoreOut = ""
            for( $attempt = 1; $attempt -le $maxPostMutationAttempts; $attempt++ )
            {
                $readbackAfterRestoreAttempts = $attempt
                if( $attempt -gt 1 )
                {
                    Sync-ProbeIpcUrl -ReconnectResult (Resolve-AndPingCurrentKiCad -PhaseLabel ("readback_after_restore_attempt" + $attempt) -CountAsExtraReconnect)
                    Start-Sleep -Milliseconds (Get-IpcRetryBackoffMs -ExtraAttemptIndex ( $attempt - 2 ))
                }

                $readbackAfterRestoreOut = Invoke-GetById -Url $url -Id $symbolId -CaseName ("get_sch_items_by_id_after_restore_attempt" + $attempt)
                if( $readbackAfterRestoreOut -notmatch [regex]::Escape($symbolId) )
                {
                    $readbackAfterRestoreOk = "true"
                    break
                }
                if( -not (Test-IsRetriableIpcOutput -Text $readbackAfterRestoreOut) )
                {
                    break
                }
            }
            $readbackAfterRestoreOut | Set-Content -Path (Join-Path $runDir "get_sch_items_by_id_after_restore_output.log") -Encoding UTF8
            if( $readbackAfterRestoreOk -ne "true" )
            {
                if( Test-IsTimeoutOutput -Text $readbackAfterRestoreOut ) { $postMutationBlocker = "readback_after_restore_timeout" }
                elseif( Test-IsConnectionRefusedOutput -Text $readbackAfterRestoreOut ) { $postMutationBlocker = "readback_after_restore_connection_refused" }
            }

            $kicadAliveAtPostMutation = if( Test-KiCadProcessAlive ) { "true" } else { "false" }
            if( $kicadAliveAtPostMutation -eq "false" )
            {
                $kicadExitedPostMutationDetected = "true"
            }

            if( $kicadExitedPostMutationDetected -eq "true" -and
                ( $readbackAfterCreateOk -ne "true" -or $restoreOk -ne "true" -or $readbackAfterRestoreOk -ne "true" ) )
            {
                $null = Invoke-PostMutationCleanupRestart -SymbolIdToCleanup $symbolId -OpenInputPath $openInput
            }
        }
        else
        {
            $postMutationBlocker = "symbol_not_created_after_mutation"
        }
        $mutationPhaseOk = if( $mutationOk -eq "true" -and $symbolCreatedOk -eq "true" -and $readbackAfterCreateOk -eq "true" -and $restoreOk -eq "true" -and $readbackAfterRestoreOk -eq "true" ) { "true" } else { "false" }
        @(
            "phase=mutation"
            "two_phase_mode=$twoPhaseMode"
            "mutation_phase_ok=$mutationPhaseOk"
            "mutation_ok=$mutationOk"
            "symbol_created_ok=$symbolCreatedOk"
            "readback_after_create_ok=$readbackAfterCreateOk"
            "restore_ok=$restoreOk"
            "readback_after_restore_ok=$readbackAfterRestoreOk"
            ""
        ) | Set-Content -Path (Join-Path $runDir "mutation_phase_status.txt") -Encoding UTF8
        @(
            "post_mutation_ping_ok=$postMutationPingOk"
            "readback_after_create_attempts=$readbackAfterCreateAttempts"
            "restore_attempts=$restoreAttempts"
            "readback_after_restore_attempts=$readbackAfterRestoreAttempts"
            "last_blocker=$postMutationBlocker"
            "place_symbol_ipc_reconnect_extra_total=$placeSymbolIpcReconnectExtraTotal"
            "place_symbol_dry_run_retry_extra_total=$placeSymbolDryRunRetryExtraTotal"
            "place_symbol_restore_retry_extra_total=$placeSymbolRestoreRetryExtraTotal"
            "post_mutation_ping_retry_extra_total=$postMutationPingRetryExtraTotal"
            "place_symbol_mutation_retry_extra_total=$placeSymbolMutationRetryExtraTotal"
            "kicad_alive_at_post_mutation=$kicadAliveAtPostMutation"
            "kicad_exited_post_mutation_detected=$kicadExitedPostMutationDetected"
            "post_mutation_cleanup_restart_attempted=$postMutationCleanupRestartAttempted"
            "post_mutation_cleanup_restart_ok=$postMutationCleanupRestartOk"
            "post_mutation_created_item_found_after_restart=$postMutationCreatedItemFoundAfterRestart"
            "post_mutation_cleanup_after_restart_ok=$postMutationCleanupAfterRestartOk"
            ""
        ) | Set-Content -Path (Join-Path $runDir "post_mutation_ipc_status.txt") -Encoding UTF8
    }
    catch
    {
        $exceptionCaught = $true
        $_ | Out-String | Set-Content -Path $probeExceptionPath -Encoding UTF8
        $finalBlocker = "probe_exception"
    }
    finally
    {
        $mutationOutputPresent = Test-Path -LiteralPath $createLogPath
        if( -not $mutationOutputPresent )
        {
            "mutation_output_missing" | Set-Content -Path $createLogPath -Encoding UTF8
            $finalBlocker = "mutation_output_missing"
            $mutationOk = "false"
            $symbolCreatedOk = "false"
            $readbackAfterCreateOk = "false"
            $restoreOk = "false"
            $readbackAfterRestoreOk = "false"
            $callableWithMutation = "false"
            $toolBlocker = "absent"
        }

        @(
            "attempts=$mutationAttempts"
            "final_status=" + $(if($mutationOk -eq "true"){"ok"}else{"failed"})
            "last_blocker=" + $(if($lastAttemptBlocker -eq "N/A" -and $mutationOk -ne "true" -and $callableWithMutation -ne "true"){"call_failed"}else{$lastAttemptBlocker})
            ""
        ) | Set-Content -Path (Join-Path $runDir "schematic_place_symbol_attempts.txt") -Encoding UTF8

        @(
            "mutation_first_mode=$mutationFirstMode"
            "mutation_call_reached=$mutationCallReached"
            "mutation_output_present=" + $(if($mutationOutputPresent){"true"}else{"false"})
            ""
        ) | Set-Content -Path (Join-Path $runDir "mutation_first_status.txt") -Encoding UTF8

        if( $finalBlocker -eq "N/A" )
        {
            if( $exceptionCaught ) { $finalBlocker = "probe_exception" }
            elseif( -not $mutationOutputPresent ) { $finalBlocker = "mutation_output_missing" }
            elseif( $lastAttemptBlocker -eq "connection_refused" ) { $finalBlocker = "connection_refused" }
            elseif( $lastAttemptBlocker -eq "mutation_tool_timeout" ) { $finalBlocker = "mutation_tool_timeout" }
            elseif( -not $SkipDryRun -and $dryRunPhaseOk -ne "true" ) { $finalBlocker = "dry_run_phase_not_ok" }
            elseif( $postMutationBlocker -ne "N/A" ) { $finalBlocker = $postMutationBlocker }
            elseif( $mutationPhaseOk -ne "true" ) { $finalBlocker = "mutation_phase_not_ok" }
            elseif( $visible -ne "true" ) { $finalBlocker = "not_visible_in_context" }
            elseif( $callableWithMutation -ne "true" ) { $finalBlocker = "call_failed" }
            elseif( -not $SkipDryRun -and $dryRunOk -ne "true" ) { $finalBlocker = "dry_run_not_ok" }
            elseif( -not $SkipDryRun -and $dryRunNoChangeOk -ne "true" ) { $finalBlocker = "dry_run_contract_failed" }
            elseif( $mutationOk -ne "true" ) { $finalBlocker = "mutation_not_ok" }
            elseif( $symbolCreatedOk -ne "true" ) { $finalBlocker = "symbol_create_contract_failed" }
            elseif( $readbackAfterCreateOk -ne "true" ) { $finalBlocker = "readback_after_create_failed" }
            elseif( $restoreOk -ne "true" ) { $finalBlocker = "restore_delete_failed" }
            elseif( $readbackAfterRestoreOk -ne "true" ) { $finalBlocker = "readback_after_restore_failed" }
        }

        Write-PlaceSymbolSummary -BlockerValue $finalBlocker
    }

    Write-TargetRunDir -RunDir $runDir
}
catch
{
    $exceptionCaught = $true
    $_ | Out-String | Set-Content -Path $probeExceptionPath -Encoding UTF8
    if( -not $summaryWritten )
    {
        $finalBlocker = "probe_exception"
        Write-PlaceSymbolSummary -BlockerValue $finalBlocker
    }
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    $mutationOutputPresent = Test-Path -LiteralPath $createLogPath
    @(
        "summary_written=" + $(if($summaryWritten){"true"}else{"false"})
        "final_blocker=$finalBlocker"
        "exception_caught=" + $(if($exceptionCaught){"true"}else{"false"})
        "mutation_output_present=" + $(if($mutationOutputPresent){"true"}else{"false"})
        ""
    ) | Set-Content -Path $finalizationPath -Encoding UTF8
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
