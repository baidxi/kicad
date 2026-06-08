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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-get-board-item-by-id-session-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

function Parse-PcbGetBoardItemByIdSessionOutput {
    param([string]$Text)
    $ok = "false"
    if ($Text -match 'key:\s+"ok"[\s\S]{0,800}?bool_value:\s*(true|false)') { $ok = $Matches[1] }
    $blocker = "N/A"
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $blocker = $Matches[1] }
    $bf = ""
    if ($Text -match 'key:\s+"board_filename"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $bf = $Matches[1] }
    $iid = ""
    if ($Text -match 'key:\s+"item_id"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $iid = $Matches[1] }
    $it = ""
    if ($Text -match 'key:\s+"item_type"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $it = $Matches[1] }
    $ly = ""
    if ($Text -match 'key:\s+"layer"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $ly = $Matches[1] }
    $parseNum = {
        param($Key)
        if ($Text -match ('key:\s+"' + [regex]::Escape($Key) + '"[\s\S]{0,800}?number_value:\s*(-?[0-9]+(?:\.[0-9]+)?)')) {
            return [double]$Matches[1]
        }
        return $null
    }
    $xnm = & $parseNum "x_nm"
    $ynm = & $parseNum "y_nm"
    $net = ""
    if ($Text -match 'key:\s+"net_name"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $net = $Matches[1] }
    $pf = ""
    if ($Text -match 'key:\s+"parent_footprint_id"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $pf = $Matches[1] }
    $ref = ""
    if ($Text -match 'key:\s+"reference"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $ref = $Matches[1] }
    $val = ""
    if ($Text -match 'key:\s+"value"[\s\S]{0,800}?string_value:\s*"([^"]*)"') { $val = $Matches[1] }
    return @{
        status_ok      = ($Text -match 'status=1')
        ok             = $ok
        blocker        = $blocker
        board_filename = $bf
        item_id        = $iid
        item_type      = $it
        layer          = $ly
        x_nm           = $xnm
        y_nm           = $ynm
        net_name       = $net
        parent_fp_id   = $pf
        reference      = $ref
        value          = $val
        raw            = $Text
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
    ) | Set-Content -Path (Join-Path $runDir "pcb_get_board_item_by_id_tool_visibility_status.txt") -Encoding UTF8
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
    $inventoryCallOk = "false"
    $itemSelectedOk = "false"
    $defaultCallOk = "false"
    $itemDetailsOk = "false"
    $negInvalidOk = "false"
    $negNotFoundOk = "false"
    $negMismatchOk = "false"

    $exactBlockerSummary = "N/A"
    $toolExactBlockerField = "N/A"
    $summaryBoardFn = ""
    $summaryItemId = ""
    $summaryItemType = ""
    $summaryLayer = ""
    $summaryXnm = ""
    $summaryYnm = ""
    $summaryNet = ""
    $summaryParentFp = ""
    $summaryRef = ""
    $summaryVal = ""
    $pickedItemId = ""

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
            "inventory_call_ok=$inventoryCallOk"
            "item_selected_ok=$itemSelectedOk"
            "default_call_ok=$defaultCallOk"
            "item_details_ok=$itemDetailsOk"
            "negative_invalid_item_id_ok=$negInvalidOk"
            "negative_item_not_found_ok=$negNotFoundOk"
            "negative_board_filename_mismatch_ok=$negMismatchOk"
            "exact_blocker_if_any=qa_open_editor_frame_failed"
            "tool_exact_blocker_field=N/A"
            "board_filename="
            "item_id="
            "item_type="
            "layer="
            "x_nm="
            "y_nm="
            "net_name="
            "parent_footprint_id="
            "reference="
            "value="
            "fixture_source=ipc_v1"
            "fixture_board=$pcbAbs"
            ""
        ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_board_item_by_id_session_contract_summary.txt") -Encoding UTF8
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
        if ($lt -match "pcb_get_board_item_by_id_session") { $toolVisible = "true" }
        else {
            $toolVisible = "false"
            $toolVisibilityMode = "direct_call_fallback"
        }
    }

    function Invoke-BoardItemsSession {
        param([hashtable]$ArgsTable, [string]$LogName)
        $logPath = Join-Path $runDir $LogName
        $argsPath = Join-Path $runDir ($LogName + ".args.json")
        ($ArgsTable | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
        $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $argsPath)) -TimeoutMs 300000 -WorkingDirectory $RepoRoot).Text
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    function Invoke-ItemByIdSession {
        param([hashtable]$ArgsTable, [string]$LogName)
        $logPath = Join-Path $runDir $LogName
        $argsPath = Join-Path $runDir ($LogName + ".args.json")
        ($ArgsTable | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
        $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_get_board_item_by_id_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    $invOut = Invoke-BoardItemsSession -ArgsTable @{ board_filename = $pcbAbs; max_items = 1 } -LogName "pcb_get_board_items_session_inventory.log"
    if ($invOut -match 'status=1') { $inventoryCallOk = "true" }

    if ($invOut -match 'first_item_id=([0-9a-fA-F-]{36})') {
        $pickedItemId = $Matches[1].Trim()
    }
    elseif ($invOut -match 'first_item_id=([^\r\n]+)') {
        $pickedItemId = ($Matches[1] -replace '\s+$','').Trim()
    }

    if ($pickedItemId.Length -ge 36) { $itemSelectedOk = "true" }

    $byIdOut = ""
    if ($itemSelectedOk -eq "true") {
        $byIdOut = Invoke-ItemByIdSession -ArgsTable @{ board_filename = $pcbAbs; item_id = $pickedItemId } -LogName "pcb_get_board_item_by_id_session_default.log"
    }

    $byId = Parse-PcbGetBoardItemByIdSessionOutput -Text $byIdOut

    if ($toolVisibilityMode -eq "direct_call_fallback") {
        $directCallAttempted = "true"
        if ($byIdOut -match '^(?i)\s*timeout\s*$') { $directCallStatus = "timeout" }
        elseif ($byId.status_ok) { $directCallStatus = "ok" }
        elseif ($byIdOut -match '(?i)tool[^\\r\\n]*not found|unknown tool|no such tool') { $directCallStatus = "tool_not_found" }
        else { $directCallStatus = "failed" }
    }

    Write-VisibilityFile -PingOk $pingOk -ListToolsFinalStatus $listToolsFinalStatus `
        -ToolVisibilityMode $toolVisibilityMode -ToolVisible $toolVisible `
        -DirectCallAttempted $directCallAttempted -DirectCallStatus $directCallStatus

    $visibleField = "false"
    if ($listToolsFinalStatus -eq "ok" -and $toolVisible -eq "true") { $visibleField = "true" }
    elseif ($listToolsFinalStatus -eq "timeout" -and $byId.status_ok) { $visibleField = "unknown_list_tools_timeout_but_direct_call_ok" }
    elseif ($toolVisibilityMode -eq "direct_call_fallback" -and $byId.status_ok -and $directCallStatus -eq "ok") {
        $visibleField = "unknown_list_tools_timeout_but_direct_call_ok"
    }

    if ($byId.status_ok -and $byId.ok -eq "true") {
        $callable = "true"
        $defaultCallOk = "true"
    }

    if ($byId.status_ok -and $byId.ok -eq "true" -and $byId.item_id.Length -gt 0 -and $byId.item_type.Length -gt 0 `
            -and ($null -ne $byId.x_nm) -and ($null -ne $byId.y_nm)) {
        $itemDetailsOk = "true"
    }

    $exactBlockerSummary = $byId.blocker
    $toolExactBlockerField = $byId.blocker
    $summaryBoardFn = $byId.board_filename
    $summaryItemId = $byId.item_id
    $summaryItemType = $byId.item_type
    $summaryLayer = $byId.layer
    if ($null -ne $byId.x_nm) { $summaryXnm = [string]$byId.x_nm }
    if ($null -ne $byId.y_nm) { $summaryYnm = [string]$byId.y_nm }
    $summaryNet = $byId.net_name
    $summaryParentFp = $byId.parent_fp_id
    $summaryRef = $byId.reference
    $summaryVal = $byId.value

    # Negative: invalid item id (SniffTest should fail)
    $negInvOut = Invoke-ItemByIdSession -ArgsTable @{ board_filename = $pcbAbs; item_id = "not-a-uuid" } -LogName "pcb_get_board_item_by_id_session_neg_invalid.log"
    $negInv = Parse-PcbGetBoardItemByIdSessionOutput -Text $negInvOut
    if ($negInv.blocker -eq "invalid_item_id") { $negInvalidOk = "true" }

    # Negative: well-formed KIID string not on board
    $negNfOut = Invoke-ItemByIdSession -ArgsTable @{ board_filename = $pcbAbs; item_id = "11111111-1111-1111-1111-111111111111" } -LogName "pcb_get_board_item_by_id_session_neg_not_found.log"
    $negNf = Parse-PcbGetBoardItemByIdSessionOutput -Text $negNfOut
    if ($negNf.blocker -eq "item_not_found") { $negNotFoundOk = "true" }

    $wrongAbs = [System.IO.Path]::GetFullPath((Join-Path (Split-Path $pcbAbs) "__nonexistent__.kicad_pcb"))
    $negMmOut = Invoke-ItemByIdSession -ArgsTable @{ board_filename = $wrongAbs; item_id = $(if ($pickedItemId) { $pickedItemId } else { "11111111-1111-1111-1111-111111111111" }) } -LogName "pcb_get_board_item_by_id_session_neg_mismatch.log"
    $negMm = Parse-PcbGetBoardItemByIdSessionOutput -Text $negMmOut
    if ($negMm.blocker -eq "board_filename_mismatch") { $negMismatchOk = "true" }

    @(
        "visible_in_context_if_applicable=$visibleField"
        "callable=$callable"
        "fixture_setup_ok=$fixtureSetupOk"
        "board_open_ok=$boardOpenOk"
        "inventory_call_ok=$inventoryCallOk"
        "item_selected_ok=$itemSelectedOk"
        "default_call_ok=$defaultCallOk"
        "item_details_ok=$itemDetailsOk"
        "negative_invalid_item_id_ok=$negInvalidOk"
        "negative_item_not_found_ok=$negNotFoundOk"
        "negative_board_filename_mismatch_ok=$negMismatchOk"
        "exact_blocker_if_any=$exactBlockerSummary"
        "tool_exact_blocker_field=$toolExactBlockerField"
        "board_filename=$summaryBoardFn"
        "item_id=$summaryItemId"
        "item_type=$summaryItemType"
        "layer=$summaryLayer"
        "x_nm=$summaryXnm"
        "y_nm=$summaryYnm"
        "net_name=$summaryNet"
        "parent_footprint_id=$summaryParentFp"
        "reference=$summaryRef"
        "value=$summaryVal"
        "fixture_source=ipc_v1"
        "fixture_board=$pcbAbs"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_get_board_item_by_id_session_contract_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
