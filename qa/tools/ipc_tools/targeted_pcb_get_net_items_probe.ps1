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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-get-net-items-session-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

function Parse-PcbGetNetItemsSessionOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,800}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $bf = ""
    if ($Text -match 'key:\s+"board_filename"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $bf = $Matches[1] }
    $nn = ""
    if ($Text -match 'key:\s+"net_name"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $nn = $Matches[1] }
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,800}?number_value:\s*(-?[0-9]+(?:\.[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    $nc = & $parseNum "net_code"
    $trunc = "false"
    if ($Text -match 'key:\s+"items_summary_truncated"[\s\S]{0,800}?bool_value:\s*(true|false)') { $trunc = $Matches[1] }
    return @{
        status_ok               = ($Text -match 'status=1')
        ok                      = $ok
        blocker                 = $blocker
        board_filename          = $bf
        net_name                = $nn
        net_code                = $nc
        pad_count               = & $parseNum "pad_count"
        track_count             = & $parseNum "track_count"
        via_count               = & $parseNum "via_count"
        zone_count              = & $parseNum "zone_count"
        items_summary_count     = & $parseNum "items_summary_count"
        items_summary_truncated = $trunc
        raw                     = $Text
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
    ) | Set-Content -Path (Join-Path $runDir "pcb_get_net_items_tool_visibility_status.txt") -Encoding UTF8
}

function Invoke-NetItemsSession {
    param([hashtable]$ArgsTable, [string]$LogName)
    $logPath = Join-Path $runDir $LogName
    $argsPath = Join-Path $runDir ($LogName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_net_items_session", ("@" + $argsPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot).Text
    $out | Set-Content -Path $logPath -Encoding UTF8
    return $out
}

function Invoke-GetNets {
    param([string]$LogName)
    $logPath = Join-Path $runDir $LogName
    $argsPath = Join-Path $runDir ($LogName + ".args.json")
    (@{ board_filename = $pcbAbs } | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_nets", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $out | Set-Content -Path $logPath -Encoding UTF8
    return $out
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
    $netSelectedOk = "false"
    $defaultCallOk = "false"
    $countsOk = "false"
    $itemsSummaryOk = "false"
    $maxItemsCallOk = "false"
    $maxItemsTruncationOk = "skipped"
    $negInvalidSelOk = "false"
    $negNetNotFoundOk = "false"
    $negBoardMmOk = "false"

    $exactBlockerSummary = "N/A"
    $toolExactBlockerField = "N/A"
    $summaryBoardFn = ""
    $sn = ""; $sc = ""; $pf = ""; $tf = ""; $vf = ""; $zf = ""; $isc = ""; $istrunc = "false"

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
            "net_selected_ok=false"
            "default_call_ok=false"
            "counts_ok=false"
            "items_summary_ok=false"
            "max_items_call_ok=false"
            "max_items_truncation_ok=skipped"
            "negative_invalid_net_selector_ok=false"
            "negative_net_not_found_ok=false"
            "negative_board_filename_mismatch_ok=false"
            "negative_net_selector_mismatch_ok=skipped"
            "exact_blocker_if_any=qa_open_editor_frame_failed"
            "tool_exact_blocker_field=N/A"
            "board_filename="
            "net_name="
            "net_code="
            "pad_count="
            "track_count="
            "via_count="
            "zone_count="
            "items_summary_count="
            "items_summary_truncated="
            "fixture_source=ipc_v1"
            "fixture_board=$pcbAbs"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_net_items_session_contract_summary.txt") -Encoding UTF8
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
        if ($lt -match "pcb_get_net_items_session") { $toolVisible = "true" }
        else {
            $toolVisible = "false"
            $toolVisibilityMode = "direct_call_fallback"
        }
    }

    $netsOut = Invoke-GetNets -LogName "get_nets_for_probe.log"
    $pickedNetName = ""
    if ($netsOut -match 'first_net_name=([^\r\n]+)') {
        $pickedNetName = ($Matches[1] -replace '\s+$', '').Trim()
    }
    $matches = [regex]::Matches($netsOut, 'key:\s+"name"[\s\S]{0,120}?string_value:\s*"([^"]+)"')

    if ($pickedNetName.Length -gt 0) { $netSelectedOk = "true" }

    $defaultOut = ""
    $def = $null
    if ($netSelectedOk -eq "true") {
        $defaultOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_name = $pickedNetName } -LogName "pcb_get_net_items_session_default.log"
        $def = Parse-PcbGetNetItemsSessionOutput -Text $defaultOut
    }

    if ($toolVisibilityMode -eq "direct_call_fallback") {
        $directCallAttempted = "true"
        if ($defaultOut -match '^(?i)\s*timeout\s*$') { $directCallStatus = "timeout" }
        elseif ($null -ne $def -and $def.status_ok) { $directCallStatus = "ok" }
        elseif ($defaultOut -match '(?i)tool[^\\r\\n]*not found|unknown tool|no such tool') { $directCallStatus = "tool_not_found" }
        else { $directCallStatus = "failed" }
    }

    Write-VisibilityFile -PingOk $pingOk -ListToolsFinalStatus $listToolsFinalStatus `
        -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
        -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $visibleField = "false"
    if ($listToolsFinalStatus -eq "ok" -and $toolVisible -eq "true") { $visibleField = "true" }
    elseif ($listToolsFinalStatus -eq "timeout" -and $null -ne $def -and $def.status_ok) { $visibleField = "unknown_list_tools_timeout_but_direct_call_ok" }
    elseif ($toolVisibilityMode -eq "direct_call_fallback" -and $null -ne $def -and $def.status_ok -and $directCallStatus -eq "ok") {
        $visibleField = "unknown_list_tools_timeout_but_direct_call_ok"
    }

    if ($null -ne $def -and $def.status_ok -and $def.ok -eq "true") {
        $callable = "true"
        $defaultCallOk = "true"
    }

    $numOk = $false
    if ($null -ne $def) {
        $numOk = ($null -ne $def.pad_count) -and ($null -ne $def.track_count) -and ($null -ne $def.via_count) -and ($null -ne $def.zone_count) -and ($null -ne $def.items_summary_count)
    }
    if ($numOk) { $countsOk = "true" }

    $sumParts = 0
    if ($null -ne $def) {
        if ($null -ne $def.pad_count) { $sumParts += [int]$def.pad_count }
        if ($null -ne $def.track_count) { $sumParts += [int]$def.track_count }
        if ($null -ne $def.via_count) { $sumParts += [int]$def.via_count }
        if ($null -ne $def.zone_count) { $sumParts += [int]$def.zone_count }
    }

    if ($null -ne $def -and $def.status_ok -and $def.board_filename.Length -gt 0 -and $numOk -and ([double]$def.items_summary_count -le 500)) {
        $itemsSummaryOk = "true"
    }

    $canonicalCode = $null
    if ($null -ne $def -and $null -ne $def.net_code) { $canonicalCode = [int]$def.net_code }

    # Optional second call by net_code when observable
    if ($null -ne $canonicalCode -and $canonicalCode -gt 0) {
        $byCodeOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_code = $canonicalCode } -LogName "pcb_get_net_items_session_by_code.log"
        $byCodeOut | Out-Null
    }

    # max_items = 1
    $maxOut = ""
    $maxP = $null
    if ($netSelectedOk -eq "true") {
        $maxOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_name = $pickedNetName; max_items = 1 } -LogName "pcb_get_net_items_session_max1.log"
        $maxP = Parse-PcbGetNetItemsSessionOutput -Text $maxOut
        if ($maxP.status_ok -and $maxP.ok -eq "true" -and [double]$maxP.items_summary_count -le 1) {
            $maxItemsCallOk = "true"
        }
        if ($maxItemsCallOk -eq "true") {
            if ($sumParts -le 1) {
                if ($maxP.items_summary_truncated -eq "false") { $maxItemsTruncationOk = "true" }
            }
            else {
                if ($maxP.items_summary_truncated -eq "true") { $maxItemsTruncationOk = "true" }
            }
        }
    }

    # Negatives
    $negSelOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs } -LogName "pcb_get_net_items_session_neg_no_selector.log"
    $negSel = Parse-PcbGetNetItemsSessionOutput -Text $negSelOut
    if ($negSel.blocker -eq "invalid_net_selector") { $negInvalidSelOk = "true" }

    $negZeroOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_code = 0 } -LogName "pcb_get_net_items_session_neg_netcode_zero.log"
    $negZero = Parse-PcbGetNetItemsSessionOutput -Text $negZeroOut
    if ($negZero.blocker -eq "invalid_net_selector") { $negInvalidSelOk = "true" }

    $negNfOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_name = "__qa_nonexistent_net_z__" } -LogName "pcb_get_net_items_session_neg_not_found.log"
    $negNf = Parse-PcbGetNetItemsSessionOutput -Text $negNfOut
    if ($negNf.blocker -eq "net_not_found") { $negNetNotFoundOk = "true" }

    $wrongAbs = [System.IO.Path]::GetFullPath((Join-Path (Split-Path $pcbAbs) "__nonexistent__.kicad_pcb"))
    $negMmOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $wrongAbs; net_name = $(if ($pickedNetName) { $pickedNetName } else { "X" }) } -LogName "pcb_get_net_items_session_neg_mismatch_board.log"
    $negMm = Parse-PcbGetNetItemsSessionOutput -Text $negMmOut
    if ($negMm.blocker -eq "board_filename_mismatch") { $negBoardMmOk = "true" }

    $negMismatchResult = "skipped"
    if ($null -ne $pickedNetName -and $pickedNetName.Length -gt 0 -and $null -ne $canonicalCode -and $canonicalCode -gt 0) {
        $secondNetName = ""
        foreach ($m in $matches) {
            $v = $m.Groups[1].Value
            if ($v.Length -gt 0 -and $v -ne $pickedNetName) { $secondNetName = $v; break }
        }
        if ($secondNetName.Length -gt 0) {
            $probeOther = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_name = $secondNetName } -LogName "pcb_get_net_items_session_probe_second_net.log"
            $probeP = Parse-PcbGetNetItemsSessionOutput -Text $probeOther
            if ($probeP.ok -eq "true" -and $null -ne $probeP.net_code -and [int]$probeP.net_code -ne $canonicalCode) {
                $wrongCode = [int]$probeP.net_code
                $mixOut = Invoke-NetItemsSession -ArgsTable @{ board_filename = $pcbAbs; net_name = $pickedNetName; net_code = $wrongCode } -LogName "pcb_get_net_items_session_neg_selector_mismatch.log"
                $mix = Parse-PcbGetNetItemsSessionOutput -Text $mixOut
                if ($mix.blocker -eq "net_selector_mismatch") {
                    $negMismatchResult = "true"
                }
                else {
                    $negMismatchResult = "false"
                }
            }
        }
    }

    if ($null -ne $def) {
        $exactBlockerSummary = $def.blocker
        $toolExactBlockerField = $def.blocker
        $summaryBoardFn = $def.board_filename
        $sn = $def.net_name
        if ($null -ne $def.net_code) { $sc = [string]([int]$def.net_code) }
        if ($null -ne $def.pad_count) { $pf = [string]$def.pad_count }
        if ($null -ne $def.track_count) { $tf = [string]$def.track_count }
        if ($null -ne $def.via_count) { $vf = [string]$def.via_count }
        if ($null -ne $def.zone_count) { $zf = [string]$def.zone_count }
        if ($null -ne $def.items_summary_count) { $isc = [string]$def.items_summary_count }
        $istrunc = $def.items_summary_truncated
    }

    @(
        "visible_in_context_if_applicable=$visibleField"
        "callable=$callable"
        "fixture_setup_ok=$fixtureSetupOk"
        "board_open_ok=$boardOpenOk"
        "net_selected_ok=$netSelectedOk"
        "default_call_ok=$defaultCallOk"
        "counts_ok=$countsOk"
        "items_summary_ok=$itemsSummaryOk"
        "max_items_call_ok=$maxItemsCallOk"
        "max_items_truncation_ok=$maxItemsTruncationOk"
        "negative_invalid_net_selector_ok=$negInvalidSelOk"
        "negative_net_not_found_ok=$negNetNotFoundOk"
        "negative_board_filename_mismatch_ok=$negBoardMmOk"
        "negative_net_selector_mismatch_ok=$negMismatchResult"
        "exact_blocker_if_any=$exactBlockerSummary"
        "tool_exact_blocker_field=$toolExactBlockerField"
        "board_filename=$summaryBoardFn"
        "net_name=$sn"
        "net_code=$sc"
        "pad_count=$pf"
        "track_count=$tf"
        "via_count=$vf"
        "zone_count=$zf"
        "items_summary_count=$isc"
        "items_summary_truncated=$istrunc"
        "fixture_source=ipc_v1"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_net_items_session_contract_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
