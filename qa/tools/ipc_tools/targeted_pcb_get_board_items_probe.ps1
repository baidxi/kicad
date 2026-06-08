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
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-get-board-items-session-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

@(
    "captured_at=$( Get-Date -Format 'o' )"
    "PWD=$(( Get-Location ).Path)"
    "PATH=$env:PATH"
    "KICAD_RUN_FROM_BUILD_DIR=$env:KICAD_RUN_FROM_BUILD_DIR"
    "KICAD_CONFIG_HOME=$env:KICAD_CONFIG_HOME"
    "KICAD_IPC_AUTOMATION=$env:KICAD_IPC_AUTOMATION"
    "KICAD_IPC_STARTUP_DIAG=$env:KICAD_IPC_STARTUP_DIAG"
    "PYTHONHOME=$env:PYTHONHOME"
    ""
) | Set-Content -Path (Join-Path $runDir "harness_runtime_env.txt") -Encoding UTF8

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

function Parse-PcbBoardItemsSessionOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,800}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $bf = ""
    if ($Text -match 'key:\s+"board_filename"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $bf = $Matches[1] }
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,800}?number_value:\s*([0-9]+(?:\.[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    $trunc = "false"
    if ($Text -match 'key:\s+"items_summary_truncated"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trunc = $Matches[1] }
    return @{
        status_ok       = ($Text -match 'status=1')
        ok              = $ok
        blocker         = $blocker
        board_filename  = $bf
        footprint_count = & $parseNum "footprint_count"
        pad_count       = & $parseNum "pad_count"
        track_count     = & $parseNum "track_count"
        via_count       = & $parseNum "via_count"
        zone_count      = & $parseNum "zone_count"
        drawing_count   = & $parseNum "drawing_count"
        items_summary_count = & $parseNum "items_summary_count"
        items_summary_truncated = $trunc
        raw             = $Text
    }
}

function Write-VisibilityFile {
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
    ) | Set-Content -Path (Join-Path $runDir "pcb_get_board_items_tool_visibility_status.txt") -Encoding UTF8
}

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if ($resolved.Resolved) { $resolved.Url } else { $null }
    if (-not $url) { throw "Unable to resolve API URL" }

    $ping = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
    $pingOk = if ($ping.Text -match 'status=1') { "true" } else { "false" }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "MCP get_project_info did not report loaded=true before qa_open_editor_frame"
    }

    $lt = ""
    $toolsSucceeded = $false
    $toolsTimedOut = $false
    $listToolsFinalStatus = "not_run"
    $toolVisibilityMode = "list_tools"
    $toolVisible = "unknown"
    $directCallAttempted = "false"
    $directCallStatus = "not_run"

    $fixtureSetupOk = "true"
    $boardOpenOk = "false"
    $callable = "false"
    $defaultCallOk = "false"
    $countsOk = "false"
    $itemsSummaryOk = "false"
    $maxItemsCallOk = "false"
    $maxItemsTruncationOk = "false"
    $negInvalidOk = "false"
    $negMismatchOk = "false"

    $exactBlockerSummary = "N/A"
    $toolExactBlockerField = "N/A"
    $summaryBoardFn = ""
    $sf = ""; $pf = ""; $tf = ""; $vf = ""; $zf = ""; $df = ""; $isc = ""; $istrunc = "false"

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if ($openOut -match 'status=1') { $boardOpenOk = "true"; Start-Sleep -Seconds 3 }
    else {
        $fixtureSetupOk = "false"
        $listToolsFinalStatus = "skipped_board_not_open"
        Write-VisibilityFile -PingOk $pingOk -ListToolsFinalStatus $listToolsFinalStatus `
            -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
            -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus
        @(
            "visible_in_context_if_applicable=$toolVisible"
            "callable=$callable"
            "fixture_setup_ok=$fixtureSetupOk"
            "board_open_ok=$boardOpenOk"
            "default_call_ok=$defaultCallOk"
            "counts_ok=$countsOk"
            "items_summary_ok=$itemsSummaryOk"
            "max_items_call_ok=$maxItemsCallOk"
            "max_items_truncation_ok=$maxItemsTruncationOk"
            "negative_invalid_item_type_ok=$negInvalidOk"
            "negative_board_filename_mismatch_ok=$negMismatchOk"
            "exact_blocker_if_any=qa_open_editor_frame_failed"
            "tool_exact_blocker_field=N/A"
            "board_filename="
            "footprint_count="
            "pad_count="
            "track_count="
            "via_count="
            "zone_count="
            "drawing_count="
            "items_summary_count="
            "items_summary_truncated="
            "fixture_source=ipc_v1"
            "fixture_board=$pcbAbs"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_board_items_session_contract_summary.txt") -Encoding UTF8
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        return
    }

    for ($i = 1; $i -le 3; $i++) {
        $ltRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_list_tools") -TimeoutMs 120000 -WorkingDirectory $RepoRoot
        $lt = $ltRes.Text
        $lt | Set-Content -Path (Join-Path $runDir ("mcp_list_tools_attempt" + $i + ".log")) -Encoding UTF8
        if ($lt -match '^(?i)\s*timeout\s*$') { $toolsTimedOut = $true; continue }
        $toolsSucceeded = $true
        break
    }
    if (-not $toolsSucceeded) {
        if ($toolsTimedOut) { $listToolsFinalStatus = "timeout"; $toolVisibilityMode = "direct_call_fallback" }
        else { $listToolsFinalStatus = "failed" }
    }
    else {
        $listToolsFinalStatus = "ok"
        if ($lt -match "pcb_get_board_items_session") { $toolVisible = "true" }
        else {
            $toolVisible = "false"
            $toolVisibilityMode = "direct_call_fallback"
        }
    }

    function Invoke-Session {
        param([hashtable]$ArgsTable, [string]$LogName)
        $logPath = Join-Path $runDir $LogName
        $argsPath = Join-Path $runDir ($LogName + ".args.json")
        ($ArgsTable | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
        $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $argsPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot).Text
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    $defaultOut = Invoke-Session -ArgsTable @{ board_filename = $pcbAbs } -LogName "pcb_get_board_items_session_default.log"
    $def = Parse-PcbBoardItemsSessionOutput -Text $defaultOut

    if ($toolVisibilityMode -eq "direct_call_fallback") {
        $directCallAttempted = "true"
        if ($defaultOut -match '^(?i)\s*timeout\s*$') { $directCallStatus = "timeout" }
        elseif ($def.status_ok) { $directCallStatus = "ok" }
        elseif ($defaultOut -match '(?i)tool[^\\r\\n]*not found|unknown tool|no such tool') { $directCallStatus = "tool_not_found" }
        else { $directCallStatus = "failed" }
    }

    Write-VisibilityFile -PingOk $pingOk -ListToolsFinalStatus $listToolsFinalStatus `
        -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
        -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $visibleField = "false"
    if ($listToolsFinalStatus -eq "ok" -and $toolVisible -eq "true") { $visibleField = "true" }
    elseif ($listToolsFinalStatus -eq "timeout" -and $def.status_ok) { $visibleField = "unknown_list_tools_timeout_but_direct_call_ok" }
    elseif ($toolVisibilityMode -eq "direct_call_fallback" -and $def.status_ok -and $directCallStatus -eq "ok") {
        $visibleField = "unknown_list_tools_timeout_but_direct_call_ok"
    }

    if ($def.status_ok -and $def.ok -eq "true") {
        $callable = "true"
        $defaultCallOk = "true"
    }

    $numOk = ($null -ne $def.footprint_count) -and ($null -ne $def.pad_count) -and ($null -ne $def.track_count) `
        -and ($null -ne $def.via_count) -and ($null -ne $def.zone_count) -and ($null -ne $def.drawing_count) `
        -and ($null -ne $def.items_summary_count)
    if ($numOk -and $def.footprint_count -ge 0 -and $def.pad_count -ge 0) { $countsOk = "true" }

    if ($def.status_ok -and $def.board_filename.Length -gt 0 -and $numOk `
            -and ([double]$def.items_summary_count -le 500)) {
        $itemsSummaryOk = "true"
    }

    $exactBlockerSummary = $def.blocker
    $toolExactBlockerField = $def.blocker
    $summaryBoardFn = $def.board_filename
    if ($null -ne $def.footprint_count) { $sf = [string]$def.footprint_count }
    if ($null -ne $def.pad_count) { $pf = [string]$def.pad_count }
    if ($null -ne $def.track_count) { $tf = [string]$def.track_count }
    if ($null -ne $def.via_count) { $vf = [string]$def.via_count }
    if ($null -ne $def.zone_count) { $zf = [string]$def.zone_count }
    if ($null -ne $def.drawing_count) { $df = [string]$def.drawing_count }
    if ($null -ne $def.items_summary_count) { $isc = [string]$def.items_summary_count }
    $istrunc = $def.items_summary_truncated

    # max_items = 1
    $maxOut = Invoke-Session -ArgsTable @{ board_filename = $pcbAbs; max_items = 1 } -LogName "pcb_get_board_items_session_max1.log"
    $maxP = Parse-PcbBoardItemsSessionOutput -Text $maxOut
    if ($maxP.status_ok -and $maxP.ok -eq "true" -and [double]$maxP.items_summary_count -le 1) {
        $maxItemsCallOk = "true"
    }
    $totalEst = 0
    if ($null -ne $def.footprint_count) { $totalEst += [int]$def.footprint_count }
    if ($null -ne $def.pad_count) { $totalEst += [int]$def.pad_count }
    if ($null -ne $def.track_count) { $totalEst += [int]$def.track_count }
    if ($null -ne $def.via_count) { $totalEst += [int]$def.via_count }
    if ($null -ne $def.zone_count) { $totalEst += [int]$def.zone_count }
    if ($null -ne $def.drawing_count) { $totalEst += [int]$def.drawing_count }

    if ($maxItemsCallOk -eq "true") {
        if ($totalEst -le 1) {
            if ($maxP.items_summary_truncated -eq "false") { $maxItemsTruncationOk = "true" }
        }
        else {
            if ($maxP.items_summary_truncated -eq "true") { $maxItemsTruncationOk = "true" }
        }
    }

    # invalid item type -> bad request path with invalid_item_type
    $badTypeOut = Invoke-Session -ArgsTable @{ board_filename = $pcbAbs; item_types = @("not_supported_type_qa") } -LogName "pcb_get_board_items_session_bad_type.log"
    if (($badTypeOut -match 'status=3') -and ($badTypeOut -match 'invalid_item_type')) { $negInvalidOk = "true" }

    $wrongAbs = [System.IO.Path]::GetFullPath((Join-Path (Split-Path $pcbAbs) "__nonexistent__.kicad_pcb"))
    $mmOut = Invoke-Session -ArgsTable @{ board_filename = $wrongAbs } -LogName "pcb_get_board_items_session_mismatch.log"
    $mm = Parse-PcbBoardItemsSessionOutput -Text $mmOut
    if ($mm.blocker -eq "board_filename_mismatch") { $negMismatchOk = "true" }

    @(
        "visible_in_context_if_applicable=$visibleField"
        "callable=$callable"
        "fixture_setup_ok=$fixtureSetupOk"
        "board_open_ok=$boardOpenOk"
        "default_call_ok=$defaultCallOk"
        "counts_ok=$countsOk"
        "items_summary_ok=$itemsSummaryOk"
        "max_items_call_ok=$maxItemsCallOk"
        "max_items_truncation_ok=$maxItemsTruncationOk"
        "negative_invalid_item_type_ok=$negInvalidOk"
        "negative_board_filename_mismatch_ok=$negMismatchOk"
        "exact_blocker_if_any=$exactBlockerSummary"
        "tool_exact_blocker_field=$toolExactBlockerField"
        "board_filename=$summaryBoardFn"
        "footprint_count=$sf"
        "pad_count=$pf"
        "track_count=$tf"
        "via_count=$vf"
        "zone_count=$zf"
        "drawing_count=$df"
        "items_summary_count=$isc"
        "items_summary_truncated=$istrunc"
        "fixture_source=ipc_v1"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_board_items_session_contract_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
