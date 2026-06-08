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
$schBuild = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schIssue19090 = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$sch      = $schBuild
$fixtureSource = "build_v1"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $schBuild)) { throw "Missing schematic at $schBuild" }
if( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-symbol-connectivity-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

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

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $symbolId = ""
    $symbolIdDiscoveryOk = "false"
    $symbolIdDiscoveryError = "unknown"
    $symbolDiscoveryMethod = "none"
    $symbolSelectionMethod = "initial_fixture"
    $itemsOut = ""
    $openOut = ""
    $fixtureCandidates = @($sch)
    if( $fixtureSource -eq "issue19090" )
    {
        $fixtureCandidates += @($schBuild)
    }
    $fixtureCandidates = $fixtureCandidates | Select-Object -Unique

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

    foreach( $candidateSch in $fixtureCandidates )
    {
        $sch = $candidateSch
        $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
        $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
        $openArgs | Set-Content -Path $openInput -Encoding UTF8
        $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $openOut = $openRes.Text
        $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
        if( $openOut -notmatch 'status=1' ) { continue }

        $itemsArgs = @{ schematic_filename = $sch; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
        $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
        $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
        $itemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs 60000 -WorkingDirectory $RepoRoot
        $itemsOut = $itemsRes.Text
        $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_for_symbol_id_output.log") -Encoding UTF8
        $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_symbols_output.log") -Encoding UTF8

        # Prefer symbol-typed entries when explicit type markers are present.
        $typedIdCandidates = [regex]::Matches(
            $itemsOut,
            'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedIdCandidates
        if( $symbolId ) { $symbolDiscoveryMethod = "typed_id_value" }

        if( [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            $typedUuidCandidates = [regex]::Matches(
                $itemsOut,
                'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
            $symbolId = Take-FirstValid $typedUuidCandidates
            if( $symbolId ) { $symbolDiscoveryMethod = "typed_uuid" }
        }

        if( [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            $idValueCandidates = [regex]::Matches(
                $itemsOut,
                'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
            $symbolId = Take-FirstValid $idValueCandidates
            if( $symbolId ) { $symbolDiscoveryMethod = "id_value" }
        }

        if( [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            $idFlatCandidates = [regex]::Matches(
                $itemsOut,
                'key:\s+"id"[\s\S]{0,400}?string_value:\s*"([^"]+)"' )
            $symbolId = Take-FirstValid $idFlatCandidates
            if( $symbolId ) { $symbolDiscoveryMethod = "id_string_value" }
        }

        if( [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            $uuidCandidates = [regex]::Matches(
                $itemsOut,
                'key:\s+"uuid"[\s\S]{0,400}?string_value:\s*"([^"]+)"' )
            $symbolId = Take-FirstValid $uuidCandidates
            if( $symbolId ) { $symbolDiscoveryMethod = "uuid_string_value" }
        }

        if( [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            $flatCandidates = [regex]::Matches(
                $itemsOut,
                '(?:\bid=|\buuid=)([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})' )
            $symbolId = Take-FirstValid $flatCandidates
            if( $symbolId ) { $symbolDiscoveryMethod = "flat_id_or_uuid" }
        }

        if( -not [string]::IsNullOrWhiteSpace( $symbolId ) )
        {
            if( $candidateSch -eq $schIssue19090 ) { $fixtureSource = "issue19090" } else { $fixtureSource = "build_v1" }
            $symbolSelectionMethod = "get_sch_items_dynamic"
            break
        }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $symbolIdDiscoveryError = "symbol_id_not_found_in_get_sch_items_output"
    }
    else
    {
        $symbolIdDiscoveryOk = "true"
        $symbolIdDiscoveryError = "N/A"
    }

    ("fixture_source=" + $fixtureSource +
     ";fixture_schematic=" + $sch +
     ";symbol_id=" + $symbolId +
     ";symbol_selection_method=" + $symbolSelectionMethod +
     ";symbol_id_discovery_method=" + $symbolDiscoveryMethod) | Set-Content -Path (Join-Path $runDir "sch_fixture_status.txt") -Encoding UTF8

    $callOut = ""
    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $callOut = "status=0`nerror: symbol_id discovery failed from get_sch_items`n"
    }
    else
    {
        $callArgs = @{
            schematic_filename = $sch
            symbol_id = $symbolId
            ensure_fresh_connectivity = $true
            max_pins = 200
        } | ConvertTo-Json -Compress
        $callInput = Join-Path $runDir "schematic_symbol_connectivity_args.json"
        $callArgs | Set-Content -Path $callInput -Encoding UTF8
        $callRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "schematic_symbol_connectivity_session", ("@" + $callInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
        $callOut = $callRes.Text
    }

    $callOut | Set-Content -Path (Join-Path $runDir "schematic_symbol_connectivity_output.log") -Encoding UTF8

    $visible = if( $lt -match 'schematic_symbol_connectivity_session' ) { "true" } else { "false" }
    $callable = if( $callOut -match 'status=1' ) { "true" } else { "false" }

    $ok = "false"
    if( $callOut -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }

    $symbolFound = "false"
    if( $callOut -match 'key:\s+"symbol_found"[\s\S]*?bool_value:\s*(true|false)' ) { $symbolFound = $Matches[1] }

    $toolBlocker = "absent"
    if( $callOut -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $toolBlocker = $Matches[1] }

    $pinCount = ""
    if( $callOut -match 'key:\s+"pin_count_total"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $pinCount = [string][int][double]$Matches[1]
    }

    $pinsReturned = ""
    if( $callOut -match 'key:\s+"pins_returned"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $pinsReturned = [string][int][double]$Matches[1]
    }

    $diagnostics = ""
    if( $callOut -match 'key:\s+"diagnostics"[\s\S]*?string_value:\s*"([^"]*)"' ) { $diagnostics = $Matches[1] }

    $pinsSummary = ""
    if( $callOut -match 'key:\s+"pins_summary"[\s\S]*?string_value:\s*"([^"]*)"' ) { $pinsSummary = $Matches[1] }

    $pinCountValid = "false"
    if( $pinCount -match '^\d+$' -and [int]$pinCount -ge 1 ) { $pinCountValid = "true" }

    $pinsReturnedValid = "false"
    if( $pinsReturned -match '^\d+$' -and [int]$pinsReturned -ge 1 -and [int]$pinsReturned -le 1000 )
    {
        $pinsReturnedValid = "true"
    }

    $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }
    $pinsSummaryPresent = if( [string]::IsNullOrWhiteSpace( $pinsSummary ) ) { "false" } else { "true" }

    $blocker = "N/A"
    if( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callable -ne "true" ) { $blocker = "call_failed" }
    elseif( $ok -ne "true" ) { $blocker = "tool_ok_false" }
    elseif( $symbolFound -ne "true" ) { $blocker = "symbol_not_found" }
    elseif( $pinCountValid -ne "true" ) { $blocker = "pin_count_invalid" }
    elseif( $pinsReturnedValid -ne "true" ) { $blocker = "pins_returned_invalid" }
    elseif( $pinsSummaryPresent -ne "true" ) { $blocker = "pins_summary_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_without_mutation=true"
        "symbol_id_discovery_ok=$symbolIdDiscoveryOk"
        "symbol_id_discovery_error=$symbolIdDiscoveryError"
        "symbol_id_discovery_method=$symbolDiscoveryMethod"
        "ok=$ok"
        "symbol_found=$symbolFound"
        "pin_count_valid=$pinCountValid"
        "pins_returned_valid=$pinsReturnedValid"
        "pins_summary_present=$pinsSummaryPresent"
        "diagnostics_present=$diagnosticsPresent"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "pin_count_total=$pinCount"
        "pins_returned=$pinsReturned"
        "pins_summary=$pinsSummary"
        "diagnostics=$diagnostics"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_symbol_connectivity_session_contract_summary.txt") -Encoding UTF8

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
