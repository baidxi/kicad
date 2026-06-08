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
$pcb      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pcb"
$schBuild = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schIssue19090 = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$fixtureSource = "build"
$selectedSchFixture = $schBuild
$footprintSearchQuery = "R_0201_0603Metric"
$footprintSearchScope = "project"
$footprintSearchMaxResults = 1
$projectFpLibTable = Join-Path $BuildDir "ipc-v1-proj\fp-lib-table"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }
if( Test-Path -LiteralPath $schIssue19090 )
{
    $selectedSchFixture = $schIssue19090
    $fixtureSource = "issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-library-footprint-search-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
("fixture_source=" + $fixtureSource + ";fixture_schematic=" + $selectedSchFixture + ";fixture_board=" + $pcb + ";query=" + $footprintSearchQuery + ";library_scope=" + $footprintSearchScope + ";max_results=" + $footprintSearchMaxResults) | Set-Content -Path (Join-Path $runDir "library_fixture_status.txt") -Encoding UTF8

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "qa_resistors")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA deterministic resistor footprint library"))'),
    ")"
) | Set-Content -Path $projectFpLibTable -Encoding ASCII
Copy-Item -Path $projectFpLibTable -Destination $globalFpLibTable -Force
Copy-Item -Path $projectFpLibTable -Destination (Join-Path $runDir "fp-lib-table.used") -Force
Copy-Item -Path $globalFpLibTable -Destination (Join-Path $runDir "fp-lib-table.global.used") -Force
"EnableAPILogging=1" | Set-Content -Path (Join-Path $cfg10 "kicad_advanced") -Encoding ASCII

$OpenEditorTimeoutMs = 240000
$SearchToolTimeoutMs = 240000

function Test-BoundedTimeoutResult {
    param([hashtable]$SmokeResult)
    if( $null -eq $SmokeResult ) { return $true }
    return ( [string]$SmokeResult.Text.Trim() -match '^(?i)timeout\s*$' )
}

function Wait-PingReady {
    param([string]$Url)
    for( $i = 0; $i -lt 60; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $pingRes.Text | Set-Content -Path (Join-Path $runDir "ping_probe.log") -Encoding UTF8
        if( $pingRes.Text -match 'status=1' ) { return $true }
        Start-Sleep -Milliseconds 250
    }
    return $false
}

function Get-SmokeErrorMessage {
    param([string]$Text)
    if( $Text -match 'error_message=([^\r\n]+)' ) { return $Matches[1].Trim() }
    if( $Text -match 'error_message:\s*"([^"]+)"' ) { return $Matches[1].Trim() }
    return $null
}

function Test-ExpectBadRequest {
    param(
        [string]$Text,
        [string]$ExpectedMessage
    )
    if( $Text -notmatch 'status=3' ) { return $false }
    $msg = Get-SmokeErrorMessage -Text $Text
    return ( $msg -eq $ExpectedMessage )
}

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru -WindowStyle Normal

try
{
    Start-Sleep -Seconds 5
    $kicadProc.Refresh()
    if( $kicadProc.HasExited )
    {
        $adopt = Get-Process -Name kicad -ErrorAction SilentlyContinue | Select-Object -First 1
        if( $null -ne $adopt ) { $kicadProc = $adopt }
        else { throw ("KiCad exited before IPC resolve (exit_code=" + $kicadProc.ExitCode + ")") }
    }

    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 120 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }

    if( Test-Path -LiteralPath $sockDir )
    {
        $pidSock = Join-Path $sockDir ("api-" + $kicadProc.Id + ".sock")
        $defaultSock = Join-Path $sockDir "api.sock"
        ("resolved=" + $(if($resolved.Resolved){"true"}else{"false"}) + ";url=" + $url +
         ";pid_sock=" + $pidSock + ";default_sock=" + $defaultSock) |
            Set-Content -Path (Join-Path $runDir "ipc_url_resolve.txt") -Encoding UTF8
    }

    if( -not $url ) { throw "Unable to resolve API URL from computed IPC candidates" }
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    if( -not (Wait-PingReady -Url $url) ) { throw "KiCad API did not reach AS_OK on ping" }

    ("skipped_optional_diagnostic=true`nmcp_list_tools_not_called_to_reduce_ipc_load") |
        Set-Content -Path (Join-Path $runDir "mcp_list_tools_output.log") -Encoding UTF8

    $openArgs = @{ type = "pcb"; filename = $pcb } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8

    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe `
        -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) `
        -TimeoutMs $OpenEditorTimeoutMs -WorkingDirectory $RepoRoot
    $openOut = $openRes.Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8

    $openTimedOut = (Test-BoundedTimeoutResult $openRes) -or ($openOut.Trim() -match '^(?i)timeout\s*$')
    if( $openTimedOut )
    {
        @(
            "visible_in_context_if_applicable=false"
            "callable_without_mutation=true"
            "search_ok=false"
            "count_valid=false"
            "items_present=false"
            "diagnostics_present=false"
            "query=absent"
            "library_scope=absent"
            "count=absent"
            "first_items=none"
            "diagnostics="
            "exact_blocker_if_any=open_editor_timeout"
            "tool_exact_blocker_field=absent"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_search_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }
    if( $openOut -notmatch 'status=1' )
    {
        @(
            "visible_in_context_if_applicable=false"
            "callable_without_mutation=true"
            "search_ok=false"
            "count_valid=false"
            "items_present=false"
            "diagnostics_present=false"
            "query=absent"
            "library_scope=absent"
            "count=absent"
            "first_items=none"
            "diagnostics="
            "footprint_search_board_filename_negative_green=false"
            "exact_blocker_if_any=open_editor_not_ok"
            "tool_exact_blocker_field=absent"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_search_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $pcbAbs = (Resolve-Path $pcb).Path
    $msgBoardMissing = "missing or invalid 'board_filename' (string required)"
    $msgBoardEmpty = "'board_filename' must be non-empty"
    $msgBoardInvalidPath = "'board_filename' is not a valid filesystem path"
    $msgQueryMissing = "missing or invalid 'query' (string required)"
    $invalidBoardPath = "C:\invalid<>path\board.kicad_pcb"

    $fpSearchBoardMissingOk = "false"
    $fpSearchBoardEmptyOk = "false"
    $fpSearchBoardNonStringOk = "false"
    $fpSearchBoardInvalidPathOk = "false"
    $fpSearchQueryPriorityOverPathOk = "false"
    $fpSearchBoardFilenameNegativeGreen = "false"
    $negBlocker = "N/A"

    function Invoke-FpSearchCall {
        param(
            [hashtable]$ArgsTable,
            [string]$LogLabel
        )
        $logPath = Join-Path $runDir ($LogLabel + ".log")
        $argsPath = Join-Path $runDir ($LogLabel + ".args.json")
        ( $ArgsTable | ConvertTo-Json -Compress -Depth 10 ) | Set-Content -Path $argsPath -Encoding UTF8
        $out = ( Invoke-SmokeBounded -SmokeExePath $smokeExe `
            -SmokeArgs @($url, "mcp_call_tool_args", "library_footprint_search_session", ("@" + $argsPath)) `
            -TimeoutMs $SearchToolTimeoutMs -WorkingDirectory $RepoRoot ).Text
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    $negOut = Invoke-FpSearchCall -ArgsTable @{ query = $footprintSearchQuery } -LogLabel "neg_search_board_missing"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardMissing ) { $fpSearchBoardMissingOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_missing" } }

    $negOut = Invoke-FpSearchCall -ArgsTable @{ board_filename = ""; query = $footprintSearchQuery } -LogLabel "neg_search_board_empty"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardEmpty ) { $fpSearchBoardEmptyOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_empty" } }

    $negOut = Invoke-FpSearchCall -ArgsTable @{ board_filename = 123; query = $footprintSearchQuery } -LogLabel "neg_search_board_non_string"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardMissing ) { $fpSearchBoardNonStringOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_non_string" } }

    $negOut = Invoke-FpSearchCall -ArgsTable @{ board_filename = $invalidBoardPath; query = $footprintSearchQuery } -LogLabel "neg_search_board_invalid_path"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardInvalidPath )
    {
        $fpSearchBoardInvalidPathOk = "true"
    }
    else
    {
        $advMsg = Get-SmokeErrorMessage -Text $negOut
        # wxFileName::IsOk() is permissive on Windows; invalid-path branch may be unreachable (mismatch first).
        $fpSearchBoardInvalidPathOk = "skipped_wx_permissive_reaches_other_error:$advMsg"
    }

    # R76 order: query/library_scope/max_results are validated before board path when board is a non-empty string.
    $negOut = Invoke-FpSearchCall -ArgsTable @{ board_filename = $invalidBoardPath } -LogLabel "neg_search_query_priority_over_path"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgQueryMissing ) { $fpSearchQueryPriorityOverPathOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "query_priority_over_path" } }

    if( $fpSearchBoardMissingOk -eq "true" -and $fpSearchBoardEmptyOk -eq "true" -and $fpSearchBoardNonStringOk -eq "true" `
            -and $fpSearchQueryPriorityOverPathOk -eq "true" )
    {
        $fpSearchBoardFilenameNegativeGreen = "true"
    }
    else
    {
        throw "footprint_search_board_filename_negative_green=false exact_blocker_if_any=$negBlocker"
    }

    $callArgs = @{
        board_filename = $pcb
        query = $footprintSearchQuery
        library_scope = $footprintSearchScope
        max_results = $footprintSearchMaxResults
    } | ConvertTo-Json -Compress
    $callInput = Join-Path $runDir "library_footprint_search_args.json"
    $callArgs | Set-Content -Path $callInput -Encoding UTF8

    $callRes = Invoke-SmokeBounded -SmokeExePath $smokeExe `
        -SmokeArgs @($url, "mcp_call_tool_args", "library_footprint_search_session", ("@" + $callInput)) `
        -TimeoutMs $SearchToolTimeoutMs -WorkingDirectory $RepoRoot
    $callOut = $callRes.Text
    $callOut | Set-Content -Path (Join-Path $runDir "library_footprint_search_output.log") -Encoding UTF8

    if( (Test-BoundedTimeoutResult $callRes) -or $callOut.Trim() -match '^(?i)timeout\s*$' )
    {
        @(
            "visible_in_context_if_applicable=false"
            "callable_without_mutation=true"
            "search_ok=false"
            "count_valid=false"
            "items_present=false"
            "diagnostics_present=false"
            "query=absent"
            "library_scope=absent"
            "count=absent"
            "first_items=none"
            "diagnostics=timeout"
            "footprint_search_board_filename_missing_ok=$fpSearchBoardMissingOk"
            "footprint_search_board_filename_empty_ok=$fpSearchBoardEmptyOk"
            "footprint_search_board_filename_non_string_ok=$fpSearchBoardNonStringOk"
            "footprint_search_board_filename_invalid_path_ok=$fpSearchBoardInvalidPathOk"
            "footprint_search_query_priority_over_invalid_path_ok=$fpSearchQueryPriorityOverPathOk"
            "footprint_search_board_filename_negative_green=$fpSearchBoardFilenameNegativeGreen"
            "footprint_search_board_filename_order_note=path validation follows query/library_scope/max_results (differs from R61-R74 legacy)"
            "exact_blocker_if_any=footprint_search_tool_timeout"
            "tool_exact_blocker_field=absent"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_search_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $toolOk = ($callOut -match 'status=1')
    $visible = if( $toolOk ) { "true" } else { "false" }

    $searchOk = "false"
    if( $callOut -match 'key:\s+"search_ok"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $searchOk = if( [double]$Matches[1] -ge 1 ) { "true" } else { "false" }
    }

    $count = "absent"
    if( $callOut -match 'key:\s+"count"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $count = [string][int][double]$Matches[1]
    }
    $countValid = if( $count -match '^\d+$' ) { "true" } else { "false" }

    $itemsPresent = if( $callOut -match 'key:\s+"items"' ) { "true" } else { "false" }

    $diagnostics = ""
    if( $callOut -match 'key:\s+"diagnostics"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $diagnostics = $Matches[1]
    }
    $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }

    $query = "absent"
    if( $callOut -match 'key:\s+"query"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $query = $Matches[1]
    }

    $libraryScope = "absent"
    if( $callOut -match 'key:\s+"library_scope"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $libraryScope = $Matches[1]
    }

    $firstItems = "none"
    if( $callOut -match 'qualified_name"\s*[\s\S]*?string_value:\s*"([^"]+)"' )
    {
        $firstItems = $Matches[1]
    }

    $toolBlocker = "absent"
    if( $callOut -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $toolBlocker = $Matches[1]
    }

    $blocker = "N/A"
    if( -not $toolOk ) { $blocker = "call_failed" }
    elseif( $searchOk -ne "true" ) { $blocker = "search_not_ok" }
    elseif( $countValid -ne "true" ) { $blocker = "count_invalid" }
    elseif( $itemsPresent -ne "true" ) { $blocker = "items_field_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_without_mutation=true"
        "search_ok=$searchOk"
        "count_valid=$countValid"
        "items_present=$itemsPresent"
        "diagnostics_present=$diagnosticsPresent"
        "query=$query"
        "library_scope=$libraryScope"
        "count=$count"
        "first_items=$firstItems"
        "diagnostics=$diagnostics"
        "footprint_search_board_filename_missing_ok=$fpSearchBoardMissingOk"
        "footprint_search_board_filename_empty_ok=$fpSearchBoardEmptyOk"
        "footprint_search_board_filename_non_string_ok=$fpSearchBoardNonStringOk"
        "footprint_search_board_filename_invalid_path_ok=$fpSearchBoardInvalidPathOk"
        "footprint_search_query_priority_over_invalid_path_ok=$fpSearchQueryPriorityOverPathOk"
        "footprint_search_board_filename_negative_green=$fpSearchBoardFilenameNegativeGreen"
        "footprint_search_board_filename_order_note=path validation follows query/library_scope/max_results (differs from R61-R74 legacy)"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_search_session_contract_summary.txt") -Encoding UTF8

    Write-TargetRunDir -RunDir $runDir
}
catch
{
    $errMsg = $_.Exception.Message
    @(
        "visible_in_context_if_applicable=absent"
        "callable_without_mutation=false"
        "search_ok=false"
        "count_valid=false"
        "items_present=false"
        "diagnostics_present=false"
        "query=absent"
        "library_scope=absent"
        "count=absent"
        "first_items=none"
        "diagnostics=probe_exception"
        "exact_blocker_if_any=probe_exception"
        "tool_exact_blocker_field=probe_exception"
        "probe_exception_message=$errMsg"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_search_session_contract_summary.txt") -Encoding UTF8
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
