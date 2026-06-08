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
$symbolSearchQuery = "GND"
$symbolSearchScope = "project"
$symbolSearchMaxResults = 1
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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-library-symbol-search-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
("fixture_source=" + $fixtureSource + ";fixture_schematic=" + $sch + ";query=" + $symbolSearchQuery + ";library_scope=" + $symbolSearchScope + ";max_results=" + $symbolSearchMaxResults) | Set-Content -Path (Join-Path $runDir "library_fixture_status.txt") -Encoding UTF8

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

function Write-SymbolSearchSummary {
    param(
        [string]$Visible,
        [string]$SearchOk,
        [string]$CountValid,
        [string]$ItemsPresent,
        [string]$DiagnosticsPresent,
        [string]$SourceContext,
        [string]$SearchStrategy,
        [string]$Query,
        [string]$LibraryScope,
        [string]$Count,
        [string]$FirstItems,
        [string]$Diagnostics,
        [string]$Blocker
    )
    @(
        "visible_in_context_if_applicable=$Visible"
        "callable_without_mutation=true"
        "search_ok=$SearchOk"
        "count_valid=$CountValid"
        "items_present=$ItemsPresent"
        "diagnostics_present=$DiagnosticsPresent"
        "source_context=$SourceContext"
        "search_strategy=$SearchStrategy"
        "query=$Query"
        "library_scope=$LibraryScope"
        "count=$Count"
        "first_items=$FirstItems"
        "diagnostics=$Diagnostics"
        "exact_blocker_if_any=$Blocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_search_session_contract_summary.txt") -Encoding UTF8
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
        Write-SymbolSearchSummary -Visible "false" -SearchOk "false" -CountValid "false" `
            -ItemsPresent "absent" -DiagnosticsPresent "absent" `
            -SourceContext "absent" -SearchStrategy "absent" -Query "absent" -LibraryScope "absent" `
            -Count "absent" -FirstItems "absent" -Diagnostics "absent" `
            -Blocker "open_editor_timeout"
        Write-TargetRunDir -RunDir $runDir
        return
    }
    if( $openOut -notmatch 'status=1' )
    {
        Write-SymbolSearchSummary -Visible "false" -SearchOk "false" -CountValid "false" `
            -ItemsPresent "absent" -DiagnosticsPresent "absent" `
            -SourceContext "absent" -SearchStrategy "absent" -Query "absent" -LibraryScope "absent" `
            -Count "absent" -FirstItems "absent" -Diagnostics "absent" `
            -Blocker "open_editor_not_ok"
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $callArgs = @{
        schematic_filename = $sch
        query = $symbolSearchQuery
        library_scope = $symbolSearchScope
        max_results = $symbolSearchMaxResults
    } | ConvertTo-Json -Compress
    $callInput = Join-Path $runDir "library_symbol_search_args.json"
    $callArgs | Set-Content -Path $callInput -Encoding UTF8

    $callRes = Invoke-SmokeBounded -SmokeExePath $smokeExe `
        -SmokeArgs @($url, "mcp_call_tool_args", "library_symbol_search_session", ("@" + $callInput)) `
        -TimeoutMs $SearchToolTimeoutMs -WorkingDirectory $RepoRoot
    $callOut = $callRes.Text
    $callOut | Set-Content -Path (Join-Path $runDir "library_symbol_search_output.log") -Encoding UTF8

    $toolOk = ($callOut -match 'status=1')
    $visible = if( $toolOk ) { "true" } else { "false" }

    $lines = $callOut -split "`r?`n"
    function Get-FirstValue([string]$Prefix, [string]$DefaultValue = "absent")
    {
        $line = $lines | Where-Object { $_ -like ($Prefix + "*") } | Select-Object -First 1
        if( [string]::IsNullOrWhiteSpace( $line ) ) { return $DefaultValue }
        return ($line -replace ('^' + [regex]::Escape( $Prefix )), '').Trim()
    }

    $searchOk = if( (Get-FirstValue "search_ok=" "0") -eq "1" ) { "true" } else { "false" }
    $count = Get-FirstValue "count="
    $countValid = if( $count -match '^\d+$' ) { "true" } else { "false" }
    $itemsPresent = Get-FirstValue "items_present="
    $diagnosticsPresent = Get-FirstValue "diagnostics_present="
    $query = Get-FirstValue "query="
    $libraryScope = Get-FirstValue "library_scope="
    $firstItems = Get-FirstValue "first_items="
    $diagnostics = Get-FirstValue "diagnostics="

    $sourceContext = "absent"
    if( $callOut -match 'source_context=([^;\r\n]+)' ) { $sourceContext = $Matches[1].Trim() }
    $searchStrategy = "absent"
    if( $callOut -match 'search_strategy=([^;\r\n]+)' ) { $searchStrategy = $Matches[1].Trim() }

    if( (Test-BoundedTimeoutResult $callRes) -or $callOut.Trim() -match '^(?i)timeout\s*$' )
    {
        Write-SymbolSearchSummary -Visible $visible -SearchOk "false" -CountValid "false" `
            -ItemsPresent "absent" -DiagnosticsPresent "absent" `
            -SourceContext "absent" -SearchStrategy "absent" -Query "absent" -LibraryScope "absent" `
            -Count "absent" -FirstItems "absent" -Diagnostics "timeout" `
            -Blocker "symbol_search_tool_timeout"
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $blocker = "N/A"
    if( -not $toolOk ) { $blocker = "call_failed" }
    elseif( $searchOk -ne "true" ) { $blocker = "search_not_ok" }
    elseif( $countValid -ne "true" ) { $blocker = "count_invalid" }
    elseif( $itemsPresent -ne "true" ) { $blocker = "items_field_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    Write-SymbolSearchSummary -Visible $visible -SearchOk $searchOk -CountValid $countValid `
        -ItemsPresent $itemsPresent -DiagnosticsPresent $diagnosticsPresent `
        -SourceContext $sourceContext -SearchStrategy $searchStrategy -Query $query -LibraryScope $libraryScope `
        -Count $count -FirstItems $firstItems -Diagnostics $diagnostics -Blocker $blocker

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
        "source_context=absent"
        "search_strategy=absent"
        "query=absent"
        "library_scope=absent"
        "count=absent"
        "first_items=absent"
        "diagnostics=probe_exception"
        "exact_blocker_if_any=probe_exception"
        "probe_exception_message=$errMsg"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_symbol_search_session_contract_summary.txt") -Encoding UTF8
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
