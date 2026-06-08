param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R138 — Fixture issue19090 : mutations FOOTPRINT temporaires (set_symbol_field) pour cas validate ;
# restore R_0805_2012Metric en fin de probe ; Stop-KiCadProcesses debut/fin ;
# fp-lib-table projet ecrit (alias Resistor_SMD) — effet disque QA toleré.
# Entre suites lourdes mutantes : Stop-KiCadProcesses avant checkpoint MCP v2.

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_pro"
$sch = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$cfg10 = Join-Path $cfgRoot "10.0"
$projectFpLibTable = Join-Path (Split-Path $proj) "fp-lib-table"
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"
$missingFootprint = "qa_resistors:missing_fp"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $proj)) { throw "Missing project $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic $sch" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$schAbs = (Resolve-Path $sch).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-footprint-readiness-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "qa_resistors")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA deterministic resistor footprint library"))'),
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA alias for issue19090 restore footprint"))'),
    ")"
) | Set-Content -Path $projectFpLibTable -Encoding ASCII
Copy-Item -Path $projectFpLibTable -Destination $globalFpLibTable -Force

$script:ProbeIpcRetryExtraTotal = 0

function Parse-FootprintReadinessOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                                              = ($Text -match 'status=1')
        footprint_readiness_ok                                 = (& $parseLine "footprint_readiness_ok")
        footprint_readiness_symbols_count                      = (& $parseLine "footprint_readiness_symbols_count")
        footprint_readiness_symbols_with_footprint_count       = (& $parseLine "footprint_readiness_symbols_with_footprint_count")
        footprint_readiness_symbols_without_footprint_count    = (& $parseLine "footprint_readiness_symbols_without_footprint_count")
        footprint_readiness_sch_to_pcb_ready                     = (& $parseLine "footprint_readiness_sch_to_pcb_ready")
        footprint_readiness_footprint_validation_available     = (& $parseLine "footprint_readiness_footprint_validation_available")
        footprint_readiness_footprint_validation_performed     = (& $parseLine "footprint_readiness_footprint_validation_performed")
        footprint_readiness_symbols_with_invalid_footprint_count = (& $parseLine "footprint_readiness_symbols_with_invalid_footprint_count")
        footprint_readiness_sch_to_pcb_ready_validated         = (& $parseLine "footprint_readiness_sch_to_pcb_ready_validated")
        footprint_readiness_symbols_with_unvalidated_footprint_count = (& $parseLine "footprint_readiness_symbols_with_unvalidated_footprint_count")
        footprint_readiness_invalid_footprints_returned        = (& $parseLine "footprint_readiness_invalid_footprints_returned")
        footprint_readiness_exact_blocker_if_any               = (& $parseLine "footprint_readiness_exact_blocker_if_any")
        footprint_readiness_symbols_returned                   = (& $parseLine "footprint_readiness_symbols_returned")
        footprint_readiness_diagnostics                        = (& $parseLine "footprint_readiness_diagnostics")
        footprint_readiness_scope                              = (& $parseLine "footprint_readiness_scope")
        footprint_readiness_sheets_count                       = (& $parseLine "footprint_readiness_sheets_count")
        footprint_readiness_symbols_instances_count            = (& $parseLine "footprint_readiness_symbols_instances_count")
        footprint_readiness_first_sheet_path                   = (& $parseLine "footprint_readiness_first_sheet_path")
        footprint_readiness_first_instance_path                = (& $parseLine "footprint_readiness_first_instance_path")
        footprint_readiness_hierarchy_assign_instance_safe_enabled = (& $parseLine "footprint_readiness_hierarchy_assign_instance_safe_enabled")
        footprint_readiness_recommended_footprint_assignment_workflow = (& $parseLine "footprint_readiness_recommended_footprint_assignment_workflow")
        footprint_readiness_fallback_footprint_assignment_workflow = (& $parseLine "footprint_readiness_fallback_footprint_assignment_workflow")
        footprint_readiness_recommended_assign_tool            = (& $parseLine "footprint_readiness_recommended_assign_tool")
        footprint_readiness_recommended_assign_scope           = (& $parseLine "footprint_readiness_recommended_assign_scope")
    }
}

function Test-RuntimeWorkflowEchoFields {
    param($Parsed)
    if ($Parsed.footprint_readiness_hierarchy_assign_instance_safe_enabled -ne "true") { return $false }
    if ($Parsed.footprint_readiness_recommended_footprint_assignment_workflow -ne "hierarchy_instance_safe") { return $false }
    if ($Parsed.footprint_readiness_fallback_footprint_assignment_workflow -ne "navigate_current_sheet_dry_run") { return $false }
    if ($Parsed.footprint_readiness_recommended_assign_tool -ne "schematic_assign_footprint_session") { return $false }
    if ($Parsed.footprint_readiness_recommended_assign_scope -ne "hierarchy") { return $false }
    return $true
}

function Invoke-FootprintReadiness {
    param(
        [string]$ApiUrl,
        [hashtable]$ArgsTable,
        [string]$LogBaseName
    )
    $jsonPath = Join-Path $runDir ($LogBaseName + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "schematic_footprint_readiness_session", ("@" + $jsonPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $rax = $sm['IpcSmokeRetryExtraAttempts']; if ($null -ne $rax) { $script:ProbeIpcRetryExtraTotal += [int]$rax }
    $out = $sm.Text
    $out | Set-Content -Path (Join-Path $runDir ($LogBaseName + ".log")) -Encoding UTF8
    return @{
        Parsed = Parse-FootprintReadinessOutput -Text $out
        Status = if ($out -match 'status=1') { "ok" } else { "error" }
    }
}

function Test-KiidLike {
    param([string]$Value)
    return ($Value -match '^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')
}

function Take-FirstValid {
    param([System.Text.RegularExpressions.MatchCollection]$Matches)
    foreach ($m in $Matches) {
        if ($m.Groups.Count -lt 2) { continue }
        $v = $m.Groups[1].Value.Trim()
        if (Test-KiidLike $v) { return $v }
    }
    return ""
}

function Resolve-SymbolIdFromSchItemsText {
    param([string]$ItemsOut)
    if ([string]::IsNullOrWhiteSpace($ItemsOut)) { return "" }
    $symbolId = ""
    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
    $symbolId = Take-FirstValid $typedSchSymId
    if ([string]::IsNullOrWhiteSpace($symbolId)) {
        $typedSchSymUuid = [regex]::Matches(
            $ItemsOut,
            'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
        $symbolId = Take-FirstValid $typedSchSymUuid
    }
    if ([string]::IsNullOrWhiteSpace($symbolId)) {
        $idValueCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
        $symbolId = Take-FirstValid $idValueCandidates
    }
    return $symbolId
}

function Resolve-AllSymbolIdsFromSchItemsText {
    param([string]$ItemsOut)
    $ids = @()
    if ([string]::IsNullOrWhiteSpace($ItemsOut)) { return $ids }
    foreach ($m in [regex]::Matches($ItemsOut, 'sch_item_id=([0-9a-fA-F-]{36})')) {
        $v = $m.Groups[1].Value.Trim()
        if (Test-KiidLike $v) { $ids += $v }
    }
    if ($ids.Count -gt 0) { return @($ids | Select-Object -Unique) }
    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"')
    foreach ($m in $typedSchSymId) {
        if ($m.Groups.Count -lt 2) { continue }
        $v = $m.Groups[1].Value.Trim()
        if (Test-KiidLike $v) { $ids += $v }
    }
    if ($ids.Count -eq 0 -and $ItemsOut -match 'first_item_id=([0-9a-fA-F-]{36})') {
        $v = $Matches[1]
        if (Test-KiidLike $v) { $ids += $v }
    }
    return @($ids | Select-Object -Unique)
}

function Set-SymbolFootprintField {
    param(
        [string]$ApiUrl,
        [string]$SchFilename,
        [string]$SymId,
        [string]$FpValue,
        [string]$LogSuffix
    )
    $args = @{
        schematic_filename = $SchFilename
        symbol_id          = $SymId
        field_name         = "Footprint"
        new_value          = $FpValue
        dry_run            = $false
    } | ConvertTo-Json -Compress
    $inputPath = Join-Path $runDir ("set_symbol_field_" + $LogSuffix + ".json")
    Set-Content -Path $inputPath -Value $args -Encoding UTF8
    $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "schematic_set_symbol_field_session", ("@" + $inputPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($sm.Text -notmatch 'status=1') { throw "set_symbol_field_failed_$LogSuffix" }
}

$happyOk = "false"
$countsOk = "false"
$schReadyOk = "false"
$defaultUnchangedOk = "false"
$validateHappyOk = "false"
$validateInvalidOk = "false"
$negMissingFilenameOk = "false"
$negMismatchOk = "false"
$hierarchyScopeOk = "false"
$hierarchyCountsOk = "false"
$hierarchyPathsOk = "false"
$hierarchyValidateOk = "false"
$hierarchyNoDoubleCountOk = "false"
$runtimeWorkflowEchoOk = "false"
$hierarchyAssignRuntimeEchoOk = "false"
$recommendedAssignScopeEchoOk = "false"
$finalBlocker = "N/A"

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "ipc_url_unresolved" }
    $url = $resolved.Url
    if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
        throw "project_not_loaded"
    }

    $openArgs = @{ type = "sch"; filename = $schAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_sch_input.json"
    Set-Content -Path $openInput -Value $openArgs -Encoding UTF8
    $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_failed" }
    Start-Sleep -Seconds 3

    $tDefault = Invoke-FootprintReadiness -ApiUrl $url -ArgsTable @{
        schematic_filename  = $schAbs
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $false
    } -LogBaseName "footprint_readiness_default_false"

    $symCount = 0
    $withCount = 0
    $withoutCount = 0
    if ($tDefault.Parsed.footprint_readiness_symbols_count -match '^[0-9]+$') {
        $symCount = [int]$tDefault.Parsed.footprint_readiness_symbols_count
    }
    if ($tDefault.Parsed.footprint_readiness_symbols_with_footprint_count -match '^[0-9]+$') {
        $withCount = [int]$tDefault.Parsed.footprint_readiness_symbols_with_footprint_count
    }
    if ($tDefault.Parsed.footprint_readiness_symbols_without_footprint_count -match '^[0-9]+$') {
        $withoutCount = [int]$tDefault.Parsed.footprint_readiness_symbols_without_footprint_count
    }

    if ($tDefault.Status -eq "ok" -and $tDefault.Parsed.footprint_readiness_ok -eq "true" `
            -and $tDefault.Parsed.footprint_readiness_exact_blocker_if_any -eq "N/A" `
            -and $tDefault.Parsed.footprint_readiness_footprint_validation_performed -eq "false" `
            -and $tDefault.Parsed.footprint_readiness_symbols_with_invalid_footprint_count -eq "null" `
            -and $tDefault.Parsed.footprint_readiness_sch_to_pcb_ready_validated -eq "null" `
            -and $tDefault.Parsed.footprint_readiness_symbols_with_unvalidated_footprint_count -match '^[0-9]+$') {
        $defaultUnchangedOk = "true"
    }

    if ($tDefault.Status -eq "ok" -and $tDefault.Parsed.footprint_readiness_ok -eq "true" `
            -and $tDefault.Parsed.footprint_readiness_exact_blocker_if_any -eq "N/A") {
        $happyOk = "true"
    }
    if ($symCount -ge 2 -and $withCount -ge 1 `
            -and ($withCount + $withoutCount) -eq $symCount) {
        $countsOk = "true"
    }
    if ($tDefault.Parsed.footprint_readiness_sch_to_pcb_ready -eq "true" -and $withoutCount -eq 0) {
        $schReadyOk = "true"
    }

    if (Test-RuntimeWorkflowEchoFields -Parsed $tDefault.Parsed) {
        $runtimeWorkflowEchoOk = "true"
        $hierarchyAssignRuntimeEchoOk = "true"
        $recommendedAssignScopeEchoOk = "true"
    }

    $itemsArgs = @{ schematic_filename = $schAbs; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
    Set-Content -Path $itemsInput -Value $itemsArgs -Encoding UTF8
    $itemsSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
    $symbolIds = @(Resolve-AllSymbolIdsFromSchItemsText -ItemsOut $itemsSm.Text)
    if ($symbolIds.Count -lt 1) { throw "symbol_id discovery failed for validation setup" }
    $symbolId = $symbolIds[0]

    $validFootprint = "Resistor_SMD:R_0201_0603Metric"
    $restoreFootprint = "Resistor_SMD:R_0805_2012Metric"
    foreach ($sid in $symbolIds) {
        Set-SymbolFootprintField -ApiUrl $url -SchFilename $schAbs -SymId $sid -FpValue $validFootprint -LogSuffix ("valid_fp_" + $sid.Substring(0, 8))
    }
    Start-Sleep -Seconds 1

    $tValidateHappy = Invoke-FootprintReadiness -ApiUrl $url -ArgsTable @{
        schematic_filename  = $schAbs
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $true
    } -LogBaseName "footprint_readiness_validate_happy"

    $invalidCountHappy = -1
    if ($tValidateHappy.Parsed.footprint_readiness_symbols_with_invalid_footprint_count -match '^[0-9]+$') {
        $invalidCountHappy = [int]$tValidateHappy.Parsed.footprint_readiness_symbols_with_invalid_footprint_count
    }

    if ($tValidateHappy.Status -eq "ok" `
            -and $tValidateHappy.Parsed.footprint_readiness_ok -eq "true" `
            -and $tValidateHappy.Parsed.footprint_readiness_footprint_validation_available -eq "true" `
            -and $tValidateHappy.Parsed.footprint_readiness_footprint_validation_performed -eq "true" `
            -and $invalidCountHappy -eq 0 `
            -and $tValidateHappy.Parsed.footprint_readiness_sch_to_pcb_ready_validated -eq "true" `
            -and $tValidateHappy.Parsed.footprint_readiness_diagnostics -match "validation_cache_hits=") {
        $validateHappyOk = "true"
    }

    if ($runtimeWorkflowEchoOk -ne "true" -and (Test-RuntimeWorkflowEchoFields -Parsed $tValidateHappy.Parsed)) {
        $runtimeWorkflowEchoOk = "true"
        $hierarchyAssignRuntimeEchoOk = "true"
        $recommendedAssignScopeEchoOk = "true"
    }

    Set-SymbolFootprintField -ApiUrl $url -SchFilename $schAbs -SymId $symbolId -FpValue $missingFootprint -LogSuffix "invalid_fp"
    Start-Sleep -Seconds 1

    $tValidateInvalid = Invoke-FootprintReadiness -ApiUrl $url -ArgsTable @{
        schematic_filename  = $schAbs
        include_symbols     = $true
        max_symbols         = 50
        validate_footprints = $true
    } -LogBaseName "footprint_readiness_validate_invalid"

    $invalidCountBad = 0
    if ($tValidateInvalid.Parsed.footprint_readiness_symbols_with_invalid_footprint_count -match '^[0-9]+$') {
        $invalidCountBad = [int]$tValidateInvalid.Parsed.footprint_readiness_symbols_with_invalid_footprint_count
    }
    $invalidListCount = 0
    if ($tValidateInvalid.Parsed.footprint_readiness_invalid_footprints_returned -match '^[0-9]+$') {
        $invalidListCount = [int]$tValidateInvalid.Parsed.footprint_readiness_invalid_footprints_returned
    }

    if ($tValidateInvalid.Status -eq "ok" `
            -and $tValidateInvalid.Parsed.footprint_readiness_footprint_validation_performed -eq "true" `
            -and $invalidCountBad -ge 1 `
            -and $tValidateInvalid.Parsed.footprint_readiness_sch_to_pcb_ready_validated -eq "false" `
            -and $invalidListCount -ge 1) {
        $validateInvalidOk = "true"
    }

    foreach ($sid in $symbolIds) {
        Set-SymbolFootprintField -ApiUrl $url -SchFilename $schAbs -SymId $sid -FpValue $restoreFootprint -LogSuffix ("restore_fp_" + $sid.Substring(0, 8))
    }

    $tNegMissing = Invoke-FootprintReadiness -ApiUrl $url -ArgsTable @{} -LogBaseName "footprint_readiness_neg_missing_filename"
    if ($tNegMissing.Status -ne "ok") {
        $negMissingFilenameOk = "true"
    }

    $tNegMismatch = Invoke-FootprintReadiness -ApiUrl $url -ArgsTable @{
        schematic_filename = "C:\nonexistent\wrong.kicad_sch"
    } -LogBaseName "footprint_readiness_neg_mismatch"
    if ($tNegMismatch.Status -eq "ok" `
            -and $tNegMismatch.Parsed.footprint_readiness_ok -eq "false" `
            -and $tNegMismatch.Parsed.footprint_readiness_exact_blocker_if_any -eq "schematic_filename_mismatch") {
        $negMismatchOk = "true"
    }

    $green = "true"
    if ($defaultUnchangedOk -ne "true") { $green = "false"; $finalBlocker = "default_false_regression" }
    elseif ($happyOk -ne "true") { $green = "false"; $finalBlocker = "happy_path_failed" }
    elseif ($countsOk -ne "true") { $green = "false"; $finalBlocker = "counts_inconsistent" }
    elseif ($schReadyOk -ne "true") { $green = "false"; $finalBlocker = "sch_to_pcb_ready_failed" }
    elseif ($validateHappyOk -ne "true") { $green = "false"; $finalBlocker = "validate_happy_failed" }
    elseif ($validateInvalidOk -ne "true") { $green = "false"; $finalBlocker = "validate_invalid_failed" }
    elseif ($negMissingFilenameOk -ne "true") { $green = "false"; $finalBlocker = "neg_missing_filename_failed" }
    elseif ($negMismatchOk -ne "true") { $green = "false"; $finalBlocker = "neg_mismatch_failed" }
    elseif ($runtimeWorkflowEchoOk -ne "true") { $green = "false"; $finalBlocker = "runtime_workflow_echo_failed" }
    elseif ($hierarchyAssignRuntimeEchoOk -ne "true") { $green = "false"; $finalBlocker = "hierarchy_assign_runtime_echo_failed" }
    elseif ($recommendedAssignScopeEchoOk -ne "true") { $green = "false"; $finalBlocker = "recommended_assign_scope_echo_failed" }

    $hierProj = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_pro"
    $hierSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_sch"
    if ((Test-Path $hierProj) -and (Test-Path $hierSch)) {
        if ($kicadProc -and -not $kicadProc.HasExited) {
            Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
        Start-Sleep -Seconds 2
        $null = Clear-IpcSockets

        $hierSchAbs = (Resolve-Path $hierSch).Path
        $hierFpLibTable = Join-Path (Split-Path $hierSch) "fp-lib-table"
        @(
            "(fp_lib_table",
            "  (version 7)",
            ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA hierarchy footprint readiness"))'),
            ")"
        ) | Set-Content -Path $hierFpLibTable -Encoding ASCII

        $kicadProcHier = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru
        try {
            $resolvedHier = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcHier.Id `
                -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
            if (-not $resolvedHier.Resolved) { throw "ipc_url_unresolved_hierarchy" }
            $urlHier = $resolvedHier.Url
            if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $urlHier -WorkingDirectory $RepoRoot)) {
                throw "project_not_loaded_hierarchy"
            }

            $openHierArgs = @{ type = "sch"; filename = $hierSchAbs } | ConvertTo-Json -Compress
            $openHierInput = Join-Path $runDir "qa_open_editor_frame_hierarchy_sch.json"
            Set-Content -Path $openHierInput -Value $openHierArgs -Encoding UTF8
            $openHierSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($urlHier, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openHierInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            if ($openHierSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame_hierarchy_failed" }
            Start-Sleep -Seconds 3

            $tCurrentOnly = Invoke-FootprintReadiness -ApiUrl $urlHier -ArgsTable @{
                schematic_filename = $hierSchAbs
                scope              = "current_sheet"
            } -LogBaseName "footprint_readiness_hierarchy_current_sheet"

            $tHierarchy = Invoke-FootprintReadiness -ApiUrl $urlHier -ArgsTable @{
                schematic_filename  = $hierSchAbs
                scope               = "hierarchy"
                include_sheet_paths = $true
                include_symbols     = $true
                max_symbols         = 50
            } -LogBaseName "footprint_readiness_hierarchy_full"

            $currentCount = 0
            $hierCount = 0
            $hierInstances = 0
            $sheetsCount = 0
            if ($tCurrentOnly.Parsed.footprint_readiness_symbols_count -match '^[0-9]+$') {
                $currentCount = [int]$tCurrentOnly.Parsed.footprint_readiness_symbols_count
            }
            if ($tHierarchy.Parsed.footprint_readiness_symbols_count -match '^[0-9]+$') {
                $hierCount = [int]$tHierarchy.Parsed.footprint_readiness_symbols_count
            }
            if ($tHierarchy.Parsed.footprint_readiness_symbols_instances_count -match '^[0-9]+$') {
                $hierInstances = [int]$tHierarchy.Parsed.footprint_readiness_symbols_instances_count
            }
            if ($tHierarchy.Parsed.footprint_readiness_sheets_count -match '^[0-9]+$') {
                $sheetsCount = [int]$tHierarchy.Parsed.footprint_readiness_sheets_count
            }

            if ($tHierarchy.Status -eq "ok" `
                    -and $tHierarchy.Parsed.footprint_readiness_ok -eq "true" `
                    -and $tHierarchy.Parsed.footprint_readiness_scope -eq "hierarchy" `
                    -and $sheetsCount -gt 1 `
                    -and $hierInstances -ge $hierCount `
                    -and $hierCount -ge $currentCount `
                    -and $hierCount -ge 3) {
                $hierarchyScopeOk = "true"
                $hierarchyCountsOk = "true"
            }

            if ($tHierarchy.Parsed.footprint_readiness_first_sheet_path -ne "absent" `
                    -and $tHierarchy.Parsed.footprint_readiness_first_instance_path -ne "absent" `
                    -and $tHierarchy.Parsed.footprint_readiness_first_instance_path -match '^/') {
                $hierarchyPathsOk = "true"
            }

            if ($hierCount -eq $hierInstances -and $hierCount -ge 3) {
                $hierarchyNoDoubleCountOk = "true"
            }

            $tHierarchyValidate = Invoke-FootprintReadiness -ApiUrl $urlHier -ArgsTable @{
                schematic_filename  = $hierSchAbs
                scope               = "hierarchy"
                include_sheet_paths = $true
                validate_footprints = $true
                include_symbols     = $true
                max_symbols         = 50
            } -LogBaseName "footprint_readiness_hierarchy_validate"

            if ($tHierarchyValidate.Status -eq "ok" `
                    -and $tHierarchyValidate.Parsed.footprint_readiness_footprint_validation_performed -eq "true" `
                    -and $tHierarchyValidate.Parsed.footprint_readiness_sch_to_pcb_ready -eq "false" `
                    -and $tHierarchyValidate.Parsed.footprint_readiness_sch_to_pcb_ready_validated -eq "false") {
                $hierarchyValidateOk = "true"
            }

            if ((Test-RuntimeWorkflowEchoFields -Parsed $tCurrentOnly.Parsed) `
                    -and (Test-RuntimeWorkflowEchoFields -Parsed $tHierarchy.Parsed) `
                    -and (Test-RuntimeWorkflowEchoFields -Parsed $tHierarchyValidate.Parsed)) {
                $runtimeWorkflowEchoOk = "true"
                $hierarchyAssignRuntimeEchoOk = "true"
                $recommendedAssignScopeEchoOk = "true"
            }
        }
        finally {
            if ($kicadProcHier -and -not $kicadProcHier.HasExited) {
                Stop-Process -Id $kicadProcHier.Id -Force -ErrorAction SilentlyContinue
            }
            Stop-KiCadProcesses
        }
    } else {
        $hierarchyScopeOk = "skipped"
        $hierarchyCountsOk = "skipped"
        $hierarchyPathsOk = "skipped"
        $hierarchyValidateOk = "skipped"
        $hierarchyNoDoubleCountOk = "skipped"
    }

    if ($hierarchyScopeOk -ne "true" -and $hierarchyScopeOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "hierarchy_scope_failed" }
    }
    if ($hierarchyCountsOk -ne "true" -and $hierarchyCountsOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "hierarchy_counts_failed" }
    }
    if ($hierarchyPathsOk -ne "true" -and $hierarchyPathsOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "hierarchy_paths_failed" }
    }
    if ($hierarchyNoDoubleCountOk -ne "true" -and $hierarchyNoDoubleCountOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "hierarchy_double_count_failed" }
    }
    if ($hierarchyValidateOk -ne "true" -and $hierarchyValidateOk -ne "skipped") {
        $green = "false"
        if ($finalBlocker -eq "N/A") { $finalBlocker = "hierarchy_validate_failed" }
    }

    @(
        "schematic_footprint_readiness_green=$green"
        "default_false_unchanged_ok=$defaultUnchangedOk"
        "happy_path_ok=$happyOk"
        "counts_ok=$countsOk"
        "sch_to_pcb_ready_ok=$schReadyOk"
        "validate_happy_ok=$validateHappyOk"
        "validate_invalid_ok=$validateInvalidOk"
        "neg_missing_filename_ok=$negMissingFilenameOk"
        "neg_mismatch_ok=$negMismatchOk"
        "hierarchy_scope_ok=$hierarchyScopeOk"
        "hierarchy_counts_ok=$hierarchyCountsOk"
        "hierarchy_paths_ok=$hierarchyPathsOk"
        "hierarchy_no_double_count_ok=$hierarchyNoDoubleCountOk"
        "hierarchy_validate_ok=$hierarchyValidateOk"
        "runtime_workflow_echo_ok=$runtimeWorkflowEchoOk"
        "hierarchy_assign_runtime_echo_ok=$hierarchyAssignRuntimeEchoOk"
        "recommended_assign_scope_echo_ok=$recommendedAssignScopeEchoOk"
        "exact_blocker_if_any=$finalBlocker"
        "ipc_smoke_ipc_retry_extra_total=$script:ProbeIpcRetryExtraTotal"
        "target_run_dir=$runDir"
        "fixture_schematic=$schAbs"
        "reported_symbols_count=$symCount"
        "reported_symbols_without_footprint_count=$withoutCount"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_footprint_readiness_summary.txt") -Encoding UTF8

    if ($green -ne "true") {
        Write-Host "TARGET_RUN_DIR=$runDir"
        exit 1
    }
}
finally {
    if ($kicadProc -and -not $kicadProc.HasExited) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
    Stop-KiCadProcesses
}

Write-Host "TARGET_RUN_DIR=$runDir"
exit 0
