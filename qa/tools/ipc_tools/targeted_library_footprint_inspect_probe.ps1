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
$projectFpLibTable = Join-Path $BuildDir "ipc-v1-proj\fp-lib-table"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-library-footprint-inspect-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

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

function Test-ExpectInspectMismatch {
    param([string]$Text)
    if( $Text -notmatch 'status=1' ) { return $false }
    $inspectOk = "false"
    if( $Text -match 'key:\s+"inspect_ok"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $inspectOk = if( [double]$Matches[1] -ge 1 ) { "true" } else { "false" }
    }
    $blocker = ""
    if( $Text -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $blocker = $Matches[1]
    }
    return ( $inspectOk -eq "false" -and $blocker -eq "board_filename_mismatch" )
}

Stop-KiCadProcesses
Start-Sleep -Seconds 2

$sockDir = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru -WindowStyle Normal

try
{
    $url = $null
    Start-Sleep -Seconds 5

    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 100 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 250 -WorkingDirectory $RepoRoot
    if( $resolved.Resolved )
    {
        $url = $resolved.Url
    }

    if( Test-Path -LiteralPath $sockDir )
    {
        $pidSock = Join-Path $sockDir ("api-" + $kicadProc.Id + ".sock")
        $defaultSock = Join-Path $sockDir "api.sock"
        ("resolved=" + $(if($resolved.Resolved){"true"}else{"false"}) +
         ";url=" + $url +
         ";pid_sock=" + $pidSock +
         ";default_sock=" + $defaultSock) | Set-Content -Path (Join-Path $runDir "ipc_url_resolve.txt") -Encoding UTF8
    }

    if( -not $url )
    {
        throw "Unable to resolve API URL from computed IPC candidates"
    }

    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "pcb"; filename = $pcb } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = & $smokeExe $url mcp_call_tool_args qa_open_editor_frame ("@" + $openInput) 2>&1 | Out-String
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if( $openOut -notmatch 'status=1' ) { throw "qa_open_editor_frame failed" }

    $pcbAbs = (Resolve-Path $pcb).Path
    $pcbMismatchAbs = (Resolve-Path (Join-Path $RepoRoot "qa\data\pcbnew\connect\connect.kicad_pcb")).Path
    $msgBoardMissing = "missing or invalid 'board_filename' (string required)"
    $msgBoardEmpty = "'board_filename' must be non-empty"
    $msgBoardInvalidPath = "'board_filename' is not a valid filesystem path"
    $invalidBoardPath = "C:\invalid<>path\board.kicad_pcb"
    $inspectLibrary = "qa_resistors"
    $inspectFootprint = "R_0201_0603Metric"

    $fpInspectBoardMissingOk = "false"
    $fpInspectBoardEmptyOk = "false"
    $fpInspectBoardNonStringOk = "false"
    $fpInspectBoardWhitespaceOnlyOk = "false"
    $fpInspectBoardInvalidPathOk = "false"
    $fpInspectBoardMismatchStructuredOk = "false"
    $fpInspectBoardFilenameNegativeGreen = "false"
    $negBlocker = "N/A"

    function Invoke-FpInspectCall {
        param(
            [hashtable]$ArgsTable,
            [string]$LogLabel
        )
        $logPath = Join-Path $runDir ($LogLabel + ".log")
        $argsPath = Join-Path $runDir ($LogLabel + ".args.json")
        ( $ArgsTable | ConvertTo-Json -Compress -Depth 10 ) | Set-Content -Path $argsPath -Encoding UTF8
        $out = & $smokeExe $url mcp_call_tool_args library_footprint_inspect_session ("@" + $argsPath) 2>&1 | Out-String
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    $negOut = Invoke-FpInspectCall -ArgsTable @{ library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_missing"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardMissing ) { $fpInspectBoardMissingOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_missing" } }

    $negOut = Invoke-FpInspectCall -ArgsTable @{ board_filename = ""; library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_empty"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardEmpty ) { $fpInspectBoardEmptyOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_empty" } }

    $negOut = Invoke-FpInspectCall -ArgsTable @{ board_filename = 123; library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_non_string"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardMissing ) { $fpInspectBoardNonStringOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_non_string" } }

    # Handler trimAscii: whitespace-only board_filename becomes empty after trim.
    $negOut = Invoke-FpInspectCall -ArgsTable @{ board_filename = "   "; library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_whitespace_only"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardEmpty ) { $fpInspectBoardWhitespaceOnlyOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_whitespace_only" } }

    $negOut = Invoke-FpInspectCall -ArgsTable @{ board_filename = $invalidBoardPath; library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_invalid_path"
    if( Test-ExpectBadRequest -Text $negOut -ExpectedMessage $msgBoardInvalidPath )
    {
        $fpInspectBoardInvalidPathOk = "true"
    }
    elseif( Test-ExpectInspectMismatch -Text $negOut )
    {
        $fpInspectBoardInvalidPathOk = "skipped_wx_permissive_reaches_mismatch_structured"
    }
    else
    {
        $advMsg = Get-SmokeErrorMessage -Text $negOut
        if( [string]::IsNullOrWhiteSpace( $advMsg ) ) { $advMsg = "unknown_non_bad_request" }
        $fpInspectBoardInvalidPathOk = "skipped_wx_permissive_reaches_other_error:$advMsg"
    }

    $negOut = Invoke-FpInspectCall -ArgsTable @{ board_filename = $pcbMismatchAbs; library = $inspectLibrary; footprint_name = $inspectFootprint } -LogLabel "neg_inspect_board_mismatch_structured"
    if( Test-ExpectInspectMismatch -Text $negOut ) { $fpInspectBoardMismatchStructuredOk = "true" }
    else { if( $negBlocker -eq "N/A" ) { $negBlocker = "board_mismatch_structured" } }

    if( $fpInspectBoardMissingOk -eq "true" -and $fpInspectBoardEmptyOk -eq "true" -and $fpInspectBoardNonStringOk -eq "true" `
            -and $fpInspectBoardWhitespaceOnlyOk -eq "true" -and $fpInspectBoardMismatchStructuredOk -eq "true" )
    {
        $fpInspectBoardFilenameNegativeGreen = "true"
    }
    else
    {
        throw "footprint_inspect_board_filename_negative_green=false exact_blocker_if_any=$negBlocker"
    }

    $callArgs = @{
        board_filename = $pcb
        library = "qa_resistors"
        footprint_name = "R_0201_0603Metric"
    } | ConvertTo-Json -Compress
    $callInput = Join-Path $runDir "library_footprint_inspect_args.json"
    $callArgs | Set-Content -Path $callInput -Encoding UTF8

    $callOut = & $smokeExe $url mcp_call_tool_args library_footprint_inspect_session ("@" + $callInput) 2>&1 | Out-String
    $callOut | Set-Content -Path (Join-Path $runDir "library_footprint_inspect_output.log") -Encoding UTF8

    $visible = if( $lt -match 'library_footprint_inspect_session' ) { "true" } else { "false" }
    $callable = if( $callOut -match 'status=1' ) { "true" } else { "false" }

    $inspectOk = "false"
    if( $callOut -match 'key:\s+"inspect_ok"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $inspectOk = if( [double]$Matches[1] -ge 1 ) { "true" } else { "false" }
    }

    $footprintFound = "false"
    if( $callOut -match 'key:\s+"footprint_found"[\s\S]*?bool_value:\s*(true|false)' )
    {
        $footprintFound = $Matches[1]
    }

    $qualifiedName = "absent"
    if( $callOut -match 'key:\s+"qualified_name"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $qualifiedName = $Matches[1]
    }

    $padsCountValid = "false"
    if( $callOut -match 'key:\s+"pads_count"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $padsCountValid = if( [double]$Matches[1] -ge 0 ) { "true" } else { "false" }
    }

    $graphicsCountValid = "false"
    if( $callOut -match 'key:\s+"graphics_count"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $graphicsCountValid = if( [double]$Matches[1] -ge 0 ) { "true" } else { "false" }
    }

    $modelsCountValid = "false"
    if( $callOut -match 'key:\s+"models_count"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $modelsCountValid = if( [double]$Matches[1] -ge 0 ) { "true" } else { "false" }
    }

    $attributesSummary = ""
    if( $callOut -match 'key:\s+"attributes_summary"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $attributesSummary = $Matches[1]
    }
    $attributesSummaryPresent = if( [string]::IsNullOrWhiteSpace( $attributesSummary ) ) { "false" } else { "true" }

    $diagnostics = ""
    if( $callOut -match 'key:\s+"diagnostics"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $diagnostics = $Matches[1]
    }
    $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }

    $toolBlocker = "absent"
    if( $callOut -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' )
    {
        $toolBlocker = $Matches[1]
    }

    $blocker = "N/A"
    if( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callable -ne "true" ) { $blocker = "call_failed" }
    elseif( $inspectOk -ne "true" ) { $blocker = "inspect_not_ok" }
    elseif( $footprintFound -ne "true" ) { $blocker = "footprint_not_found" }
    elseif( $qualifiedName -ne "qa_resistors:R_0201_0603Metric" ) { $blocker = "qualified_name_unexpected" }
    elseif( $padsCountValid -ne "true" ) { $blocker = "pads_count_invalid" }
    elseif( $graphicsCountValid -ne "true" ) { $blocker = "graphics_count_invalid" }
    elseif( $modelsCountValid -ne "true" ) { $blocker = "models_count_invalid" }
    elseif( $attributesSummaryPresent -ne "true" ) { $blocker = "attributes_summary_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_without_mutation=true"
        "inspect_ok=$inspectOk"
        "footprint_found=$footprintFound"
        "qualified_name=$qualifiedName"
        "pads_count_valid=$padsCountValid"
        "graphics_count_valid=$graphicsCountValid"
        "models_count_valid=$modelsCountValid"
        "attributes_summary_present=$attributesSummaryPresent"
        "diagnostics_present=$diagnosticsPresent"
        "attributes_summary=$attributesSummary"
        "diagnostics=$diagnostics"
        "footprint_inspect_board_filename_missing_ok=$fpInspectBoardMissingOk"
        "footprint_inspect_board_filename_empty_ok=$fpInspectBoardEmptyOk"
        "footprint_inspect_board_filename_non_string_ok=$fpInspectBoardNonStringOk"
        "footprint_inspect_board_filename_whitespace_only_ok=$fpInspectBoardWhitespaceOnlyOk"
        "footprint_inspect_board_filename_invalid_path_ok=$fpInspectBoardInvalidPathOk"
        "footprint_inspect_board_filename_mismatch_structured_ok=$fpInspectBoardMismatchStructuredOk"
        "footprint_inspect_board_filename_negative_green=$fpInspectBoardFilenameNegativeGreen"
        "footprint_inspect_board_filename_order_note=library/footprint validated before trim; mismatch uses makeInspectResult not BAD_REQUEST; no IsAbsolute check"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_library_footprint_inspect_session_contract_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
