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
$sch      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schWithKnownSymbols = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
if( Test-Path -LiteralPath $schWithKnownSymbols )
{
    $sch = $schWithKnownSymbols
}
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-set-symbol-value-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
}

function Take-FirstValid {
    param([System.Text.RegularExpressions.MatchCollection]$Matches)
    foreach( $m in $Matches )
    {
        if( $m.Groups.Count -lt 2 ) { continue }
        $v = $m.Groups[1].Value.Trim()
        if( Test-KiidLike $v ) { return $v }
    }
    return ""
}

function Resolve-SymbolIdFromSchItemsText {
    param([string]$ItemsOut)

    if( [string]::IsNullOrWhiteSpace( $ItemsOut ) )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = "none"; Error = "empty_get_sch_items_output" }
    }
    if( $ItemsOut.Trim() -eq "timeout" )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = "none"; Error = "get_sch_items_smoke_timeout" }
    }

    $symbolId = ""
    $method = "none"

    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
    $symbolId = Take-FirstValid $typedSchSymId
    if( $symbolId ) { $method = "typed_sch_symbol_t_id_value" }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $typedSchSymUuid = [regex]::Matches(
            $ItemsOut,
            'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedSchSymUuid
        if( $symbolId ) { $method = "typed_sch_symbol_t_uuid" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $typedKotId = [regex]::Matches(
            $ItemsOut,
            'KOT_SCH_SYMBOL[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedKotId
        if( $symbolId ) { $method = "typed_kot_sch_symbol_id_value" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $typedKotUuid = [regex]::Matches(
            $ItemsOut,
            'KOT_SCH_SYMBOL[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedKotUuid
        if( $symbolId ) { $method = "typed_kot_sch_symbol_uuid" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $idValueCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $idValueCandidates
        if( $symbolId ) { $method = "id_value" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $idFlatCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"id"[\s\S]{0,400}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $idFlatCandidates
        if( $symbolId ) { $method = "id_string_value" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $uuidCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"uuid"[\s\S]{0,400}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $uuidCandidates
        if( $symbolId ) { $method = "uuid_string_value" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $flatCandidates = [regex]::Matches(
            $ItemsOut,
            '(?:\bid=|\buuid=)([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})' )
        $symbolId = Take-FirstValid $flatCandidates
        if( $symbolId ) { $method = "flat_id_or_uuid" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = $method; Error = "symbol_id_not_found_in_get_sch_items_output" }
    }
    return [pscustomobject]@{ SymbolId = $symbolId; Method = $method; Error = "N/A" }
}

function Write-SymbolIdDiscoverySnapshot {
    param(
        [string]$RunDir,
        [string]$DiscoveryOk,
        [string]$Method,
        [string]$ErrorText
    )
    @(
        "symbol_id_discovery_ok=$DiscoveryOk"
        "symbol_id_discovery_method=$Method"
        "symbol_id_discovery_error=$ErrorText"
        ""
    ) | Set-Content -Path (Join-Path $RunDir "qa_symbol_id_discovery.txt") -Encoding UTF8
}

function Read-SymbolValue {
    param(
        [string]$Url,
        [string]$SchematicFilename,
        [string]$SymbolId,
        [string]$LogPath
    )

    $argsJson = @{
        schematic_filename = $SchematicFilename
        id = $SymbolId
    } | ConvertTo-Json -Compress
    $argsPath = Join-Path $runDir "get_symbol_fields_by_id_args.json"
    $argsJson | Set-Content -Path $argsPath -Encoding UTF8

    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "get_symbol_fields_by_id", ("@" + $argsPath)) -TimeoutMs 60000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8

    $value = ""
    if( $out -match 'key:\s+"value"[\s\S]{0,800}?string_value:\s*"([^"]*)"' ) { $value = $Matches[1] }
    return @{
        status_ok = ($out -match 'status=1')
        value = $value
        raw = $out
    }
}

function Parse-SetValueResult {
    param([string]$OutputText)

    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if( $OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }
    $changed = "false"
    if( $OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)' ) { $changed = $Matches[1] }
    $blocker = "N/A"
    if( $OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $blocker = $Matches[1] }
    $oldValue = ""
    if( $OutputText -match 'key:\s+"old_value"[\s\S]*?string_value:\s*"([^"]*)"' ) { $oldValue = $Matches[1] }
    $newValue = ""
    if( $OutputText -match 'key:\s+"new_value"[\s\S]*?string_value:\s*"([^"]*)"' ) { $newValue = $Matches[1] }
    $symbolRef = ""
    if( $OutputText -match 'key:\s+"symbol_ref"[\s\S]*?string_value:\s*"([^"]*)"' ) { $symbolRef = $Matches[1] }

    return @{
        status_ok = $statusOk
        ok = $ok
        changed = $changed
        blocker = $blocker
        old_value = $oldValue
        new_value = $newValue
        symbol_ref = $symbolRef
    }
}

function Invoke-SetValueSession {
    param(
        [string]$Url,
        [hashtable]$ToolArgs,
        [string]$ArgsPath,
        [string]$LogPath
    )

    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_set_symbol_value_session", ("@" + $ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    $parsed = Parse-SetValueResult -OutputText $out
    return @{
        raw = $out
        parsed = $parsed
    }
}

function Write-ToolVisibilityStatus {
    param(
        [string]$PingOk,
        [string]$ListToolsFinalStatus,
        [string]$ToolVisibilityMode,
        [string]$ToolVisible,
        [string]$DirectCallAttempted,
        [string]$DirectCallStatus
    )

    @(
        "ping_before_list_tools_ok=$PingOk"
        "list_tools_final_status=$ListToolsFinalStatus"
        "tool_visibility_mode=$ToolVisibilityMode"
        "tool_visible=$ToolVisible"
        "direct_call_attempted=$DirectCallAttempted"
        "direct_call_status=$DirectCallStatus"
        ""
    ) | Set-Content -Path (Join-Path $runDir "set_symbol_value_tool_visibility_status.txt") -Encoding UTF8
}

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 120 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }

    if( Test-Path -LiteralPath $sockDir )
    {
        $pidSock = Join-Path $sockDir ("api-" + $kicadProc.Id + ".sock")
        $defaultSock = Join-Path $sockDir "api.sock"
        ("resolved=" + $(if($resolved.Resolved){"true"}else{"false"}) +
         ";url=" + $url +
         ";pid_sock=" + $pidSock +
         ";default_sock=" + $defaultSock) | Set-Content -Path (Join-Path $runDir "ipc_url_resolve.txt") -Encoding UTF8
    }

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

    $pingBeforeListToolsOk = "true"
    $listToolsFinalStatus = "failed"
    $toolVisibilityMode = "list_tools"
    $toolVisible = "unknown"
    $directCallAttempted = "false"
    $directCallStatus = "not_run"
    $lt = ""
    $toolsTimedOut = $false
    $toolsSucceeded = $false
    for( $i = 1; $i -le 3; $i++ )
    {
        $ltRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_list_tools") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $lt = $ltRes.Text
        $lt | Set-Content -Path (Join-Path $runDir ("mcp_list_tools_attempt" + $i + ".log")) -Encoding UTF8
        if( $lt -match '^(?i)\s*timeout\s*$' )
        {
            $toolsTimedOut = $true
            if( $i -lt 3 ) { Start-Sleep -Milliseconds 750 }
            continue
        }
        $toolsSucceeded = $true
        break
    }

    if( -not $toolsSucceeded )
    {
        if( $toolsTimedOut )
        {
            $listToolsFinalStatus = "timeout"
            $toolVisibilityMode = "direct_call_fallback"
            $toolVisible = "unknown"
        }
        else
        {
            $listToolsFinalStatus = "failed"
            $toolVisible = "unknown"
        }
    }
    else
    {
        $listToolsFinalStatus = "ok"
        if( $lt -match "schematic_set_symbol_value_session" ) { $toolVisible = "true" }
        else
        {
            $toolVisible = "false"
            $toolVisibilityMode = "direct_call_fallback"
        }
    }

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openTimeoutMs = 120000
    $openOut = ""
    for( $openAttempt = 0; $openAttempt -lt 2; $openAttempt++ )
    {
        $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs $openTimeoutMs -WorkingDirectory $RepoRoot
        $openOut = $openRes.Text
        $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
        if( $openOut -match 'status=1' ) { break }
        if( $openAttempt -eq 0 ) { Start-Sleep -Seconds 4 }
    }
    if( $openOut -notmatch 'status=1' )
    {
        Write-SymbolIdDiscoverySnapshot -RunDir $runDir -DiscoveryOk "false" -Method "none" -ErrorText "qa_open_editor_frame_not_ok_or_timeout"
        throw "qa_open_editor_frame did not return AS_OK after retries"
    }

    $itemsArgs = @{ schematic_filename = $sch; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
    $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
    $itemsTimeoutMs = 120000
    $itemsOut = ""
    for( $itemsAttempt = 0; $itemsAttempt -lt 2; $itemsAttempt++ )
    {
        $itemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs $itemsTimeoutMs -WorkingDirectory $RepoRoot
        $itemsOut = $itemsRes.Text
        $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_for_symbol_id_output.log") -Encoding UTF8
        $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_symbols_output.log") -Encoding UTF8
        if( $itemsOut.Trim() -ne "timeout" ) { break }
        if( $itemsAttempt -eq 0 ) { Start-Sleep -Seconds 4 }
    }

    $discovery = Resolve-SymbolIdFromSchItemsText -ItemsOut $itemsOut
    $symbolId = $discovery.SymbolId
    $discoveryOkStr = if( $symbolId ) { "true" } else { "false" }
    Write-SymbolIdDiscoverySnapshot -RunDir $runDir -DiscoveryOk $discoveryOkStr -Method $discovery.Method -ErrorText $discovery.Error

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        throw "symbol_id discovery failed from get_sch_items output"
    }

    $readBefore = Read-SymbolValue -Url $url -SchematicFilename $sch -SymbolId $symbolId -LogPath (Join-Path $runDir "get_symbol_fields_before_output.log")
    if( -not $readBefore.status_ok ) { throw "get_symbol_fields_by_id before mutation failed" }
    $oldValue = [string]$readBefore.value

    $temporaryValue = "MCP_QA_VALUE_" + (Get-Date -Format "yyyyMMddHHmmssfff")

    $dryRunArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $temporaryValue
        expected_old_value = $oldValue
        dry_run = $true
    }
    $dryRunCall = Invoke-SetValueSession -Url $url -ToolArgs $dryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_dry_run_output.log")

    if( $toolVisibilityMode -eq "direct_call_fallback" )
    {
        $directCallAttempted = "true"
        if( $dryRunCall.raw -match '^(?i)\s*timeout\s*$' ) { $directCallStatus = "timeout" }
        elseif( $dryRunCall.parsed.status_ok ) { $directCallStatus = "ok" }
        elseif( $dryRunCall.raw -match '(?i)tool[^\\r\\n]*not found|unknown tool|no such tool' ) { $directCallStatus = "tool_not_found" }
        else { $directCallStatus = "failed" }
    }
    if( $listToolsFinalStatus -eq "timeout" -and $directCallStatus -eq "ok" )
    {
        $toolVisible = "unknown_list_tools_timeout_but_direct_call_ok"
    }
    Write-ToolVisibilityStatus -PingOk $pingBeforeListToolsOk -ListToolsFinalStatus $listToolsFinalStatus `
        -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
        -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $verifyAfterDryRunCall = Invoke-SetValueSession -Url $url -ToolArgs $dryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_dry_run_output.log")

    $readAfterDryRun = Read-SymbolValue -Url $url -SchematicFilename $sch -SymbolId $symbolId -LogPath (Join-Path $runDir "get_symbol_fields_after_dry_run_output.log")
    if( -not $readAfterDryRun.status_ok ) { throw "get_symbol_fields_by_id after dry_run failed" }
    $afterDryRunValue = [string]$readAfterDryRun.value

    $setArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $temporaryValue
        expected_old_value = $oldValue
        dry_run = $false
    }
    $setCall = Invoke-SetValueSession -Url $url -ToolArgs $setArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_set_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_output.log")

    $verifyAfterMutationArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $temporaryValue
        expected_old_value = $temporaryValue
        dry_run = $true
    }
    $verifyAfterMutationCall = Invoke-SetValueSession -Url $url -ToolArgs $verifyAfterMutationArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_mutation_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_mutation_output.log")

    $readAfterSet = Read-SymbolValue -Url $url -SchematicFilename $sch -SymbolId $symbolId -LogPath (Join-Path $runDir "get_symbol_fields_after_set_output.log")
    if( -not $readAfterSet.status_ok ) { throw "get_symbol_fields_by_id after set failed" }
    $afterSetValue = [string]$readAfterSet.value

    $restoreArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $oldValue
        expected_old_value = $temporaryValue
        dry_run = $false
    }
    $restoreCall = Invoke-SetValueSession -Url $url -ToolArgs $restoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_restore_output.log")

    $verifyAfterRestoreArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $oldValue
        expected_old_value = $oldValue
        dry_run = $true
    }
    $verifyAfterRestoreCall = Invoke-SetValueSession -Url $url -ToolArgs $verifyAfterRestoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_value_verify_after_restore_output.log")

    $readAfterRestore = Read-SymbolValue -Url $url -SchematicFilename $sch -SymbolId $symbolId -LogPath (Join-Path $runDir "get_symbol_fields_after_restore_output.log")
    if( -not $readAfterRestore.status_ok ) { throw "get_symbol_fields_by_id after restore failed" }
    $restoredValue = [string]$readAfterRestore.value

    $negativeExpectedMismatchArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_value = $temporaryValue
        expected_old_value = "__MCP_QA_EXPECTED_OLD_VALUE_MISMATCH__"
        dry_run = $true
    }
    $negativeExpectedMismatch = Invoke-SetValueSession -Url $url -ToolArgs $negativeExpectedMismatchArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_value_negative_expected_old_value_mismatch_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_value_negative_expected_old_value_mismatch_output.log")

    $negativeInvalidSymbolArgs = @{
        schematic_filename = $sch
        symbol_id = "invalid-symbol-id"
        new_value = "QA_INVALID_SYMBOL"
        dry_run = $true
    }
    $negativeInvalidSymbol = Invoke-SetValueSession -Url $url -ToolArgs $negativeInvalidSymbolArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_value_negative_invalid_symbol_id_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_value_negative_invalid_symbol_id_output.log")

    $visible = $toolVisible
    $fixtureSetupOk = if( $openOut -match 'status=1' ) { "true" } else { "false" }
    $symbolSelectedOk = if( [string]::IsNullOrWhiteSpace( $symbolId ) ) { "false" } else { "true" }
    $callableWithMutation = if( $setCall.parsed.status_ok ) { "true" } else { "false" }
    $dryRunOk = if( $dryRunCall.parsed.status_ok -and $dryRunCall.parsed.ok -eq "true" -and $dryRunCall.parsed.changed -eq "false" ) { "true" } else { "false" }
    $dryRunNoChangeOk = if( $verifyAfterDryRunCall.parsed.status_ok -and $verifyAfterDryRunCall.parsed.ok -eq "true" -and $verifyAfterDryRunCall.parsed.changed -eq "false" -and $afterDryRunValue -eq $oldValue ) { "true" } else { "false" }
    $mutationOk = if( $setCall.parsed.status_ok -and $setCall.parsed.ok -eq "true" -and $setCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterMutationOk = if( $verifyAfterMutationCall.parsed.status_ok -and $verifyAfterMutationCall.parsed.ok -eq "true" -and $verifyAfterMutationCall.parsed.old_value -eq $temporaryValue -and $afterSetValue -eq $temporaryValue ) { "true" } else { "false" }
    $restoreOkFlag = if( $restoreCall.parsed.status_ok -and $restoreCall.parsed.ok -eq "true" -and $restoreCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterRestoreOk = if( $verifyAfterRestoreCall.parsed.status_ok -and $verifyAfterRestoreCall.parsed.ok -eq "true" -and $verifyAfterRestoreCall.parsed.old_value -eq $oldValue -and $restoredValue -eq $oldValue ) { "true" } else { "false" }
    $negativeExpectedOldValueMismatchOk = if( $negativeExpectedMismatch.parsed.status_ok -and $negativeExpectedMismatch.parsed.ok -eq "false" -and $negativeExpectedMismatch.parsed.blocker -eq "expected_old_value_mismatch" ) { "true" } else { "false" }
    $negativeInvalidSymbolIdOk = if( $negativeInvalidSymbol.parsed.status_ok -and $negativeInvalidSymbol.parsed.ok -eq "false" -and ( $negativeInvalidSymbol.parsed.blocker -eq "invalid_symbol_id" -or $negativeInvalidSymbol.parsed.blocker -eq "symbol_not_found" ) ) { "true" } else { "false" }

    $blocker = "N/A"
    if( $callableWithMutation -ne "true" ) { $blocker = "call_failed" }
    elseif( $fixtureSetupOk -ne "true" ) { $blocker = "fixture_open_failed" }
    elseif( $symbolSelectedOk -ne "true" ) { $blocker = "symbol_selection_failed" }
    elseif( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $blocker = "dry_run_changed_state" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_not_ok" }
    elseif( $readbackAfterMutationOk -ne "true" ) { $blocker = "readback_after_mutation_failed" }
    elseif( $restoreOkFlag -ne "true" ) { $blocker = "restore_not_ok" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "restore_readback_mismatch" }
    elseif( $negativeExpectedOldValueMismatchOk -ne "true" ) { $blocker = "negative_expected_old_value_mismatch_failed" }
    elseif( $negativeInvalidSymbolIdOk -ne "true" ) { $blocker = "negative_invalid_symbol_id_failed" }

    $toolBlocker = $setCall.parsed.blocker
    if( [string]::IsNullOrWhiteSpace( $toolBlocker ) ) { $toolBlocker = "N/A" }
    $symbolRef = $setCall.parsed.symbol_ref
    if( [string]::IsNullOrWhiteSpace( $symbolRef ) ) { $symbolRef = $dryRunCall.parsed.symbol_ref }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "symbol_selected_ok=$symbolSelectedOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "readback_after_mutation_ok=$readbackAfterMutationOk"
        "restore_ok=$restoreOkFlag"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "negative_expected_old_value_mismatch_ok=$negativeExpectedOldValueMismatchOk"
        "negative_invalid_symbol_id_ok=$negativeInvalidSymbolIdOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "symbol_ref=$symbolRef"
        "old_value=$oldValue"
        "new_value=$temporaryValue"
        "restored_value=$restoredValue"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_set_symbol_value_session_contract_summary.txt") -Encoding UTF8

    Write-TargetRunDir -RunDir $runDir
}
catch
{
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
