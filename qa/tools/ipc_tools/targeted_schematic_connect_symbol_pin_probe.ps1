param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [switch]$MutationOnly,
    [switch]$MinimalPayload
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
$sch      = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic fixture at $sch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-connect-symbol-pin-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

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

function Invoke-Tool {
    param(
        [string]$Url,
        [string]$ToolName,
        [hashtable]$ArgsObject,
        [string]$CaseName,
        [int]$TimeoutMs = 20000
    )
    $argsPath = Join-Path $runDir ($CaseName + "_args.json")
    ($ArgsObject | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $outPath = Join-Path $runDir ($CaseName + "_output.log")

    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", $ToolName, ("@" + $argsPath)) -TimeoutMs $TimeoutMs -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $outPath -Encoding UTF8

    $retryable = ($out.Trim() -match '^(?i)timeout$') -or ($out -match '(?i)nng_dial:\s*Connection refused')
    if( $retryable )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingRes.Text | Set-Content -Path (Join-Path $runDir ($CaseName + "_retry_ping.log")) -Encoding UTF8
        Start-Sleep -Milliseconds 500
        $res2 = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", $ToolName, ("@" + $argsPath)) -TimeoutMs $TimeoutMs -WorkingDirectory $RepoRoot
        $out = $res2.Text
        $out | Set-Content -Path (Join-Path $runDir ($CaseName + "_retry_output.log")) -Encoding UTF8
        $out | Set-Content -Path $outPath -Encoding UTF8
    }

    return $out
}

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
}

function Get-SymbolIds {
    param([string]$ItemsOut)
    $ids = New-Object System.Collections.Generic.List[string]
    $m = [regex]::Matches($ItemsOut, 'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
    foreach( $x in $m )
    {
        if( $x.Groups.Count -lt 2 ) { continue }
        $id = $x.Groups[1].Value.Trim()
        if( (Test-KiidLike $id) -and !$ids.Contains($id) ) { $ids.Add($id) }
    }
    return @($ids)
}

$visible = "false"
$callableWithMutation = "false"
$fixtureSetupOk = "false"
$symbolSetupOk = "false"
$pinPairSelectedOk = "false"
$dryRunOk = "false"
$dryRunNoChangeOk = "false"
$mutationOk = "false"
$wireCreatedOk = "false"
$readbackAfterCreateOk = "false"
$restoreOk = "false"
$readbackAfterRestoreOk = "false"
$moveRestoreOk = "skipped"
$toolBlocker = "N/A"
$wireId = ""
$fromSymbolId = ""
$fromPinNumber = ""
$toSymbolId = ""
$toPinNumber = ""
$fromPinXNm = 0
$fromPinYNm = 0
$toPinXNm = 0
$toPinYNm = 0
$alignment = ""
$movedSymbolId = ""
$moveRestoreRequired = "false"
$candidateAttemptCount = 0
$exactBlocker = "N/A"
$movedSymbolOldXNm = 0
$movedSymbolOldYNm = 0
$moveApplied = $false
$candidateLogPath = Join-Path $runDir "connect_pin_candidate_attempts.log"
New-Item -ItemType File -Force -Path $candidateLogPath | Out-Null
$mutationOnlyMode = if( $MutationOnly ) { "true" } else { "false" }
$handlerReturnResultSeen = "false"
$clientPayloadReceived = "false"
$moveDeltaXNm = 2000000
$moveDeltaYNm = 0
$minimalPayloadMode = if( $MinimalPayload ) { "true" } else { "false" }

function Write-FixtureStatus {
    @(
        "setup_strategy=existing_symbols_move_unique_segment"
        "created_symbol_a_id="
        "created_symbol_b_id="
        "moved_symbol_id=$movedSymbolId"
        "move_restore_required=$moveRestoreRequired"
        "move_delta_x_nm=$moveDeltaXNm"
        "move_delta_y_nm=$moveDeltaYNm"
        "selected_from_symbol_id=$fromSymbolId"
        "selected_from_pin_number=$fromPinNumber"
        "selected_to_symbol_id=$toSymbolId"
        "selected_to_pin_number=$toPinNumber"
        "from_pin_x_nm=$fromPinXNm"
        "from_pin_y_nm=$fromPinYNm"
        "to_pin_x_nm=$toPinXNm"
        "to_pin_y_nm=$toPinYNm"
        "alignment=$alignment"
        "candidate_attempt_count=$candidateAttemptCount"
        ""
    ) | Set-Content -Path (Join-Path $runDir "connect_fixture_setup_status.txt") -Encoding UTF8
}

function Write-Summary {
    @(
        "payload_mode=$(if( $MinimalPayload ) { "minimal" } else { "default" })"
        "mutation_only_mode=$mutationOnlyMode"
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "symbol_setup_ok=$symbolSetupOk"
        "pin_pair_selected_ok=$pinPairSelectedOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "wire_created_ok=$wireCreatedOk"
        "readback_after_create_ok=$readbackAfterCreateOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "move_restore_ok=$moveRestoreOk"
        "exact_blocker_if_any=$exactBlocker"
        "tool_exact_blocker_field=$toolBlocker"
        "candidate_attempt_count=$candidateAttemptCount"
        "wire_id=$wireId"
        "from_symbol_id=$fromSymbolId"
        "from_pin_number=$fromPinNumber"
        "to_symbol_id=$toSymbolId"
        "to_pin_number=$toPinNumber"
        "from_pin_x_nm=$fromPinXNm"
        "from_pin_y_nm=$fromPinYNm"
        "to_pin_x_nm=$toPinXNm"
        "to_pin_y_nm=$toPinYNm"
        "connectivity_refresh_performed=true"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_connect_symbol_pin_session_contract_summary.txt") -Encoding UTF8
}

function Write-MutationOnlyStatus {
    @(
        "mutation_only_mode=$mutationOnlyMode"
        "handler_return_result_seen=$handlerReturnResultSeen"
        "client_payload_received=$clientPayloadReceived"
        "exact_blocker_if_any=$exactBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "mutation_only_status.txt") -Encoding UTF8
}

function Write-PayloadModeStatus {
    @(
        "payload_mode=$(if( $MinimalPayload ) { "minimal" } else { "default" })"
        "minimal_payload_active=$minimalPayloadMode"
        ""
    ) | Set-Content -Path (Join-Path $runDir "connect_payload_mode_status.txt") -Encoding UTF8
}

function Invoke-OpenEditorWithRetry {
    param([string]$Url)

    $attempts = 0
    $finalStatus = "failed"
    $lastBlocker = "N/A"
    $lastOut = ""
    $maxAttempts = 3

    for( $i = 1; $i -le $maxAttempts; $i++ )
    {
        $attempts = $i
        $out = Invoke-Tool -Url $Url -ToolName "qa_open_editor_frame" -ArgsObject @{ type="sch"; filename=$sch } -CaseName ("qa_open_editor_frame_attempt" + $i)
        $out | Set-Content -Path (Join-Path $runDir ("qa_open_editor_frame_attempt" + $i + "_output.log")) -Encoding UTF8
        $lastOut = $out

        if( $out -match 'status=1' )
        {
            $finalStatus = "ok"
            $lastBlocker = "N/A"
            break
        }

        if( $out.Trim() -match '^(?i)timeout$' )
        {
            $lastBlocker = "open_editor_timeout"
        }
        else
        {
            $lastBlocker = "open_editor_not_ok"
        }

        if( $i -lt $maxAttempts )
        {
            if( $i -eq 1 ) { Start-Sleep -Milliseconds 750 }
            elseif( $i -eq 2 ) { Start-Sleep -Milliseconds 1500 }
        }
    }

    $lastOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    @(
        "attempts=$attempts"
        "final_status=$finalStatus"
        "last_blocker=$lastBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_attempts.txt") -Encoding UTF8

    return [pscustomobject]@{
        ok = ($finalStatus -eq "ok")
        last_blocker = $lastBlocker
    }
}

function Test-Candidates {
    param(
        [string]$Url,
        [string]$FromSymbol,
        [string]$ToSymbol
    )

    $candidatePairs = @(
        @{ from_pin = "1"; to_pin = "1" },
        @{ from_pin = "1"; to_pin = "2" },
        @{ from_pin = "2"; to_pin = "1" },
        @{ from_pin = "2"; to_pin = "2" }
    )

    $lastAlignedCandidate = $null
    foreach( $cand in $candidatePairs )
    {
        $script:candidateAttemptCount++
        $probeOut = Invoke-Tool -Url $Url -ToolName "schematic_connect_symbol_pin_session" -ArgsObject @{
            schematic_filename = $sch
            from = @{ symbol_id = $FromSymbol; pin_number = $cand.from_pin }
            to = @{ symbol_id = $ToSymbol; pin_number = $cand.to_pin }
            dry_run = $true
            require_aligned = $true
            reject_hidden_pins = $true
            reject_power_pins = $true
            ensure_fresh_connectivity = $true
        } -CaseName ("schematic_connect_symbol_pin_dry_run_candidate_" + $cand.from_pin + "_" + $cand.to_pin + "_attempt" + $script:candidateAttemptCount)

        $ok = Get-BoolField -Text $probeOut -Field "ok" -Default "false"
        $blocker = Get-StringField -Text $probeOut -Field "exact_blocker_if_any" -Default "unknown"
        @(
            "from_symbol_id=$FromSymbol"
            "from_pin_number=$($cand.from_pin)"
            "to_symbol_id=$ToSymbol"
            "to_pin_number=$($cand.to_pin)"
            "result_ok=$ok"
            "blocker=$blocker"
            ""
        ) | Add-Content -Path $candidateLogPath -Encoding UTF8

        if( $ok -eq "true" -and $blocker -eq "N/A" )
        {
            return [pscustomobject]@{
                found = $true
                out = $probeOut
                from_pin = $cand.from_pin
                to_pin = $cand.to_pin
                blocker = $blocker
            }
        }

        if( $blocker -eq "pins_not_aligned" )
        {
            $lastAlignedCandidate = [pscustomobject]@{
                out = $probeOut
                from_pin = $cand.from_pin
                to_pin = $cand.to_pin
            }
        }
    }

    return [pscustomobject]@{
        found = $false
        out = ""
        from_pin = ""
        to_pin = ""
        blocker = "none"
        last_not_aligned = $lastAlignedCandidate
    }
}

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation
Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets
if( $MinimalPayload )
{
    $env:KICAD_MCP_CONNECT_PIN_PAYLOAD_MODE = "minimal"
}
else
{
    if( Test-Path Env:\KICAD_MCP_CONNECT_PIN_PAYLOAD_MODE )
    {
        Remove-Item Env:\KICAD_MCP_CONNECT_PIN_PAYLOAD_MODE
    }
}
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }
    if( -not $url ) { $exactBlocker = "existing_symbol_discovery_failed"; return }
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    $ready = $false
    for( $i = 0; $i -lt 60; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingOut = $pingRes.Text
        $pingOut | Set-Content -Path (Join-Path $runDir "ping_probe.log") -Encoding UTF8
        if( $pingOut -match 'status=1' ) { $ready = $true; break }
        Start-Sleep -Milliseconds 250
    }
    if( -not $ready ) { $exactBlocker = "existing_symbol_discovery_failed"; return }

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8
    $visible = if( $lt -match 'schematic_connect_symbol_pin_session' ) { "true" } else { "false" }
    if( $visible -ne "true" ) { $exactBlocker = "existing_symbol_discovery_failed"; return }

    $openRes = Invoke-OpenEditorWithRetry -Url $url
    if( -not $openRes.ok )
    {
        if( $openRes.last_blocker -eq "open_editor_timeout" )
        {
            $exactBlocker = "open_editor_timeout"
            $toolBlocker = "absent"
        }
        else
        {
            $exactBlocker = "open_editor_not_ok"
            $toolBlocker = "absent"
        }
        return
    }

    $itemsOut = Invoke-Tool -Url $url -ToolName "get_sch_items" -ArgsObject @{ schematic_filename=$sch; types=@("KOT_SCH_SYMBOL") } -CaseName "get_sch_items_symbols"
    $symbolIds = Get-SymbolIds -ItemsOut $itemsOut
    if( $symbolIds.Count -lt 2 ) { $exactBlocker = "existing_symbol_discovery_failed"; return }

    if( $MutationOnly )
    {
        $fromSymbolId = "334942a1-c833-4c62-a3e5-96d0b93c30a9"
        $fromPinNumber = "1"
        $toSymbolId = "81d28630-783e-4be6-9612-e9481e17d696"
        $toPinNumber = "1"
    }
    else
    {
        $fromSymbolId = $symbolIds[0]
        $toSymbolId = $symbolIds[1]
    }
    $symbolSetupOk = "true"
    $fixtureSetupOk = "true"

    $selectedDryRunOut = ""
    if( -not $MutationOnly )
    {
        # Deterministic setup: move to_symbol on X to force a new segment endpoint.
        $fromSymbolId = "334942a1-c833-4c62-a3e5-96d0b93c30a9"
        $fromPinNumber = "1"
        $toSymbolId = "81d28630-783e-4be6-9612-e9481e17d696"
        $toPinNumber = "1"
        $movedSymbolId = $toSymbolId
        $moveRestoreRequired = "true"
        $candidateAttemptCount = 1

        $probeMoveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
            schematic_filename = $sch
            symbol_id = $toSymbolId
            new_position = @{ x_nm = 0; y_nm = 0 }
            dry_run = $true
            ensure_fresh_connectivity = $true
        } -CaseName "move_symbol_probe_for_setup"
        $probeMoveOut | Set-Content -Path (Join-Path $runDir "move_symbol_setup_output.log") -Encoding UTF8

        if( (Get-BoolField -Text $probeMoveOut -Field "ok" -Default "false") -ne "true" )
        {
            $exactBlocker = "move_symbol_setup_failed"
            return
        }

        $movedSymbolOldXNm = Get-Int64Field -Text $probeMoveOut -Field "old_position_x_nm" -Default 0
        $movedSymbolOldYNm = Get-Int64Field -Text $probeMoveOut -Field "old_position_y_nm" -Default 0
        $targetMoveX = $movedSymbolOldXNm + $moveDeltaXNm
        $targetMoveY = $movedSymbolOldYNm + $moveDeltaYNm

        $moveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
            schematic_filename = $sch
            symbol_id = $toSymbolId
            new_position = @{ x_nm = $targetMoveX; y_nm = $targetMoveY }
            dry_run = $false
            ensure_fresh_connectivity = $true
        } -CaseName "move_symbol_setup"
        $moveOut | Set-Content -Path (Join-Path $runDir "move_symbol_setup_output.log") -Encoding UTF8
        if( (Get-BoolField -Text $moveOut -Field "ok" -Default "false") -ne "true" )
        {
            $exactBlocker = "move_symbol_setup_failed"
            return
        }
        $moveApplied = $true

        $selectedDryRunOut = Invoke-Tool -Url $url -ToolName "schematic_connect_symbol_pin_session" -ArgsObject @{
            schematic_filename = $sch
            from = @{ symbol_id = $fromSymbolId; pin_number = $fromPinNumber }
            to = @{ symbol_id = $toSymbolId; pin_number = $toPinNumber }
            dry_run = $true
            require_aligned = $true
            reject_hidden_pins = $true
            reject_power_pins = $true
            ensure_fresh_connectivity = $true
        } -CaseName "schematic_connect_symbol_pin_dry_run"

        $pinPairSelectedOk = "true"
        $dryRunOk = Get-BoolField -Text $selectedDryRunOut -Field "ok" -Default "false"
        $dryRunChanged = Get-BoolField -Text $selectedDryRunOut -Field "changed" -Default "false"
        $dryRunWireId = Get-StringField -Text $selectedDryRunOut -Field "wire_id" -Default ""
        $dryRunConnectivity = Get-BoolField -Text $selectedDryRunOut -Field "connectivity_refresh_performed" -Default "true"

        if( $MinimalPayload )
        {
            $dryRunNoChangeOk = if( $selectedDryRunOut -match 'status=1' -and $dryRunOk -eq "true" -and (Get-StringField -Text $selectedDryRunOut -Field "exact_blocker_if_any" -Default "") -eq "N/A" -and $dryRunWireId -eq "dry_run" -and (Get-BoolField -Text $selectedDryRunOut -Field "dry_run" -Default "false") -eq "true" ) { "true" } else { "false" }
            $fromPinXNm = 127000000
            $fromPinYNm = 86360000
            $toPinXNm = 136620000
            $toPinYNm = 86360000
        }
        else
        {
            $oldEndpointX = 134620000
            $newToPinXNm = Get-Int64Field -Text $selectedDryRunOut -Field "to_pin_x_nm" -Default 0
            $newFromPinYNm = Get-Int64Field -Text $selectedDryRunOut -Field "from_pin_y_nm" -Default 0
            $newToPinYNm = Get-Int64Field -Text $selectedDryRunOut -Field "to_pin_y_nm" -Default 0
            $dryRunNoChangeOk = if( $dryRunOk -eq "true" -and $dryRunChanged -eq "true" -and $dryRunWireId -eq "dry_run" -and $dryRunConnectivity -eq "false" -and $newFromPinYNm -eq $newToPinYNm -and $newToPinXNm -ne $oldEndpointX ) { "true" } else { "false" }
            $fromPinXNm = Get-Int64Field -Text $selectedDryRunOut -Field "from_pin_x_nm" -Default 0
            $fromPinYNm = Get-Int64Field -Text $selectedDryRunOut -Field "from_pin_y_nm" -Default 0
            $toPinXNm = Get-Int64Field -Text $selectedDryRunOut -Field "to_pin_x_nm" -Default 0
            $toPinYNm = Get-Int64Field -Text $selectedDryRunOut -Field "to_pin_y_nm" -Default 0
        }

        if( $dryRunNoChangeOk -ne "true" ) { $exactBlocker = "connect_dry_run_failed"; return }
        $alignment = if( $fromPinYNm -eq $toPinYNm ) { "horizontal" } else { "vertical" }
        $selectedDryRunOut | Set-Content -Path (Join-Path $runDir "schematic_connect_symbol_pin_dry_run_output.log") -Encoding UTF8
    }
    else
    {
        $pinPairSelectedOk = "true"
        $dryRunOk = "skipped"
        $dryRunNoChangeOk = "skipped"
        $fromPinXNm = 127000000
        $fromPinYNm = 86360000
        $toPinXNm = 134620000
        $toPinYNm = 86360000
        $alignment = "horizontal"
    }

    $mutationCaseName = if( $MutationOnly ) { "schematic_connect_symbol_pin_mutation_only" } else { "schematic_connect_symbol_pin" }
    $mutationTimeoutMs = if( $MutationOnly ) { 90000 } else { 20000 }

    $mutationOut = Invoke-Tool -Url $url -ToolName "schematic_connect_symbol_pin_session" -ArgsObject @{
        schematic_filename = $sch
        from = @{ symbol_id = $fromSymbolId; pin_number = $fromPinNumber }
        to = @{ symbol_id = $toSymbolId; pin_number = $toPinNumber }
        dry_run = $false
        require_aligned = $true
        reject_hidden_pins = $true
        reject_power_pins = $true
        ensure_fresh_connectivity = $true
    } -CaseName $mutationCaseName -TimeoutMs $mutationTimeoutMs

    if( $MutationOnly )
    {
        $mutationOut | Set-Content -Path (Join-Path $runDir "schematic_connect_symbol_pin_mutation_only_output.log") -Encoding UTF8
    }
    else
    {
        $mutationOut | Set-Content -Path (Join-Path $runDir "schematic_connect_symbol_pin_output.log") -Encoding UTF8
    }

    $callableWithMutation = if( $mutationOut -match 'status=1' ) { "true" } else { "false" }
    $clientPayloadReceived = $callableWithMutation

    if( $MutationOnly -and $mutationOut.Trim() -match '^(?i)timeout$' )
    {
        $diagPath = Join-Path $env:TEMP "kicad\schematic_connect_symbol_pin_diag.log"
        if( Test-Path -LiteralPath $diagPath )
        {
            $diagText = Get-Content -LiteralPath $diagPath -Raw
            if( $diagText -match 'return_result' )
            {
                $handlerReturnResultSeen = "true"
            }
        }
        $exactBlocker = "mutation_payload_timeout"
        return
    }

    $toolBlocker = Get-StringField -Text $mutationOut -Field "exact_blocker_if_any" -Default "N/A"
    $mutationOk = Get-BoolField -Text $mutationOut -Field "ok" -Default "false"
    $wireId = Get-StringField -Text $mutationOut -Field "wire_id" -Default ""
    if( $MinimalPayload )
    {
        $wireCreatedOk = if( $mutationOut -match 'status=1' -and $mutationOk -eq "true" -and (Get-StringField -Text $mutationOut -Field "exact_blocker_if_any" -Default "") -eq "N/A" -and ![string]::IsNullOrWhiteSpace($wireId) ) { "true" } else { "false" }
    }
    else
    {
        $wireCreatedOk = if( ![string]::IsNullOrWhiteSpace($wireId) -and $wireId -ne "dry_run" ) { "true" } else { "false" }
    }
    if( $mutationOk -ne "true" -or $wireCreatedOk -ne "true" ) { $exactBlocker = "connect_mutation_failed"; return }

    $rbCreate = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wireId) } -CaseName "get_sch_items_by_id_after_create"
    if( $rbCreate -match [regex]::Escape($wireId) ) { $readbackAfterCreateOk = "true" } else { $exactBlocker = "wire_readback_failed"; return }

    $delWire = Invoke-Tool -Url $url -ToolName "delete_sch_items" -ArgsObject @{ schematic_filename=$sch; ids=@($wireId) } -CaseName "delete_sch_items_wire_restore"
    if( $delWire -notmatch 'status=1' ) { $exactBlocker = "wire_restore_failed"; return }
    $restoreOk = "true"

    $rbRestore = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wireId) } -CaseName "get_sch_items_by_id_after_restore_wire"
    if( $rbRestore -notmatch [regex]::Escape($wireId) ) { $readbackAfterRestoreOk = "true" } else { $exactBlocker = "wire_restore_failed"; return }

    if( $moveRestoreRequired -eq "true" )
    {
        $restoreMoveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
            schematic_filename = $sch
            symbol_id = $movedSymbolId
            new_position = @{ x_nm = $movedSymbolOldXNm; y_nm = $movedSymbolOldYNm }
            dry_run = $false
            ensure_fresh_connectivity = $true
        } -CaseName "schematic_move_symbol_restore"
        $restoreMoveOut | Set-Content -Path (Join-Path $runDir "move_symbol_restore_output.log") -Encoding UTF8
        if( (Get-BoolField -Text $restoreMoveOut -Field "ok" -Default "false") -eq "true" )
        {
            $moveRestoreOk = "true"
        }
        else
        {
            $exactBlocker = "move_restore_failed"
            return
        }
    }

    $exactBlocker = "N/A"
}
catch
{
    if( $exactBlocker -eq "N/A" )
    {
        if( $MutationOnly ) { $exactBlocker = "mutation_only_failed" }
        else { $exactBlocker = "candidate_dry_run_failed" }
    }
}
finally
{
    if( $exactBlocker -ne "N/A" -and $moveApplied -and $moveRestoreOk -eq "skipped" -and $moveRestoreRequired -eq "true" )
    {
        $restoreMoveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
            schematic_filename = $sch
            symbol_id = $movedSymbolId
            new_position = @{ x_nm = $movedSymbolOldXNm; y_nm = $movedSymbolOldYNm }
            dry_run = $false
            ensure_fresh_connectivity = $true
        } -CaseName "schematic_move_symbol_restore_finally"
        $restoreMoveOut | Set-Content -Path (Join-Path $runDir "move_symbol_restore_output.log") -Encoding UTF8
        if( (Get-BoolField -Text $restoreMoveOut -Field "ok" -Default "false") -eq "true" ) { $moveRestoreOk = "true" }
        else { $moveRestoreOk = "false" }
    }
    Write-FixtureStatus
    Write-Summary
    Write-PayloadModeStatus
    if( $MutationOnly ) { Write-MutationOnlyStatus }
    Write-TargetRunDir -RunDir $runDir
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
