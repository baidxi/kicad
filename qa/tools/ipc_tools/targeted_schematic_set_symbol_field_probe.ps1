param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R138 — Prefere issue19090 si present ; mutation DATASHEET + restore ;
# Stop-KiCadProcesses recommande avant checkpoint si autres probes mutantes ont tourne (MCP_DECISIONS R138).

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-set-symbol-field-targeted")
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

function Parse-SetFieldResult {
    param([string]$OutputText)

    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if( $OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }
    $changed = "false"
    if( $OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)' ) { $changed = $Matches[1] }
    $blocker = "absent"
    if( $OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $blocker = $Matches[1] }
    $oldValue = ""
    if( $OutputText -match 'key:\s+"old_value"[\s\S]*?string_value:\s*"([^"]*)"' ) { $oldValue = $Matches[1] }
    $newValue = ""
    if( $OutputText -match 'key:\s+"new_value"[\s\S]*?string_value:\s*"([^"]*)"' ) { $newValue = $Matches[1] }
    $fieldId = ""
    if( $OutputText -match 'key:\s+"field_id"[\s\S]*?string_value:\s*"([^"]*)"' ) { $fieldId = $Matches[1] }
    $fieldName = ""
    if( $OutputText -match 'key:\s+"field_name"[\s\S]*?string_value:\s*"([^"]*)"' ) { $fieldName = $Matches[1] }
    $symbolRef = ""
    if( $OutputText -match 'key:\s+"symbol_ref"[\s\S]*?string_value:\s*"([^"]*)"' ) { $symbolRef = $Matches[1] }

    return @{
        status_ok = $statusOk
        ok = $ok
        changed = $changed
        blocker = $blocker
        old_value = $oldValue
        new_value = $newValue
        field_id = $fieldId
        field_name = $fieldName
        symbol_ref = $symbolRef
    }
}

function Invoke-SetFieldSession {
    param(
        [string]$Url,
        [hashtable]$ToolArgs,
        [string]$ArgsPath,
        [string]$LogPath
    )

    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_set_symbol_field_session", ("@" + $ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    $parsed = Parse-SetFieldResult -OutputText $out
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
    ) | Set-Content -Path (Join-Path $runDir "set_symbol_field_tool_visibility_status.txt") -Encoding UTF8
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
        if( $lt -match "schematic_set_symbol_field_session" ) { $toolVisible = "true" }
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

    $temporaryValue = "https://example.invalid/mcp-qa-datasheet"
    $missingFieldName = "MCP_QA_MISSING_FIELD_" + (Get-Date -Format "yyyyMMddHHmmssfff")
    $candidateFieldIds = @("DATASHEET", "FOOTPRINT", "DESCRIPTION")

    $selectedFieldId = ""
    $selectedFieldName = ""
    $oldValue = ""
    $symbolRef = ""
    foreach( $candidate in $candidateFieldIds )
    {
        $probeArgs = @{
            schematic_filename = $sch
            symbol_id = $symbolId
            field_id = $candidate
            new_value = $temporaryValue
            dry_run = $true
        }

        $probeCall = Invoke-SetFieldSession -Url $url -ToolArgs $probeArgs `
            -ArgsPath (Join-Path $runDir ("set_symbol_field_candidate_" + $candidate + "_args.json")) `
            -LogPath (Join-Path $runDir ("set_symbol_field_candidate_" + $candidate + "_output.log"))

        $parsed = $probeCall.parsed
        if( $parsed.status_ok -and $parsed.ok -eq "true" -and $parsed.blocker -eq "N/A" )
        {
            $selectedFieldId = $parsed.field_id
            $selectedFieldName = $parsed.field_name
            $oldValue = $parsed.old_value
            $symbolRef = $parsed.symbol_ref
            break
        }
    }

    if( [string]::IsNullOrWhiteSpace( $selectedFieldId ) )
    {
        throw "no suitable existing field found among DATASHEET/FOOTPRINT/DESCRIPTION"
    }

    $dryRunArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $temporaryValue
        expected_old_value = $oldValue
        dry_run = $true
    }
    $dryRunCall = Invoke-SetFieldSession -Url $url -ToolArgs $dryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_dry_run_output.log")

    $verifyAfterDryRunArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $temporaryValue
        expected_old_value = $oldValue
        dry_run = $true
    }
    $verifyAfterDryRunCall = Invoke-SetFieldSession -Url $url -ToolArgs $verifyAfterDryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_dry_run_output.log")

    $setArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $temporaryValue
        expected_old_value = $oldValue
        dry_run = $false
    }
    $setCall = Invoke-SetFieldSession -Url $url -ToolArgs $setArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_set_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_output.log")

    $verifyAfterSetArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $temporaryValue
        expected_old_value = $temporaryValue
        dry_run = $true
    }
    $verifyAfterSetCall = Invoke-SetFieldSession -Url $url -ToolArgs $verifyAfterSetArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_set_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_set_output.log")

    $restoreArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $oldValue
        expected_old_value = $temporaryValue
        dry_run = $false
    }
    $restoreCall = Invoke-SetFieldSession -Url $url -ToolArgs $restoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_restore_output.log")

    $verifyAfterRestoreArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = $selectedFieldId
        new_value = $oldValue
        expected_old_value = $oldValue
        dry_run = $true
    }
    $verifyAfterRestoreCall = Invoke-SetFieldSession -Url $url -ToolArgs $verifyAfterRestoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_symbol_field_verify_after_restore_output.log")
    if( $toolVisibilityMode -eq "direct_call_fallback" )
    {
        $directCallAttempted = "true"
        $directCallStatus = if( $dryRunCall.parsed.status_ok ) { "ok" } else { "failed" }
    }
    if( $listToolsFinalStatus -eq "timeout" -and $directCallStatus -eq "ok" )
    {
        $toolVisible = "unknown_list_tools_timeout_but_direct_call_ok"
    }
    Write-ToolVisibilityStatus -PingOk $pingBeforeListToolsOk -ListToolsFinalStatus $listToolsFinalStatus `
        -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
        -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $negRefArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = "REFERENCE"
        new_value = "RTEST_QA"
        dry_run = $true
    }
    $negRef = Invoke-SetFieldSession -Url $url -ToolArgs $negRefArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_field_negative_reference_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_field_negative_reference_output.log")

    $negValueArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_id = "VALUE"
        new_value = "VALUE_QA"
        dry_run = $true
    }
    $negValue = Invoke-SetFieldSession -Url $url -ToolArgs $negValueArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_field_negative_value_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_field_negative_value_output.log")

    $negCreateMissingArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_name = $missingFieldName
        new_value = "QA_MISSING_CREATE"
        create_if_missing = $true
        dry_run = $true
    }
    $negCreateMissing = Invoke-SetFieldSession -Url $url -ToolArgs $negCreateMissingArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_field_negative_create_missing_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_field_negative_create_missing_output.log")

    $negMissingFieldArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        field_name = $missingFieldName
        new_value = "QA_MISSING_NO_CREATE"
        dry_run = $true
    }
    $negMissingField = Invoke-SetFieldSession -Url $url -ToolArgs $negMissingFieldArgs `
        -ArgsPath (Join-Path $runDir "set_symbol_field_negative_missing_field_args.json") `
        -LogPath (Join-Path $runDir "set_symbol_field_negative_missing_field_output.log")

    $fixtureSetupOk = if( $openOut -match 'status=1' ) { "true" } else { "false" }
    $fieldSelectedOk = if( [string]::IsNullOrWhiteSpace( $selectedFieldId ) ) { "false" } else { "true" }
    $visible = $toolVisible
    $callableWithMutation = if( $setCall.parsed.status_ok ) { "true" } else { "false" }
    $dryRunOk = if( $dryRunCall.parsed.status_ok -and $dryRunCall.parsed.ok -eq "true" -and $dryRunCall.parsed.changed -eq "false" ) { "true" } else { "false" }
    $dryRunNoChangeOk = if( $verifyAfterDryRunCall.parsed.status_ok -and $verifyAfterDryRunCall.parsed.ok -eq "true" -and $verifyAfterDryRunCall.parsed.changed -eq "false" -and $verifyAfterDryRunCall.parsed.old_value -eq $oldValue ) { "true" } else { "false" }
    $mutationOk = if( $setCall.parsed.status_ok -and $setCall.parsed.ok -eq "true" -and $setCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterSetOk = if( $verifyAfterSetCall.parsed.status_ok -and $verifyAfterSetCall.parsed.ok -eq "true" -and $verifyAfterSetCall.parsed.old_value -eq $temporaryValue ) { "true" } else { "false" }
    $restoreOk = if( $restoreCall.parsed.status_ok -and $restoreCall.parsed.ok -eq "true" -and $restoreCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterRestoreOk = if( $verifyAfterRestoreCall.parsed.status_ok -and $verifyAfterRestoreCall.parsed.ok -eq "true" -and $verifyAfterRestoreCall.parsed.old_value -eq $oldValue ) { "true" } else { "false" }
    $negativeReferenceOk = if( $negRef.parsed.status_ok -and $negRef.parsed.ok -eq "false" -and $negRef.parsed.blocker -eq "reference_field_forbidden_in_v1" ) { "true" } else { "false" }
    $negativeValueOk = if( $negValue.parsed.status_ok -and $negValue.parsed.ok -eq "false" -and $negValue.parsed.blocker -eq "use_schematic_set_symbol_value_session" ) { "true" } else { "false" }
    $negativeCreateMissingOk = if( $negCreateMissing.parsed.status_ok -and $negCreateMissing.parsed.ok -eq "false" -and $negCreateMissing.parsed.blocker -eq "field_creation_not_supported" ) { "true" } else { "false" }
    $negativeMissingFieldOk = if( $negMissingField.parsed.status_ok -and $negMissingField.parsed.ok -eq "false" -and $negMissingField.parsed.blocker -eq "field_not_found" ) { "true" } else { "false" }

    $toolBlocker = $setCall.parsed.blocker
    if( [string]::IsNullOrWhiteSpace( $toolBlocker ) ) { $toolBlocker = "N/A" }
    $blocker = "N/A"
    if( $callableWithMutation -ne "true" ) { $blocker = "call_failed" }
    elseif( $fixtureSetupOk -ne "true" ) { $blocker = "fixture_open_failed" }
    elseif( $fieldSelectedOk -ne "true" ) { $blocker = "field_selection_failed" }
    elseif( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $blocker = "dry_run_changed_state" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_failed" }
    elseif( $readbackAfterSetOk -ne "true" ) { $blocker = "readback_after_mutation_failed" }
    elseif( $restoreOk -ne "true" ) { $blocker = "restore_failed" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "readback_after_restore_failed" }
    elseif( $negativeReferenceOk -ne "true" ) { $blocker = "negative_reference_failed" }
    elseif( $negativeValueOk -ne "true" ) { $blocker = "negative_value_failed" }
    elseif( $negativeCreateMissingOk -ne "true" ) { $blocker = "negative_create_missing_failed" }
    elseif( $negativeMissingFieldOk -ne "true" ) { $blocker = "negative_missing_field_failed" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "field_selected_ok=$fieldSelectedOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "readback_after_mutation_ok=$readbackAfterSetOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "negative_reference_ok=$negativeReferenceOk"
        "negative_value_ok=$negativeValueOk"
        "negative_create_missing_ok=$negativeCreateMissingOk"
        "negative_missing_field_ok=$negativeMissingFieldOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "symbol_ref=$symbolRef"
        "field_id=$selectedFieldId"
        "field_name=$selectedFieldName"
        "old_value=$oldValue"
        "new_value=$temporaryValue"
        "restored_value=$($verifyAfterRestoreCall.parsed.old_value)"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_set_symbol_field_session_contract_summary.txt") -Encoding UTF8

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
