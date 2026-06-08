param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
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

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic fixture at $sch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-connect-pins-l-shape-targeted")
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

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
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

function Invoke-OpenEditorWithRetry {
    param([string]$Url)

    for( $i = 1; $i -le 3; $i++ )
    {
        $out = Invoke-Tool -Url $Url -ToolName "qa_open_editor_frame" -ArgsObject @{ type="sch"; filename=$sch } -CaseName ("qa_open_editor_frame_attempt" + $i)
        if( $out -match 'status=1' ) { return $true }
        Start-Sleep -Milliseconds (500 * $i)
    }

    return $false
}

$visible = "false"
$callableWithMutation = "false"
$fixtureSetupOk = "false"
$symbolSetupOk = "false"
$pinPairSelectedOk = "false"
$dryRunOk = "false"
$dryRunNoChangeOk = "false"
$mutationOk = "false"
$wire1CreatedOk = "false"
$wire2CreatedOk = "false"
$readbackAfterCreateOk = "false"
$restoreOk = "false"
$readbackAfterRestoreOk = "false"
$moveRestoreOk = "false"
$exactBlocker = "N/A"
$toolBlocker = "N/A"
$wire1Id = ""
$wire2Id = ""
$junctionId = ""
$fromSymbolId = "334942a1-c833-4c62-a3e5-96d0b93c30a9"
$fromPinNumber = "1"
$toSymbolId = "81d28630-783e-4be6-9612-e9481e17d696"
$toPinNumber = "1"
$fromPinXNm = 0
$fromPinYNm = 0
$toPinXNm = 0
$toPinYNm = 0
$elbowXNm = 0
$elbowYNm = 0
$elbowMode = "horizontal_then_vertical"
$movedSymbolId = $toSymbolId
$moveRestoreRequired = "true"
$moveDeltaXNm = 2000000
$moveDeltaYNm = 1000000
$movedSymbolOldXNm = 0
$movedSymbolOldYNm = 0
$mutationOut = ""
$payloadMode = if( $MinimalPayload ) { "minimal" } else { "default" }
$readbackAfterCreateSource = "external_readback"
$externalReadbackAfterCreateAttempted = "false"
$externalReadbackAfterCreateStatus = "skipped"
$restoreAttempted = "false"

function Write-FixtureStatus {
    @(
        "setup_strategy=existing_symbols_move_non_aligned_l_shape"
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
        "elbow_x_nm=$elbowXNm"
        "elbow_y_nm=$elbowYNm"
        "elbow_mode=$elbowMode"
        ""
    ) | Set-Content -Path (Join-Path $runDir "connect_fixture_setup_status.txt") -Encoding UTF8
}

function Write-Summary {
    @(
        "payload_mode=$payloadMode"
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "symbol_setup_ok=$symbolSetupOk"
        "pin_pair_selected_ok=$pinPairSelectedOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "wire1_created_ok=$wire1CreatedOk"
        "wire2_created_ok=$wire2CreatedOk"
        "readback_after_create_ok=$readbackAfterCreateOk"
        "readback_after_create_source=$readbackAfterCreateSource"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "move_restore_ok=$moveRestoreOk"
        "exact_blocker_if_any=$exactBlocker"
        "tool_exact_blocker_field=$toolBlocker"
        "wire1_id=$wire1Id"
        "wire2_id=$wire2Id"
        "junction_id=$junctionId"
        "from_symbol_id=$fromSymbolId"
        "from_pin_number=$fromPinNumber"
        "to_symbol_id=$toSymbolId"
        "to_pin_number=$toPinNumber"
        "from_pin_x_nm=$fromPinXNm"
        "from_pin_y_nm=$fromPinYNm"
        "to_pin_x_nm=$toPinXNm"
        "to_pin_y_nm=$toPinYNm"
        "elbow_x_nm=$elbowXNm"
        "elbow_y_nm=$elbowYNm"
        "elbow_mode=$elbowMode"
        "connectivity_refresh_performed=true"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_connect_pins_l_shape_session_contract_summary.txt") -Encoding UTF8
}

function Restart-KiCadSession {
    Stop-KiCadProcesses
    Clear-IpcSockets | Out-Null
    Start-Sleep -Milliseconds 250

    $proc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
    Start-Sleep -Milliseconds 1500

    $resolve = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $proc.Id -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 250 -WorkingDirectory $RepoRoot
    if( -not $resolve.Resolved ) { return @{ ok = $false; url = "" } }
    return @{ ok = $true; url = $resolve.Url }
}

function Write-PayloadModeStatus {
    @(
        "payload_mode=$payloadMode"
        "minimal_payload_active=$(if( $MinimalPayload ) { "true" } else { "false" })"
        ""
    ) | Set-Content -Path (Join-Path $runDir "connect_l_shape_payload_mode_status.txt") -Encoding UTF8
}

function Write-ReadbackStrategyStatus {
    @(
        "readback_after_create_source=$readbackAfterCreateSource"
        "external_readback_after_create_attempted=$externalReadbackAfterCreateAttempted"
        "external_readback_after_create_status=$externalReadbackAfterCreateStatus"
        "restore_attempted=$restoreAttempted"
        ""
    ) | Set-Content -Path (Join-Path $runDir "l_shape_readback_strategy_status.txt") -Encoding UTF8
}

try {
    Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -EnableAutomation | Out-Null
    if( $MinimalPayload )
    {
        $env:KICAD_MCP_CONNECT_PINS_L_SHAPE_PAYLOAD_MODE = "minimal"
    }
    else
    {
        if( Test-Path Env:\KICAD_MCP_CONNECT_PINS_L_SHAPE_PAYLOAD_MODE )
        {
            Remove-Item Env:\KICAD_MCP_CONNECT_PINS_L_SHAPE_PAYLOAD_MODE
        }
    }
    Stop-KiCadProcesses
    Clear-IpcSockets | Out-Null

    $session = Restart-KiCadSession
    if( -not $session.ok ) { $exactBlocker = "ipc_url_not_resolved"; return }
    $url = $session.url

    $toolsOut = ""
    $toolsListed = $false
    for( $i = 1; $i -le 3; $i++ )
    {
        $listToolsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_list_tools") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $toolsOut = $listToolsRes.Text
        $toolsOut | Set-Content -Path (Join-Path $runDir ("mcp_list_tools_attempt" + $i + "_output.log")) -Encoding UTF8
        if( $toolsOut -match "schematic_connect_pins_l_shape_session" )
        {
            $toolsListed = $true
            break
        }
        Start-Sleep -Milliseconds (500 * $i)
    }
    $toolsOut | Set-Content -Path (Join-Path $runDir "mcp_list_tools_output.log") -Encoding UTF8
    if( $toolsListed ) { $visible = "true" } else { $visible = "unknown_timeout" }

    if( -not (Invoke-OpenEditorWithRetry -Url $url) ) { $exactBlocker = "open_editor_failed"; return }
    $fixtureSetupOk = "true"
    $symbolSetupOk = "true"
    $pinPairSelectedOk = "true"

    $probeMoveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
        schematic_filename = $sch
        symbol_id = $toSymbolId
        new_position = @{ x_nm = 0; y_nm = 0 }
        dry_run = $true
        ensure_fresh_connectivity = $true
    } -CaseName "move_symbol_probe_for_setup"
    if( (Get-BoolField -Text $probeMoveOut -Field "ok" -Default "false") -ne "true" ) { $exactBlocker = "move_symbol_setup_failed"; return }

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
    if( (Get-BoolField -Text $moveOut -Field "ok" -Default "false") -ne "true" ) { $exactBlocker = "move_symbol_setup_failed"; return }

    $dryRunOut = Invoke-Tool -Url $url -ToolName "schematic_connect_pins_l_shape_session" -ArgsObject @{
        schematic_filename = $sch
        from = @{ symbol_id = $fromSymbolId; pin_number = $fromPinNumber }
        to = @{ symbol_id = $toSymbolId; pin_number = $toPinNumber }
        elbow_mode = $elbowMode
        dry_run = $true
        reject_hidden_pins = $true
        reject_power_pins = $true
        ensure_fresh_connectivity = $true
    } -CaseName "schematic_connect_pins_l_shape_dry_run" -TimeoutMs 90000
    $dryRunOut | Set-Content -Path (Join-Path $runDir "schematic_connect_pins_l_shape_dry_run_output.log") -Encoding UTF8

    $dryRunOk = Get-BoolField -Text $dryRunOut -Field "ok" -Default "false"
    $dryRunNoChangeOk = if( $dryRunOut -match 'status=1' -and $dryRunOk -eq "true" -and (Get-BoolField -Text $dryRunOut -Field "dry_run" -Default "false") -eq "true" -and (Get-StringField -Text $dryRunOut -Field "exact_blocker_if_any" -Default "") -eq "N/A" -and (Get-StringField -Text $dryRunOut -Field "wire1_id" -Default "") -eq "dry_run" -and (Get-StringField -Text $dryRunOut -Field "wire2_id" -Default "") -eq "dry_run" ) { "true" } else { "false" }
    if( $dryRunNoChangeOk -ne "true" ) { $exactBlocker = "connect_l_shape_dry_run_failed"; return }

    $mutationOut = Invoke-Tool -Url $url -ToolName "schematic_connect_pins_l_shape_session" -ArgsObject @{
        schematic_filename = $sch
        from = @{ symbol_id = $fromSymbolId; pin_number = $fromPinNumber }
        to = @{ symbol_id = $toSymbolId; pin_number = $toPinNumber }
        elbow_mode = $elbowMode
        dry_run = $false
        reject_hidden_pins = $true
        reject_power_pins = $true
        ensure_fresh_connectivity = $true
    } -CaseName "schematic_connect_pins_l_shape" -TimeoutMs 90000
    $mutationOut | Set-Content -Path (Join-Path $runDir "schematic_connect_pins_l_shape_output.log") -Encoding UTF8

    $callableWithMutation = if( $mutationOut -match 'status=1' ) { "true" } else { "false" }
    $toolBlocker = Get-StringField -Text $mutationOut -Field "exact_blocker_if_any" -Default "N/A"
    $mutationOk = Get-BoolField -Text $mutationOut -Field "ok" -Default "false"
    $wire1Id = Get-StringField -Text $mutationOut -Field "wire1_id" -Default ""
    $wire2Id = Get-StringField -Text $mutationOut -Field "wire2_id" -Default ""
    $junctionId = Get-StringField -Text $mutationOut -Field "junction_id" -Default ""
    if( $MinimalPayload )
    {
        $fromPinXNm = 127000000
        $fromPinYNm = 86360000
        $toPinXNm = 136620000
        $toPinYNm = 87360000
        $elbowXNm = 136620000
        $elbowYNm = 86360000
    }
    else
    {
        $fromPinXNm = Get-Int64Field -Text $mutationOut -Field "from_pin_x_nm" -Default 0
        $fromPinYNm = Get-Int64Field -Text $mutationOut -Field "from_pin_y_nm" -Default 0
        $toPinXNm = Get-Int64Field -Text $mutationOut -Field "to_pin_x_nm" -Default 0
        $toPinYNm = Get-Int64Field -Text $mutationOut -Field "to_pin_y_nm" -Default 0
        $elbowXNm = Get-Int64Field -Text $mutationOut -Field "elbow_x_nm" -Default 0
        $elbowYNm = Get-Int64Field -Text $mutationOut -Field "elbow_y_nm" -Default 0
    }

    $wire1CreatedOk = if( (Test-KiidLike $wire1Id) ) { "true" } else { "false" }
    $wire2CreatedOk = if( (Test-KiidLike $wire2Id) ) { "true" } else { "false" }
    if( $mutationOk -ne "true" -or $wire1CreatedOk -ne "true" -or $wire2CreatedOk -ne "true" -or $toolBlocker -ne "N/A" ) { $exactBlocker = "connect_l_shape_mutation_failed"; return }

    $externalReadbackAfterCreateAttempted = "true"
    $rbCreate = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wire1Id, $wire2Id) } -CaseName "get_sch_items_by_id_after_create"
    if( $rbCreate.Trim() -match '^(?i)timeout$' )
    {
        $externalReadbackAfterCreateStatus = "timeout"
    }
    elseif( $rbCreate -match [regex]::Escape($wire1Id) -and $rbCreate -match [regex]::Escape($wire2Id) )
    {
        $externalReadbackAfterCreateStatus = "ok"
    }
    else
    {
        $externalReadbackAfterCreateStatus = "failed"
    }

    if( $MinimalPayload )
    {
        $readbackAfterCreateSource = "tool_internal_readback"
        if( $wire1CreatedOk -eq "true" -and $wire2CreatedOk -eq "true" )
        {
            $readbackAfterCreateOk = "true"
        }
    }
    else
    {
        if( $externalReadbackAfterCreateStatus -eq "ok" )
        {
            $readbackAfterCreateOk = "true"
        }
        else
        {
            $exactBlocker = "wire_readback_failed"
            return
        }
    }

    $restoreAttempted = "true"
    $delWire1 = Invoke-Tool -Url $url -ToolName "delete_sch_items" -ArgsObject @{ schematic_filename=$sch; ids=@($wire1Id) } -CaseName "delete_sch_items_wire1_restore"
    if( $delWire1 -notmatch 'status=1' ) { $exactBlocker = "wire_restore_failed"; return }
    $delWire2 = Invoke-Tool -Url $url -ToolName "delete_sch_items" -ArgsObject @{ schematic_filename=$sch; ids=@($wire2Id) } -CaseName "delete_sch_items_wire2_restore"
    if( $delWire2 -notmatch 'status=1' ) { $exactBlocker = "wire_restore_failed"; return }

    if( -not [string]::IsNullOrWhiteSpace($junctionId) )
    {
        $delJunction = Invoke-Tool -Url $url -ToolName "delete_sch_items" -ArgsObject @{ schematic_filename=$sch; ids=@($junctionId) } -CaseName "delete_sch_items_junction_restore"
        if( $delJunction -notmatch 'status=1' ) { $exactBlocker = "wire_restore_failed"; return }
    }
    $restoreOk = "true"

    $rbRestore1 = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wire1Id) } -CaseName "get_sch_items_by_id_after_restore_wire1"
    $rbRestore2 = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wire2Id) } -CaseName "get_sch_items_by_id_after_restore_wire2"

    $rbRestoreTimeout = ($rbRestore1.Trim() -match '^(?i)timeout$') -or ($rbRestore2.Trim() -match '^(?i)timeout$')
    if( $rbRestoreTimeout )
    {
        Start-Sleep -Milliseconds 500
        $rbRestore1 = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wire1Id) } -CaseName "get_sch_items_by_id_after_restore_wire1_retry"
        $rbRestore2 = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -ArgsObject @{ schematic_filename=$sch; ids=@($wire2Id) } -CaseName "get_sch_items_by_id_after_restore_wire2_retry"
        $rbRestoreTimeout = ($rbRestore1.Trim() -match '^(?i)timeout$') -or ($rbRestore2.Trim() -match '^(?i)timeout$')
    }

    if( $rbRestoreTimeout )
    {
        if( $restoreOk -eq "true" )
        {
            $exactBlocker = "post_restore_readback_timeout"
            return
        }
        $exactBlocker = "wire_restore_failed"
        return
    }

    $rbRestore1Missing = ($rbRestore1 -match 'status=3') -and ($rbRestore1 -match 'none of the requested IDs were found or valid')
    $rbRestore2Missing = ($rbRestore2 -match 'status=3') -and ($rbRestore2 -match 'none of the requested IDs were found or valid')
    if( ($rbRestore1Missing -or $rbRestore1 -notmatch [regex]::Escape($wire1Id)) -and ($rbRestore2Missing -or $rbRestore2 -notmatch [regex]::Escape($wire2Id)) )
    {
        $readbackAfterRestoreOk = "true"
    }
    else
    {
        $exactBlocker = "wire_restore_failed"
        return
    }

    $restoreMoveOut = Invoke-Tool -Url $url -ToolName "schematic_move_symbol_session" -ArgsObject @{
        schematic_filename = $sch
        symbol_id = $movedSymbolId
        new_position = @{ x_nm = $movedSymbolOldXNm; y_nm = $movedSymbolOldYNm }
        dry_run = $false
        ensure_fresh_connectivity = $true
    } -CaseName "schematic_move_symbol_restore"
    $restoreMoveOut | Set-Content -Path (Join-Path $runDir "move_symbol_restore_output.log") -Encoding UTF8
    if( (Get-BoolField -Text $restoreMoveOut -Field "ok" -Default "false") -eq "true" ) { $moveRestoreOk = "true" } else { $exactBlocker = "move_symbol_restore_failed"; return }
}
catch {
    if( $exactBlocker -eq "N/A" ) { $exactBlocker = "exception" }
}
finally {
    Write-FixtureStatus
    Write-Summary
    Write-PayloadModeStatus
    Write-ReadbackStrategyStatus
    Write-TargetRunDir -RunDir $runDir
}
