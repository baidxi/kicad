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
$sch      = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-set-symbol-reference-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

function Resolve-SymbolIdFromSchItemsText {
    param([string]$ItemsOut)
    $fromFirstItem = [regex]::Match($ItemsOut,'first_item_id=([0-9a-fA-F-]{36})')
    if($fromFirstItem.Success){ return $fromFirstItem.Groups[1].Value }

    $fromIdField = [regex]::Match($ItemsOut,'key:\s+"id"[\s\S]{0,220}?key:\s+"value"[\s\S]{0,220}?string_value:\s*"([0-9a-fA-F-]{36})"')
    if($fromIdField.Success){ return $fromIdField.Groups[1].Value }

    return ""
}

function Parse-SetReferenceResult {
    param([string]$OutputText)
    $ok = "false"; if($OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)'){ $ok=$Matches[1] }
    $changed = "false"; if($OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)'){ $changed=$Matches[1] }
    $blocker = "N/A"; if($OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"'){ $blocker=$Matches[1] }
    $oldRef = ""; if($OutputText -match 'key:\s+"old_reference"[\s\S]*?string_value:\s*"([^"]*)"'){ $oldRef=$Matches[1] }
    $newRef = ""; if($OutputText -match 'key:\s+"new_reference"[\s\S]*?string_value:\s*"([^"]*)"'){ $newRef=$Matches[1] }
    $symbolRef = ""; if($OutputText -match 'key:\s+"symbol_ref"[\s\S]*?string_value:\s*"([^"]*)"'){ $symbolRef=$Matches[1] }
    return @{ status_ok = ($OutputText -match 'status=1'); ok=$ok; changed=$changed; blocker=$blocker; old_reference=$oldRef; new_reference=$newRef; symbol_ref=$symbolRef }
}

function Invoke-SetReferenceSession {
    param([string]$Url,[hashtable]$ToolArgs,[string]$ArgsPath,[string]$LogPath)
    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url,"mcp_call_tool_args","schematic_set_symbol_reference_session",("@"+$ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    return @{ raw=$out; parsed=(Parse-SetReferenceResult -OutputText $out) }
}

function Write-ToolVisibilityStatus {
    param([string]$PingOk,[string]$ListToolsFinalStatus,[string]$ToolVisibilityMode,[string]$ToolVisible,[string]$DirectCallAttempted,[string]$DirectCallStatus)
    @(
        "ping_before_list_tools_ok=$PingOk"
        "list_tools_final_status=$ListToolsFinalStatus"
        "tool_visibility_mode=$ToolVisibilityMode"
        "tool_visible=$ToolVisible"
        "direct_call_attempted=$DirectCallAttempted"
        "direct_call_status=$DirectCallStatus"
        ""
    ) | Set-Content -Path (Join-Path $runDir "set_symbol_reference_tool_visibility_status.txt") -Encoding UTF8
}

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if($resolved.Resolved){$resolved.Url}else{$null}
    if(-not $url){ throw "Unable to resolve API URL" }

    $ping = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url,"ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
    if($ping.Text -notmatch 'status=1'){ throw "KiCad API ping failed" }

    $pingBeforeListToolsOk = "true"; $listToolsFinalStatus="failed"; $toolVisibilityMode="list_tools"; $toolVisible="unknown"; $directCallAttempted="false"; $directCallStatus="not_run"
    $lt = ""; $toolsSucceeded=$false; $toolsTimedOut=$false
    for($i=1;$i -le 3;$i++){
        $ltRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url,"mcp_list_tools") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $lt = $ltRes.Text
        $lt | Set-Content -Path (Join-Path $runDir ("mcp_list_tools_attempt"+$i+".log")) -Encoding UTF8
        if($lt -match '^(?i)\s*timeout\s*$'){ $toolsTimedOut=$true; continue }
        $toolsSucceeded=$true; break
    }
    if(-not $toolsSucceeded){
        if($toolsTimedOut){ $listToolsFinalStatus="timeout"; $toolVisibilityMode="direct_call_fallback" } else { $listToolsFinalStatus="failed" }
    } else {
        $listToolsFinalStatus="ok"
        if($lt -match "schematic_set_symbol_reference_session"){ $toolVisible="true" } else { $toolVisible="false"; $toolVisibilityMode="direct_call_fallback" }
    }

    $openArgs = @{ type="sch"; filename=$sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url,"mcp_call_tool_args","qa_open_editor_frame",("@"+$openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if($openOut -notmatch 'status=1'){ throw "qa_open_editor_frame failed" }

    $itemsArgs = @{ schematic_filename=$sch; types=@("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
    $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
    $itemsOut = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url,"mcp_call_tool_args","get_sch_items",("@"+$itemsInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_symbols_output.log") -Encoding UTF8
    $symbolId = Resolve-SymbolIdFromSchItemsText -ItemsOut $itemsOut
    if([string]::IsNullOrWhiteSpace($symbolId)){ throw "symbol_id discovery failed" }

    $newReference = "R901"
    $dryRunCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$newReference; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_dry_run_output.log")

    if($toolVisibilityMode -eq "direct_call_fallback"){
        $directCallAttempted="true"
        if($dryRunCall.raw -match '^(?i)\s*timeout\s*$'){ $directCallStatus="timeout" }
        elseif($dryRunCall.parsed.status_ok){ $directCallStatus="ok" }
        elseif($dryRunCall.raw -match '(?i)tool[^\\r\\n]*not found|unknown tool|no such tool'){ $directCallStatus="tool_not_found" }
        else { $directCallStatus="failed" }
    }
    if($listToolsFinalStatus -eq "timeout" -and $directCallStatus -eq "ok"){ $toolVisible="unknown_list_tools_timeout_but_direct_call_ok" }
    Write-ToolVisibilityStatus -PingOk $pingBeforeListToolsOk -ListToolsFinalStatus $listToolsFinalStatus -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $oldReference = $dryRunCall.parsed.old_reference
    $symbolRef = $dryRunCall.parsed.symbol_ref

    $verifyAfterDryRunCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$newReference; expected_old_reference=$oldReference; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_dry_run_output.log")
    $mutationCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$newReference; expected_old_reference=$oldReference; dry_run=$false } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_mutation_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_output.log")
    $verifyAfterMutationCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$newReference; expected_old_reference=$newReference; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_mutation_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_mutation_output.log")
    $restoreCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$oldReference; expected_old_reference=$newReference; dry_run=$false } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_restore_output.log")
    $verifyAfterRestoreCall = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$oldReference; expected_old_reference=$oldReference; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_reference_verify_after_restore_output.log")

    $negExpected = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference=$newReference; expected_old_reference="__BAD__"; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "set_symbol_reference_negative_expected_old_reference_mismatch_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_reference_negative_expected_old_reference_mismatch_output.log")
    $negInvalidRef = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id=$symbolId; new_reference="R 1"; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "set_symbol_reference_negative_invalid_reference_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_reference_negative_invalid_reference_output.log")
    $negInvalidId = Invoke-SetReferenceSession -Url $url -ToolArgs @{ schematic_filename=$sch; symbol_id="invalid-symbol-id"; new_reference="R902"; dry_run=$true } `
        -ArgsPath (Join-Path $runDir "set_symbol_reference_negative_invalid_symbol_id_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_reference_negative_invalid_symbol_id_output.log")

    $visible = $toolVisible
    $callableWithMutation = if($mutationCall.parsed.status_ok){"true"}else{"false"}
    $fixtureSetupOk = if($openOut -match 'status=1'){"true"}else{"false"}
    $symbolSelectedOk = if([string]::IsNullOrWhiteSpace($symbolId)){"false"}else{"true"}
    $dryRunOk = if($dryRunCall.parsed.status_ok -and $dryRunCall.parsed.ok -eq "true" -and $dryRunCall.parsed.changed -eq "false"){"true"}else{"false"}
    $dryRunNoChangeOk = if($verifyAfterDryRunCall.parsed.status_ok -and $verifyAfterDryRunCall.parsed.ok -eq "true" -and $verifyAfterDryRunCall.parsed.changed -eq "false" -and $verifyAfterDryRunCall.parsed.old_reference -eq $oldReference){"true"}else{"false"}
    $mutationOk = if($mutationCall.parsed.status_ok -and $mutationCall.parsed.ok -eq "true" -and $mutationCall.parsed.changed -eq "true"){"true"}else{"false"}
    $readbackAfterMutationOk = if($verifyAfterMutationCall.parsed.status_ok -and $verifyAfterMutationCall.parsed.ok -eq "true" -and $verifyAfterMutationCall.parsed.old_reference -eq $newReference){"true"}else{"false"}
    $restoreOk = if($restoreCall.parsed.status_ok -and $restoreCall.parsed.ok -eq "true" -and $restoreCall.parsed.changed -eq "true"){"true"}else{"false"}
    $readbackAfterRestoreOk = if($verifyAfterRestoreCall.parsed.status_ok -and $verifyAfterRestoreCall.parsed.ok -eq "true" -and $verifyAfterRestoreCall.parsed.old_reference -eq $oldReference){"true"}else{"false"}
    $negExpectedOk = if($negExpected.parsed.status_ok -and $negExpected.parsed.ok -eq "false" -and $negExpected.parsed.blocker -eq "expected_old_reference_mismatch"){"true"}else{"false"}
    $negInvalidRefOk = if($negInvalidRef.parsed.status_ok -and $negInvalidRef.parsed.ok -eq "false" -and $negInvalidRef.parsed.blocker -eq "invalid_reference"){"true"}else{"false"}
    $negInvalidIdOk = if($negInvalidId.parsed.status_ok -and $negInvalidId.parsed.ok -eq "false" -and ($negInvalidId.parsed.blocker -eq "invalid_symbol_id" -or $negInvalidId.parsed.blocker -eq "symbol_not_found")){"true"}else{"false"}

    $blocker = "N/A"
    if($callableWithMutation -ne "true"){ $blocker="call_failed" }
    elseif($fixtureSetupOk -ne "true"){ $blocker="fixture_open_failed" }
    elseif($symbolSelectedOk -ne "true"){ $blocker="symbol_selection_failed" }
    elseif($dryRunOk -ne "true"){ $blocker="dry_run_not_ok" }
    elseif($dryRunNoChangeOk -ne "true"){ $blocker="dry_run_changed_state" }
    elseif($mutationOk -ne "true"){ $blocker="mutation_failed" }
    elseif($readbackAfterMutationOk -ne "true"){ $blocker="readback_after_mutation_failed" }
    elseif($restoreOk -ne "true"){ $blocker="restore_failed" }
    elseif($readbackAfterRestoreOk -ne "true"){ $blocker="readback_after_restore_failed" }
    elseif($negExpectedOk -ne "true"){ $blocker="negative_expected_old_reference_mismatch_failed" }
    elseif($negInvalidRefOk -ne "true"){ $blocker="negative_invalid_reference_failed" }
    elseif($negInvalidIdOk -ne "true"){ $blocker="negative_invalid_symbol_id_failed" }

    $toolBlocker = $mutationCall.parsed.blocker
    if([string]::IsNullOrWhiteSpace($toolBlocker)){ $toolBlocker = "N/A" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "symbol_selected_ok=$symbolSelectedOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "readback_after_mutation_ok=$readbackAfterMutationOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "negative_expected_old_reference_mismatch_ok=$negExpectedOk"
        "negative_invalid_reference_ok=$negInvalidRefOk"
        "negative_invalid_symbol_id_ok=$negInvalidIdOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "old_reference=$oldReference"
        "new_reference=$newReference"
        "restored_reference=$($verifyAfterRestoreCall.parsed.old_reference)"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_set_symbol_reference_session_contract_summary.txt") -Encoding UTF8

    Write-TargetRunDir -RunDir $runDir
}
catch {
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
