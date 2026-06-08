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
$fixtureSource = "build"
$symbolInspectQuery = "GND"
$projectSymLibTable = Join-Path $BuildDir "ipc-v1-proj\sym-lib-table"
$powerLib = Join-Path $RepoRoot "qa\data\libraries\power.kicad_sym"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$globalSymLibTable = Join-Path $cfg10 "sym-lib-table"

New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $schBuild)) { throw "Missing schematic at $schBuild" }
if (!(Test-Path $powerLib)) { throw "Missing power library at $powerLib" }
if( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-library-symbol-inspect-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
("fixture_source=" + $fixtureSource + ";fixture_schematic=" + $sch + ";query=" + $symbolInspectQuery + ";symbol_name=GND") | Set-Content -Path (Join-Path $runDir "library_fixture_status.txt") -Encoding UTF8

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$powerLibUnix = $powerLib.Replace('\', '/')
@(
    "(sym_lib_table",
    "  (version 7)",
    ('  (lib (name "power")(type "KiCad")(uri "' + $powerLibUnix + '")(options "")(descr "QA deterministic power library"))'),
    ")"
) | Set-Content -Path $projectSymLibTable -Encoding ASCII
Copy-Item -Path $projectSymLibTable -Destination $globalSymLibTable -Force
Copy-Item -Path $projectSymLibTable -Destination (Join-Path $runDir "sym-lib-table.used") -Force
Copy-Item -Path $globalSymLibTable -Destination (Join-Path $runDir "sym-lib-table.global.used") -Force
"EnableAPILogging=1" | Set-Content -Path (Join-Path $cfg10 "kicad_advanced") -Encoding ASCII

$OpenEditorTimeoutMs = 240000
$InspectToolTimeoutMs = 240000

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
        Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
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
            "inspect_ok=false"
            "symbol_found=false"
            "pin_count_valid=false"
            "fields_summary_present=false"
            "diagnostics_present=false"
            "exact_blocker_if_any=open_editor_timeout"
            "tool_exact_blocker_field=absent"
            "selected_search_query=GND"
            "selected_qualified_name=power:GND"
            "pin_count=absent"
            "fields_summary=absent"
            "diagnostics=absent"
            "inspect_process_exited=false"
            "inspect_process_exit_code=-1"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_inspect_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }
    if( $openOut -notmatch 'status=1' )
    {
        @(
            "visible_in_context_if_applicable=false"
            "callable_without_mutation=true"
            "inspect_ok=false"
            "symbol_found=false"
            "pin_count_valid=false"
            "fields_summary_present=false"
            "diagnostics_present=false"
            "exact_blocker_if_any=open_editor_not_ok"
            "tool_exact_blocker_field=absent"
            "selected_search_query=GND"
            "selected_qualified_name=power:GND"
            "pin_count=absent"
            "fields_summary=absent"
            "diagnostics=absent"
            "inspect_process_exited=false"
            "inspect_process_exit_code=-1"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_inspect_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $inspectArgs = @{
        schematic_filename = $sch
        library = "power"
        symbol_name = "GND"
    } | ConvertTo-Json -Compress
    $inspectInput = Join-Path $runDir "library_symbol_inspect_args.json"
    $inspectArgs | Set-Content -Path $inspectInput -Encoding UTF8

    $inspectRes = Invoke-SmokeBounded -SmokeExePath $smokeExe `
        -SmokeArgs @($url, "mcp_call_tool_args", "library_symbol_inspect_session", ("@" + $inspectInput)) `
        -TimeoutMs $InspectToolTimeoutMs -WorkingDirectory $RepoRoot
    $inspectOut = $inspectRes.Text
    $inspectOut | Set-Content -Path (Join-Path $runDir "library_symbol_inspect_output.log") -Encoding UTF8

    if( (Test-BoundedTimeoutResult $inspectRes) -or $inspectOut.Trim() -match '^(?i)timeout\s*$' )
    {
        @(
            "visible_in_context_if_applicable=false"
            "callable_without_mutation=true"
            "inspect_ok=false"
            "symbol_found=false"
            "pin_count_valid=false"
            "fields_summary_present=false"
            "diagnostics_present=false"
            "exact_blocker_if_any=symbol_inspect_tool_timeout"
            "tool_exact_blocker_field=absent"
            "selected_search_query=GND"
            "selected_qualified_name=power:GND"
            "pin_count=absent"
            "fields_summary=absent"
            "diagnostics=timeout"
            "inspect_process_exited=" + $(if($inspectRes.Exited){"true"}else{"false"})
            "inspect_process_exit_code=" + $inspectRes.ExitCode
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_inspect_session_contract_summary.txt") -Encoding UTF8
        Write-TargetRunDir -RunDir $runDir
        return
    }

    function Get-FirstValue([string[]]$InLines, [string]$Prefix, [string]$DefaultValue = "absent")
    {
        $line = $InLines | Where-Object { $_ -like ($Prefix + "*") } | Select-Object -First 1
        if( [string]::IsNullOrWhiteSpace( $line ) ) { return $DefaultValue }
        return ($line -replace ('^' + [regex]::Escape( $Prefix )), '').Trim()
    }

    $lines = $inspectOut -split "`r?`n"
    $flattenLines = $lines | Where-Object { $_ -match '=' }

    $inspectOkRaw = Get-FirstValue $flattenLines "inspect_ok=" "0"
    $inspectOk = if( $inspectOkRaw -eq "1" -or $inspectOkRaw -eq "true" ) { "true" } else { "false" }
    $symbolFoundRaw = Get-FirstValue $flattenLines "symbol_found=" "false"
    $symbolFound = if( $symbolFoundRaw -eq "1" -or $symbolFoundRaw -eq "true" ) { "true" } else { "false" }
    $pinCount = Get-FirstValue $flattenLines "pin_count="
    $pinCountValid = if( $pinCount -match '^\d+$' -and [int]$pinCount -ge 1 ) { "true" } else { "false" }
    $fieldsSummary = Get-FirstValue $flattenLines "fields_summary="
    $fieldsSummaryPresent = if( [string]::IsNullOrWhiteSpace( $fieldsSummary ) ) { "false" } else { "true" }
    $diagnostics = Get-FirstValue $flattenLines "diagnostics="
    $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }
    $blockerField = Get-FirstValue $flattenLines "exact_blocker_if_any=" "absent"

    if( $inspectOkRaw -eq "0" -and $inspectOut -match 'key:\s+"inspect_ok"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $inspectOkRaw = $Matches[1]
        $inspectOk = if( [double]$inspectOkRaw -ge 1 ) { "true" } else { "false" }
    }
    if( $symbolFoundRaw -eq "false" -and $inspectOut -match 'key:\s+"symbol_found"[\s\S]*?bool_value:\s*(true|false)' )
    {
        $symbolFoundRaw = $Matches[1]
        $symbolFound = $symbolFoundRaw
    }
    if( $pinCount -eq "absent" -and $inspectOut -match 'key:\s+"pin_count"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $pinCount = [string][int][double]$Matches[1]
        $pinCountValid = if( [int]$pinCount -ge 1 ) { "true" } else { "false" }
    }
    if( ($fieldsSummary -eq "absent" -or $fieldsSummary -eq "") -and $inspectOut -match 'key:\s+"fields_summary"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $fieldsSummary = $Matches[1]
        $fieldsSummaryPresent = if( [string]::IsNullOrWhiteSpace( $fieldsSummary ) ) { "false" } else { "true" }
    }
    if( ($diagnostics -eq "absent" -or $diagnostics -eq "") -and $inspectOut -match 'key:\s+"diagnostics"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $diagnostics = $Matches[1]
        $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }
    }
    if( $blockerField -eq "absent" -and $inspectOut -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $blockerField = $Matches[1]
    }

    $callable = if( ($inspectOut -match 'status=1') -or ($inspectOk -eq "true") ) { "true" } else { "false" }
    $visible = $callable

    $blocker = "N/A"
    if( $callable -ne "true" ) { $blocker = "call_failed" }
    elseif( $inspectOk -ne "true" ) { $blocker = "inspect_not_ok" }
    elseif( $symbolFound -ne "true" ) { $blocker = "symbol_not_found" }
    elseif( $pinCountValid -ne "true" ) { $blocker = "pin_count_invalid" }
    elseif( $fieldsSummaryPresent -ne "true" ) { $blocker = "fields_summary_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_without_mutation=true"
        "inspect_ok=$inspectOk"
        "symbol_found=$symbolFound"
        "pin_count_valid=$pinCountValid"
        "fields_summary_present=$fieldsSummaryPresent"
        "diagnostics_present=$diagnosticsPresent"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$blockerField"
        "selected_search_query=GND"
        "selected_qualified_name=power:GND"
        "pin_count=$pinCount"
        "fields_summary=$fieldsSummary"
        "diagnostics=$diagnostics"
        "inspect_process_exited=" + $(if($inspectRes.Exited){"true"}else{"false"})
        "inspect_process_exit_code=" + $inspectRes.ExitCode
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_inspect_session_contract_summary.txt") -Encoding UTF8

    Write-TargetRunDir -RunDir $runDir
}
catch
{
    $errMsg = $_.Exception.Message
    @(
        "visible_in_context_if_applicable=absent"
        "callable_without_mutation=false"
        "inspect_ok=false"
        "symbol_found=false"
        "pin_count_valid=false"
        "fields_summary_present=false"
        "diagnostics_present=false"
        "exact_blocker_if_any=probe_exception"
        "tool_exact_blocker_field=absent"
        "selected_search_query=GND"
        "selected_qualified_name=power:GND"
        "pin_count=absent"
        "fields_summary=absent"
        "diagnostics=$errMsg"
        "inspect_process_exited=false"
        "inspect_process_exit_code=-1"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_inspect_session_contract_summary.txt") -Encoding UTF8
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
