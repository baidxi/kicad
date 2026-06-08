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
$dummyId = "00000000-0000-0000-0000-000000000001"

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-negative-contracts-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

function Get-SmokeErrorMessage {
    param([string]$Text)
    if ($Text -match 'error_message=([^\r\n]+)') { return $Matches[1].Trim() }
    if ($Text -match 'error_message:\s*"([^"]+)"') { return $Matches[1].Trim() }
    return $null
}

function Test-ExpectBadRequest {
    param(
        [string]$Text,
        [string]$ExpectedMessage
    )
    if ($Text -notmatch 'status=3') { return $false }
    $msg = Get-SmokeErrorMessage -Text $Text
    return ($msg -eq $ExpectedMessage)
}

function Test-ExpectOk {
    param([string]$Text)
    return ($Text -match 'status=1')
}

function Parse-BoardItemsSessionSummaryCount {
    param([string]$Text)
    if ($Text -match 'key:\s+"items_summary_count"[\s\S]{0,800}?number_value:\s*([0-9]+(?:\.[0-9]+)?)') {
        return [double]$Matches[1]
    }
    return $null
}

function Parse-BoardItemsSessionOk {
    param([string]$Text)
    if ($Text -match 'key:\s+"ok"[\s\S]{0,800}?bool_value:\s*(true|false)') {
        return $Matches[1]
    }
    return "false"
}

$boardFilenameLegacyMissingOk = "false"
$boardFilenameLegacyEmptyOk = "false"
$boardFilenameLegacyNonStringOk = "false"
$boardFilenameLegacyRelativeOk = "skipped_cwd_dependent_not_blocking"
$legacyVisibleLayersBoardFilenameNegativeOk = "false"
$legacyBoardEnabledLayersBoardFilenameNegativeOk = "false"
$legacyPadShapePolygonBoardFilenameNegativeOk = "false"
$legacyBoardLayerNamesBoardFilenameNegativeOk = "false"
$legacyActiveLayerBoardFilenameNegativeOk = "false"
$legacySetActiveLayerBoardFilenameNegativeOk = "false"
$legacySetVisibleLayersBoardFilenameNegativeOk = "false"
$legacyBoardEditorAppearanceSettingsBoardFilenameNegativeOk = "false"
$legacySetBoardEditorAppearanceSettingsBoardFilenameNegativeOk = "false"
$legacyBoardOriginBoardFilenameNegativeOk = "false"
$legacyBoardExtentBoardFilenameNegativeOk = "false"
$legacySetBoardOriginBoardFilenameNegativeOk = "false"
$legacyCreatePcbItemsBoardFilenameNegativeOk = "false"
$legacyDeletePcbItemsBoardFilenameNegativeOk = "false"
$legacyUpdatePcbItemsBoardFilenameNegativeOk = "false"
$boardFilenameSessionMissingOk = "false"
$boardFilenameSessionEmptyOk = "false"
$boardFilenameSessionNonStringOk = "false"
$maxItemsAbsentDefaultOk = "false"
$maxItemsStringOk = "false"
$maxItemsNonIntegerOk = "false"
$maxItemsZeroOk = "false"
$maxItemsTooLargeOk = "false"
$exactBlockerIfAny = "N/A"

$msgMissing = "missing or invalid 'board_filename' (string required)"
$msgEmpty = "'board_filename' must be non-empty"
$msgNotAbsolute = "'board_filename' must be an absolute path"
$msgMaxItemsString = "invalid 'max_items' (number required)"
$msgInvalidMaxItems = "invalid_max_items"

$legacyTools = [ordered]@{
    get_pcb_items         = @{}
    get_pcb_items_by_id   = @{ ids = @($dummyId) }
    get_nets              = @{}
    get_board_stackup     = @{}
    hit_test_pcb          = @{
        id        = $dummyId
        position  = @{ x_nm = 0; y_nm = 0 }
        tolerance = 1
    }
    get_bounding_box_pcb  = @{ ids = @($dummyId) }
}

$sessionTools = @(
    "pcb_get_board_items_session",
    "pcb_get_board_item_by_id_session",
    "pcb_get_net_items_session"
)

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if ($resolved.Resolved) { $resolved.Url } else { $null }
    if (-not $url) { throw "Unable to resolve API URL" }

    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "MCP get_project_info did not report loaded=true before qa_open_editor_frame"
    }

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if ($openOut -notmatch 'status=1') { throw "qa_open_editor_frame failed" }
    Start-Sleep -Seconds 3

    function Invoke-ToolCall {
        param(
            [string]$ToolName,
            [hashtable]$ArgsTable,
            [string]$LogLabel
        )
        $logPath = Join-Path $runDir ($LogLabel + ".log")
        $argsPath = Join-Path $runDir ($LogLabel + ".args.json")
        ($ArgsTable | ConvertTo-Json -Compress -Depth 10) | Set-Content -Path $argsPath -Encoding UTF8
        $out = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", $ToolName, ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
        $out | Set-Content -Path $logPath -Encoding UTF8
        return $out
    }

    function Test-LegacyToolBoardFilenameNegative {
        param(
            [string]$ToolName,
            [hashtable]$ExtraArgs = @{},
            [string]$LogTag = "legacy_neg"
        )
        $base = @{}
        foreach ($k in $ExtraArgs.Keys) { $base[$k] = $ExtraArgs[$k] }
        $out = Invoke-ToolCall -ToolName $ToolName -ArgsTable $base -LogLabel ($LogTag + "_missing_" + $ToolName)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            if ($script:exactBlockerIfAny -eq "N/A") { $script:exactBlockerIfAny = ($LogTag + "_missing:" + $ToolName) }
            return $false
        }

        $base = @{ board_filename = "" }
        foreach ($k in $ExtraArgs.Keys) { $base[$k] = $ExtraArgs[$k] }
        $out = Invoke-ToolCall -ToolName $ToolName -ArgsTable $base -LogLabel ($LogTag + "_empty_" + $ToolName)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgEmpty)) {
            if ($script:exactBlockerIfAny -eq "N/A") { $script:exactBlockerIfAny = ($LogTag + "_empty:" + $ToolName) }
            return $false
        }

        $base = @{ board_filename = 123 }
        foreach ($k in $ExtraArgs.Keys) { $base[$k] = $ExtraArgs[$k] }
        $out = Invoke-ToolCall -ToolName $ToolName -ArgsTable $base -LogLabel ($LogTag + "_non_string_" + $ToolName)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            if ($script:exactBlockerIfAny -eq "N/A") { $script:exactBlockerIfAny = ($LogTag + "_non_string:" + $ToolName) }
            return $false
        }

        return $true
    }

    # --- A. Legacy board_filename (makeBadRequestResult) ---
    $legacyMissingAll = $true
    foreach ($entry in $legacyTools.GetEnumerator()) {
        $tool = $entry.Key
        $base = @{}
        foreach ($k in $entry.Value.Keys) { $base[$k] = $entry.Value[$k] }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $base -LogLabel ("legacy_missing_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            $legacyMissingAll = $false
            $exactBlockerIfAny = "legacy_missing:$tool"
        }
    }
    if ($legacyMissingAll) { $boardFilenameLegacyMissingOk = "true" }

    $legacyEmptyAll = $true
    foreach ($entry in $legacyTools.GetEnumerator()) {
        $tool = $entry.Key
        $base = @{ board_filename = "" }
        foreach ($k in $entry.Value.Keys) { $base[$k] = $entry.Value[$k] }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $base -LogLabel ("legacy_empty_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgEmpty)) {
            $legacyEmptyAll = $false
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "legacy_empty:$tool" }
        }
    }
    if ($legacyEmptyAll) { $boardFilenameLegacyEmptyOk = "true" }

    $legacyNonStringAll = $true
    foreach ($entry in $legacyTools.GetEnumerator()) {
        $tool = $entry.Key
        $base = @{ board_filename = 123 }
        foreach ($k in $entry.Value.Keys) { $base[$k] = $entry.Value[$k] }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $base -LogLabel ("legacy_non_string_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            $legacyNonStringAll = $false
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "legacy_non_string:$tool" }
        }
    }
    if ($legacyNonStringAll) { $boardFilenameLegacyNonStringOk = "true" }

    # Relative path: cwd-dependent (MakeAbsolute); advisory only, not blocking.
    $relOut = Invoke-ToolCall -ToolName "get_pcb_items" -ArgsTable @{ board_filename = "qa_relative_only.kicad_pcb" } -LogLabel "legacy_relative_advisory"
    if (Test-ExpectBadRequest -Text $relOut -ExpectedMessage $msgNotAbsolute) {
        $boardFilenameLegacyRelativeOk = "true_observed_not_absolute"
    }
    elseif (Test-ExpectOk -Text $relOut) {
        $boardFilenameLegacyRelativeOk = "skipped_resolved_absolute_by_cwd"
    }
    else {
        $msg = Get-SmokeErrorMessage -Text $relOut
        $boardFilenameLegacyRelativeOk = "skipped_other_error:$msg"
    }

    # --- A2. R67 legacy tools (board_filename before other params) ---
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_visible_layers") {
        $legacyVisibleLayersBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_board_enabled_layers") {
        $legacyBoardEnabledLayersBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_pad_shape_as_polygon") {
        $legacyPadShapePolygonBoardFilenameNegativeOk = "true"
    }

    # --- A3. R70 layer tools (require PCB frame open; board_filename before layer/layers) ---
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_board_layer_names" -LogTag "r70") {
        $legacyBoardLayerNamesBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_active_layer" -LogTag "r70") {
        $legacyActiveLayerBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "set_active_layer" -LogTag "r70") {
        $legacySetActiveLayerBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "set_visible_layers" -LogTag "r70") {
        $legacySetVisibleLayersBoardFilenameNegativeOk = "true"
    }

    # --- A4. R72 appearance / origins tools (require PCB frame open) ---
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_board_editor_appearance_settings" -LogTag "r72") {
        $legacyBoardEditorAppearanceSettingsBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "set_board_editor_appearance_settings" -LogTag "r72") {
        $legacySetBoardEditorAppearanceSettingsBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_board_origin" -LogTag "r72") {
        $legacyBoardOriginBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "get_board_extent" -LogTag "r72") {
        $legacyBoardExtentBoardFilenameNegativeOk = "true"
    }

    # --- A5. R74 CRUD / set origin tools (require PCB frame open; board_filename before business params) ---
    if (Test-LegacyToolBoardFilenameNegative -ToolName "set_board_origin" -LogTag "r74") {
        $legacySetBoardOriginBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "create_pcb_items" -LogTag "r74") {
        $legacyCreatePcbItemsBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "delete_pcb_items" -LogTag "r74") {
        $legacyDeletePcbItemsBoardFilenameNegativeOk = "true"
    }
    if (Test-LegacyToolBoardFilenameNegative -ToolName "update_pcb_items" -LogTag "r74") {
        $legacyUpdatePcbItemsBoardFilenameNegativeOk = "true"
    }

    # --- B. Session board_filename (mkBad) ---
    $sessionMissingAll = $true
    foreach ($tool in $sessionTools) {
        $args = @{}
        if ($tool -eq "pcb_get_board_item_by_id_session") {
            $args["item_id"] = $dummyId
        }
        elseif ($tool -eq "pcb_get_net_items_session") {
            $args["net_name"] = "GND"
        }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $args -LogLabel ("session_missing_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            $sessionMissingAll = $false
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "session_missing:$tool" }
        }
    }
    if ($sessionMissingAll) { $boardFilenameSessionMissingOk = "true" }

    $sessionEmptyAll = $true
    foreach ($tool in $sessionTools) {
        $args = @{ board_filename = "" }
        if ($tool -eq "pcb_get_board_item_by_id_session") {
            $args["item_id"] = $dummyId
        }
        elseif ($tool -eq "pcb_get_net_items_session") {
            $args["net_name"] = "GND"
        }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $args -LogLabel ("session_empty_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgEmpty)) {
            $sessionEmptyAll = $false
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "session_empty:$tool" }
        }
    }
    if ($sessionEmptyAll) { $boardFilenameSessionEmptyOk = "true" }

    $sessionNonStringAll = $true
    foreach ($tool in $sessionTools) {
        $args = @{ board_filename = 123 }
        if ($tool -eq "pcb_get_board_item_by_id_session") {
            $args["item_id"] = $dummyId
        }
        elseif ($tool -eq "pcb_get_net_items_session") {
            $args["net_name"] = "GND"
        }
        $out = Invoke-ToolCall -ToolName $tool -ArgsTable $args -LogLabel ("session_non_string_" + $tool)
        if (-not (Test-ExpectBadRequest -Text $out -ExpectedMessage $msgMissing)) {
            $sessionNonStringAll = $false
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "session_non_string:$tool" }
        }
    }
    if ($sessionNonStringAll) { $boardFilenameSessionNonStringOk = "true" }

    # --- C. max_items (Inspection v2) ---
    $pickedNetName = ""
    $netsOut = Invoke-ToolCall -ToolName "get_nets" -ArgsTable @{ board_filename = $pcbAbs } -LogLabel "get_nets_for_max_items"
    if ($netsOut -match 'first_net_name=([^\r\n]+)') {
        $pickedNetName = ($Matches[1] -replace '\s+$', '').Trim()
    }
    if ([string]::IsNullOrWhiteSpace($pickedNetName) -and ($netsOut -match 'key:\s+"name"[\s\S]{0,120}?string_value:\s*"([^"]+)"')) {
        $pickedNetName = $Matches[1]
    }

    $defaultItemsOut = Invoke-ToolCall -ToolName "pcb_get_board_items_session" -ArgsTable @{ board_filename = $pcbAbs } -LogLabel "max_items_absent_board_items"
    $defaultItemsOk = Parse-BoardItemsSessionOk -Text $defaultItemsOut
    $defaultItemsCount = Parse-BoardItemsSessionSummaryCount -Text $defaultItemsOut
    if ((Test-ExpectOk -Text $defaultItemsOut) -and $defaultItemsOk -eq "true" -and $null -ne $defaultItemsCount -and $defaultItemsCount -le 500) {
        $maxItemsAbsentDefaultOk = "true"
    }
    else {
        if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_absent_board_items" }
    }

    $stringOut = Invoke-ToolCall -ToolName "pcb_get_board_items_session" -ArgsTable @{ board_filename = $pcbAbs; max_items = "500" } -LogLabel "max_items_string_board_items"
    if (Test-ExpectBadRequest -Text $stringOut -ExpectedMessage $msgMaxItemsString) {
        $maxItemsStringOk = "true"
    }
    else {
        if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_string_board_items" }
    }

    $nonIntOut = Invoke-ToolCall -ToolName "pcb_get_board_items_session" -ArgsTable @{ board_filename = $pcbAbs; max_items = 1.5 } -LogLabel "max_items_non_integer_board_items"
    if (Test-ExpectBadRequest -Text $nonIntOut -ExpectedMessage $msgInvalidMaxItems) {
        $maxItemsNonIntegerOk = "true"
    }
    else {
        if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_non_integer_board_items" }
    }

    $zeroOut = Invoke-ToolCall -ToolName "pcb_get_board_items_session" -ArgsTable @{ board_filename = $pcbAbs; max_items = 0 } -LogLabel "max_items_zero_board_items"
    if (Test-ExpectBadRequest -Text $zeroOut -ExpectedMessage $msgInvalidMaxItems) {
        $maxItemsZeroOk = "true"
    }
    else {
        if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_zero_board_items" }
    }

    $largeOut = Invoke-ToolCall -ToolName "pcb_get_board_items_session" -ArgsTable @{ board_filename = $pcbAbs; max_items = 5001 } -LogLabel "max_items_too_large_board_items"
    if (Test-ExpectBadRequest -Text $largeOut -ExpectedMessage $msgInvalidMaxItems) {
        $maxItemsTooLargeOk = "true"
    }
    else {
        if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_too_large_board_items" }
    }

    # Spot-check max_items on pcb_get_net_items_session when a net is available.
    if (-not [string]::IsNullOrWhiteSpace($pickedNetName)) {
        $netStringOut = Invoke-ToolCall -ToolName "pcb_get_net_items_session" `
            -ArgsTable @{ board_filename = $pcbAbs; net_name = $pickedNetName; max_items = "1" } `
            -LogLabel "max_items_string_net_items"
        if (-not (Test-ExpectBadRequest -Text $netStringOut -ExpectedMessage $msgMaxItemsString)) {
            $maxItemsStringOk = "false"
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_string_net_items" }
        }

        $netZeroOut = Invoke-ToolCall -ToolName "pcb_get_net_items_session" `
            -ArgsTable @{ board_filename = $pcbAbs; net_name = $pickedNetName; max_items = 0 } `
            -LogLabel "max_items_zero_net_items"
        if (-not (Test-ExpectBadRequest -Text $netZeroOut -ExpectedMessage $msgInvalidMaxItems)) {
            $maxItemsZeroOk = "false"
            if ($exactBlockerIfAny -eq "N/A") { $exactBlockerIfAny = "max_items_zero_net_items" }
        }
    }

    $blockingFlags = @(
        $boardFilenameLegacyMissingOk,
        $boardFilenameLegacyEmptyOk,
        $boardFilenameLegacyNonStringOk,
        $legacyVisibleLayersBoardFilenameNegativeOk,
        $legacyBoardEnabledLayersBoardFilenameNegativeOk,
        $legacyPadShapePolygonBoardFilenameNegativeOk,
        $legacyBoardLayerNamesBoardFilenameNegativeOk,
        $legacyActiveLayerBoardFilenameNegativeOk,
        $legacySetActiveLayerBoardFilenameNegativeOk,
        $legacySetVisibleLayersBoardFilenameNegativeOk,
        $legacyBoardEditorAppearanceSettingsBoardFilenameNegativeOk,
        $legacySetBoardEditorAppearanceSettingsBoardFilenameNegativeOk,
        $legacyBoardOriginBoardFilenameNegativeOk,
        $legacyBoardExtentBoardFilenameNegativeOk,
        $legacySetBoardOriginBoardFilenameNegativeOk,
        $legacyCreatePcbItemsBoardFilenameNegativeOk,
        $legacyDeletePcbItemsBoardFilenameNegativeOk,
        $legacyUpdatePcbItemsBoardFilenameNegativeOk,
        $boardFilenameSessionMissingOk,
        $boardFilenameSessionEmptyOk,
        $boardFilenameSessionNonStringOk,
        $maxItemsAbsentDefaultOk,
        $maxItemsStringOk,
        $maxItemsNonIntegerOk,
        $maxItemsZeroOk,
        $maxItemsTooLargeOk
    )
    $blockingFailed = @($blockingFlags | Where-Object { $_ -ne "true" })

    $pcbNegativeContractsGreen = if ($blockingFailed.Count -eq 0) { "true" } else { "false" }
    if ($pcbNegativeContractsGreen -eq "false" -and $exactBlockerIfAny -eq "N/A") {
        $exactBlockerIfAny = "one_or_more_negative_contract_checks_failed"
    }
    if ($pcbNegativeContractsGreen -eq "true") {
        $exactBlockerIfAny = "N/A"
    }

    @(
        "pcb_negative_contracts_green=$pcbNegativeContractsGreen"
        "board_filename_legacy_missing_ok=$boardFilenameLegacyMissingOk"
        "board_filename_legacy_empty_ok=$boardFilenameLegacyEmptyOk"
        "board_filename_legacy_non_string_ok=$boardFilenameLegacyNonStringOk"
        "board_filename_legacy_relative_ok=$boardFilenameLegacyRelativeOk"
        "legacy_visible_layers_board_filename_negative_ok=$legacyVisibleLayersBoardFilenameNegativeOk"
        "legacy_board_enabled_layers_board_filename_negative_ok=$legacyBoardEnabledLayersBoardFilenameNegativeOk"
        "legacy_pad_shape_polygon_board_filename_negative_ok=$legacyPadShapePolygonBoardFilenameNegativeOk"
        "legacy_board_layer_names_board_filename_negative_ok=$legacyBoardLayerNamesBoardFilenameNegativeOk"
        "legacy_active_layer_board_filename_negative_ok=$legacyActiveLayerBoardFilenameNegativeOk"
        "legacy_set_active_layer_board_filename_negative_ok=$legacySetActiveLayerBoardFilenameNegativeOk"
        "legacy_set_visible_layers_board_filename_negative_ok=$legacySetVisibleLayersBoardFilenameNegativeOk"
        "legacy_board_editor_appearance_settings_board_filename_negative_ok=$legacyBoardEditorAppearanceSettingsBoardFilenameNegativeOk"
        "legacy_set_board_editor_appearance_settings_board_filename_negative_ok=$legacySetBoardEditorAppearanceSettingsBoardFilenameNegativeOk"
        "legacy_board_origin_board_filename_negative_ok=$legacyBoardOriginBoardFilenameNegativeOk"
        "legacy_board_extent_board_filename_negative_ok=$legacyBoardExtentBoardFilenameNegativeOk"
        "legacy_set_board_origin_board_filename_negative_ok=$legacySetBoardOriginBoardFilenameNegativeOk"
        "legacy_create_pcb_items_board_filename_negative_ok=$legacyCreatePcbItemsBoardFilenameNegativeOk"
        "legacy_delete_pcb_items_board_filename_negative_ok=$legacyDeletePcbItemsBoardFilenameNegativeOk"
        "legacy_update_pcb_items_board_filename_negative_ok=$legacyUpdatePcbItemsBoardFilenameNegativeOk"
        "board_filename_session_missing_ok=$boardFilenameSessionMissingOk"
        "board_filename_session_empty_ok=$boardFilenameSessionEmptyOk"
        "board_filename_session_non_string_ok=$boardFilenameSessionNonStringOk"
        "max_items_absent_default_ok=$maxItemsAbsentDefaultOk"
        "max_items_string_ok=$maxItemsStringOk"
        "max_items_non_integer_ok=$maxItemsNonIntegerOk"
        "max_items_zero_ok=$maxItemsZeroOk"
        "max_items_too_large_ok=$maxItemsTooLargeOk"
        "exact_blocker_if_any=$exactBlockerIfAny"
        "target_run_dir=$runDir"
        "fixture_board=$pcbAbs"
        "legacy_tools_covered=get_pcb_items;get_pcb_items_by_id;get_nets;get_board_stackup;hit_test_pcb;get_bounding_box_pcb;get_visible_layers;get_board_enabled_layers;get_pad_shape_as_polygon;get_board_layer_names;get_active_layer;set_active_layer;set_visible_layers;get_board_editor_appearance_settings;set_board_editor_appearance_settings;get_board_origin;get_board_extent;set_board_origin;create_pcb_items;delete_pcb_items;update_pcb_items"
        "session_tools_covered=pcb_get_board_items_session;pcb_get_board_item_by_id_session;pcb_get_net_items_session"
        "board_filename_relative_note=MakeAbsolute() is cwd-dependent; relative case is advisory only"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_pcb_negative_contracts_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    if ($pcbNegativeContractsGreen -ne "true") {
        throw "pcb_negative_contracts_green=false exact_blocker_if_any=$exactBlockerIfAny"
    }
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    Stop-KiCadProcesses
}
