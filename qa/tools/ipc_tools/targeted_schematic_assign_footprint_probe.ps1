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
$sch      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schWithKnownSymbols = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
if( Test-Path -LiteralPath $schWithKnownSymbols )
{
    $sch = $schWithKnownSymbols
}
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$projectFpLibTable = Join-Path $BuildDir "ipc-v1-proj\fp-lib-table"
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-assign-footprint-targeted")
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

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

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

function Resolve-SymbolIdFromSchItemsText {
    param([string]$ItemsOut)

    if( [string]::IsNullOrWhiteSpace( $ItemsOut ) )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = "none"; Error = "empty_get_sch_items_output" }
    }
    if( $ItemsOut.Trim() -eq "timeout" )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = "none"; Error = "get_sch_items_smoke_timeout" }
    }

    $symbolId = ""
    $method = "none"

    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
    $symbolId = Take-FirstValid $typedSchSymId
    if( $symbolId ) { $method = "typed_sch_symbol_t_id_value" }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $typedSchSymUuid = [regex]::Matches(
            $ItemsOut,
            'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedSchSymUuid
        if( $symbolId ) { $method = "typed_sch_symbol_t_uuid" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $idValueCandidates = [regex]::Matches(
            $ItemsOut,
            'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $idValueCandidates
        if( $symbolId ) { $method = "id_value" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = $method; Error = "symbol_id_not_found_in_get_sch_items_output" }
    }
    return [pscustomobject]@{ SymbolId = $symbolId; Method = $method; Error = "N/A" }
}

function Parse-AssignFootprintResult {
    param([string]$OutputText)

    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if( $OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }
    $changed = "false"
    if( $OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)' ) { $changed = $Matches[1] }
    $dryRun = "false"
    if( $OutputText -match 'key:\s+"dry_run"[\s\S]*?bool_value:\s*(true|false)' ) { $dryRun = $Matches[1] }
    $footprintValidated = "false"
    if( $OutputText -match 'key:\s+"footprint_validated"[\s\S]*?bool_value:\s*(true|false)' ) { $footprintValidated = $Matches[1] }
    $pcbReady = "false"
    if( $OutputText -match 'key:\s+"pcb_ready"[\s\S]*?bool_value:\s*(true|false)' ) { $pcbReady = $Matches[1] }
    $blocker = "absent"
    if( $OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $blocker = $Matches[1] }
    $oldFootprint = ""
    if( $OutputText -match 'key:\s+"old_footprint"[\s\S]*?string_value:\s*"([^"]*)"' ) { $oldFootprint = $Matches[1] }
    $newFootprint = ""
    if( $OutputText -match 'key:\s+"new_footprint"[\s\S]*?string_value:\s*"([^"]*)"' ) { $newFootprint = $Matches[1] }
    $symbolRef = ""
    if( $OutputText -match 'key:\s+"symbol_ref"[\s\S]*?string_value:\s*"([^"]*)"' ) { $symbolRef = $Matches[1] }
    $scope = "absent"
    if( $OutputText -match 'key:\s+"scope"[\s\S]*?string_value:\s*"([^"]*)"' ) { $scope = $Matches[1] }
    $instancePath = "absent"
    if( $OutputText -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]*)"' ) { $instancePath = $Matches[1] }
    $sheetPath = "absent"
    if( $OutputText -match 'key:\s+"sheet_path"[\s\S]*?string_value:\s*"([^"]*)"' ) { $sheetPath = $Matches[1] }
    $instanceTargeted = "absent"
    if( $OutputText -match 'key:\s+"instance_targeted"[\s\S]*?bool_value:\s*(true|false)' ) { $instanceTargeted = $Matches[1] }
    $affectedInstancesCount = "absent"
    if( $OutputText -match 'key:\s+"affected_instances_count"[\s\S]*?number_value:\s*([0-9]+)' ) { $affectedInstancesCount = $Matches[1] }
    $sharedSheetDetected = "absent"
    if( $OutputText -match 'key:\s+"shared_sheet_detected"[\s\S]*?bool_value:\s*(true|false)' ) { $sharedSheetDetected = $Matches[1] }
    $globalUpdate = "absent"
    if( $OutputText -match 'key:\s+"global_update"[\s\S]*?bool_value:\s*(true|false)' ) { $globalUpdate = $Matches[1] }
    $instanceSafeWriteUsed = "absent"
    if( $OutputText -match 'key:\s+"instance_safe_write_used"[\s\S]*?bool_value:\s*(true|false)' ) { $instanceSafeWriteUsed = $Matches[1] }

    return @{
        status_ok = $statusOk
        ok = $ok
        changed = $changed
        dry_run = $dryRun
        footprint_validated = $footprintValidated
        pcb_ready = $pcbReady
        blocker = $blocker
        old_footprint = $oldFootprint
        new_footprint = $newFootprint
        symbol_ref = $symbolRef
        scope = $scope
        instance_path = $instancePath
        sheet_path = $sheetPath
        instance_targeted = $instanceTargeted
        affected_instances_count = $affectedInstancesCount
        shared_sheet_detected = $sharedSheetDetected
        global_update = $globalUpdate
        instance_safe_write_used = $instanceSafeWriteUsed
    }
}

function Invoke-FootprintReadinessSession {
    param(
        [string]$Url,
        [hashtable]$ToolArgs,
        [string]$ArgsPath,
        [string]$LogPath
    )

    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_footprint_readiness_session", ("@" + $ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    return $out
}

function Parse-ReadinessWithoutFootprintTarget {
    param([string]$OutputText)

    $symbolsStart = $OutputText.IndexOf('key: "symbols"')
    if( $symbolsStart -lt 0 ) { return $null }
    $symbolsEnd = $OutputText.IndexOf('key: "symbols_count"', $symbolsStart )
    if( $symbolsEnd -lt 0 ) { return $null }
    $symbolsSection = $OutputText.Substring( $symbolsStart, $symbolsEnd - $symbolsStart )

    $chunks = [regex]::Split( $symbolsSection, '(?=values\s*\{)' )
    foreach( $chunk in $chunks )
    {
        if( $chunk -notmatch 'key:\s+"has_footprint"[\s\S]*?bool_value:\s*false' )
        {
            continue
        }

        $symbolId = ""
        if( $chunk -match 'key:\s+"symbol_id"[\s\S]*?string_value:\s*"([^"]+)"' )
        {
            $symbolId = $Matches[1]
        }
        $instancePath = ""
        if( $chunk -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"' )
        {
            $instancePath = $Matches[1]
        }
        if( $symbolId -and $instancePath )
        {
            return [pscustomobject]@{
                SymbolId = $symbolId
                InstancePath = $instancePath
            }
        }
    }
    return $null
}

function Invoke-AssignFootprintSession {
    param(
        [string]$Url,
        [hashtable]$ToolArgs,
        [string]$ArgsPath,
        [string]$LogPath
    )

    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_assign_footprint_session", ("@" + $ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    return @{
        raw = $out
        parsed = (Parse-AssignFootprintResult -OutputText $out)
    }
}

$newFootprintTarget = "qa_resistors:R_0201_0603Metric"
$missingFootprint = "qa_resistors:DoesNotExist_QA_R120"

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 120 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }
    if( -not $url ) { throw "Unable to resolve API URL from computed IPC candidates" }
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    $ready = $false
    for( $i = 0; $i -lt 60; $i++ )
    {
        $pingRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        if( $pingRes.Text -match 'status=1' ) { $ready = $true; break }
        Start-Sleep -Milliseconds 250
    }
    if( -not $ready ) { throw "KiCad API did not reach AS_OK on ping" }

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $openOut = $openRes.Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if( $openOut -notmatch 'status=1' ) { throw "qa_open_editor_frame did not return AS_OK" }

    $itemsArgs = @{ schematic_filename = $sch; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
    $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
    $itemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $itemsOut = $itemsRes.Text
    $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_symbols_output.log") -Encoding UTF8

    $discovery = Resolve-SymbolIdFromSchItemsText -ItemsOut $itemsOut
    $symbolId = $discovery.SymbolId
    if( [string]::IsNullOrWhiteSpace( $symbolId ) ) { throw "symbol_id discovery failed" }

    $initialDryRun = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $true
        validate_footprint = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_initial_dry_run_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_initial_dry_run_output.log")

    $oldFootprint = $initialDryRun.parsed.old_footprint
    if( [string]::IsNullOrWhiteSpace( $oldFootprint ) ) { throw "initial old_footprint empty on fixture symbol" }

    $dryRunCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $true
        expected_current_footprint = $oldFootprint
        validate_footprint = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_dry_run_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_dry_run_output.log")

    $verifyAfterDryRunCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $true
        expected_current_footprint = $oldFootprint
        validate_footprint = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_verify_after_dry_run_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_verify_after_dry_run_output.log")

    $negInvalidSymbol = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = "not-a-valid-kiid"
        footprint = $newFootprintTarget
        dry_run = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_invalid_symbol_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_invalid_symbol_output.log")

    $negSymbolNotFound = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = "00000000-0000-0000-0000-000000000001"
        footprint = $newFootprintTarget
        dry_run = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_symbol_not_found_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_symbol_not_found_output.log")

    $negInvalidFootprint = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = "qa_resistors:bad<>fp"
        dry_run = $true
        allow_overwrite = $true
        validate_footprint = $false
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_invalid_footprint_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_invalid_footprint_output.log")

    $negFootprintNotFound = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $missingFootprint
        dry_run = $true
        allow_overwrite = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_footprint_not_found_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_footprint_not_found_output.log")

    $negOverwriteRefused = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $false
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_overwrite_refused_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_overwrite_refused_output.log")

    $negExpectedMismatch = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $true
        expected_current_footprint = "WrongFootprint:ExpectedMismatch"
    } -ArgsPath (Join-Path $runDir "assign_footprint_neg_expected_mismatch_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_neg_expected_mismatch_output.log")

    $setCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $false
        allow_overwrite = $true
        expected_current_footprint = $oldFootprint
        validate_footprint = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_mutation_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_mutation_output.log")

    $verifyAfterSetCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $newFootprintTarget
        dry_run = $true
        allow_overwrite = $true
        expected_current_footprint = $newFootprintTarget
        validate_footprint = $true
    } -ArgsPath (Join-Path $runDir "assign_footprint_verify_after_mutation_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_verify_after_mutation_output.log")

    $restoreCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $oldFootprint
        dry_run = $false
        allow_overwrite = $true
        expected_current_footprint = $newFootprintTarget
        validate_footprint = $false
    } -ArgsPath (Join-Path $runDir "assign_footprint_restore_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_restore_output.log")

    $verifyAfterRestoreCall = Invoke-AssignFootprintSession -Url $url -ToolArgs @{
        schematic_filename = $sch
        symbol_id = $symbolId
        footprint = $oldFootprint
        dry_run = $true
        allow_overwrite = $true
        expected_current_footprint = $oldFootprint
        validate_footprint = $false
    } -ArgsPath (Join-Path $runDir "assign_footprint_verify_after_restore_args.json") `
      -LogPath (Join-Path $runDir "assign_footprint_verify_after_restore_output.log")

    $hierarchyDryRunOk = "skipped"
    $hierarchyInstanceSafeOk = "skipped"
    $hierarchyReadbackChangedOk = "skipped"
    $hierarchyNegInvalidScopeOk = "skipped"
    $hierarchyNegMissingInstanceOk = "skipped"
    $hierarchyNegInstanceNotFoundOk = "skipped"
    $hierarchyNegInstanceMismatchOk = "skipped"

    $hierProj = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_pro"
    $hierSch = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_readiness_v1\ipc_footprint_readiness_v1.kicad_sch"
    if( (Test-Path -LiteralPath $hierProj) -and (Test-Path -LiteralPath $hierSch) )
    {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        Stop-KiCadProcesses
        Start-Sleep -Seconds 2
        $null = Clear-IpcSockets

        $hierSchAbs = (Resolve-Path $hierSch).Path
        $hierFpLibTable = Join-Path (Split-Path $hierSch) "fp-lib-table"
        @(
            "(fp_lib_table",
            "  (version 7)",
            ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA hierarchy assign footprint"))'),
            ")"
        ) | Set-Content -Path $hierFpLibTable -Encoding ASCII

        $kicadProcHier = Start-Process -FilePath $kicadExe -ArgumentList $hierProj -WorkingDirectory $RepoRoot -PassThru
        try
        {
            $resolvedHier = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcHier.Id `
                -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
            if( -not $resolvedHier.Resolved ) { throw "ipc_url_unresolved_hierarchy_assign" }
            $urlHier = $resolvedHier.Url

            $readyHier = $false
            for( $i = 0; $i -lt 60; $i++ )
            {
                $pingResHier = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($urlHier, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
                if( $pingResHier.Text -match 'status=1' ) { $readyHier = $true; break }
                Start-Sleep -Milliseconds 250
            }
            if( -not $readyHier ) { throw "KiCad API did not reach AS_OK on ping (hierarchy assign)" }

            $openHierArgs = @{ type = "sch"; filename = $hierSchAbs } | ConvertTo-Json -Compress
            $openHierInput = Join-Path $runDir "qa_open_editor_frame_hierarchy_assign.json"
            Set-Content -Path $openHierInput -Value $openHierArgs -Encoding UTF8
            $openHierRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($urlHier, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openHierInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
            if( $openHierRes.Text -notmatch 'status=1' ) { throw "qa_open_editor_frame_hierarchy_assign_failed" }

            $readinessBeforeOut = Invoke-FootprintReadinessSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                scope = "hierarchy"
                include_sheet_paths = $true
                include_symbols = $true
                max_symbols = 50
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_readiness_before_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_readiness_before_output.log")

            $withoutTarget = Parse-ReadinessWithoutFootprintTarget -OutputText $readinessBeforeOut
            if( -not $withoutTarget ) { throw "hierarchy_without_footprint_target_not_found" }

            $hierTargetSymbolId = $withoutTarget.SymbolId
            $hierTargetInstancePath = $withoutTarget.InstancePath
            $hierAssignFootprint = "Resistor_SMD:R_0201_0603Metric"

            $hierDryRun = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                instance_path = $hierTargetInstancePath
                footprint = $hierAssignFootprint
                scope = "hierarchy"
                dry_run = $true
                allow_overwrite = $true
                validate_footprint = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_dry_run_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_dry_run_output.log")

            if( $hierDryRun.parsed.status_ok -and $hierDryRun.parsed.ok -eq "true" `
                    -and $hierDryRun.parsed.scope -eq "hierarchy" `
                    -and $hierDryRun.parsed.instance_path -eq $hierTargetInstancePath `
                    -and $hierDryRun.parsed.changed -eq "true" `
                    -and $hierDryRun.parsed.global_update -eq "false" `
                    -and $hierDryRun.parsed.blocker -eq "N/A" )
            {
                $hierarchyDryRunOk = "true"
            }
            else
            {
                $hierarchyDryRunOk = "false"
            }

            $hierMutation = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                instance_path = $hierTargetInstancePath
                footprint = $hierAssignFootprint
                scope = "hierarchy"
                dry_run = $false
                allow_overwrite = $true
                validate_footprint = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_mutation_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_mutation_output.log")

            $readinessAfterOut = Invoke-FootprintReadinessSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                scope = "hierarchy"
                include_sheet_paths = $true
                include_symbols = $true
                max_symbols = 50
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_readiness_after_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_readiness_after_output.log")

            if( $hierMutation.parsed.status_ok -and $hierMutation.parsed.ok -eq "true" `
                    -and $hierMutation.parsed.changed -eq "true" `
                    -and $hierMutation.parsed.scope -eq "hierarchy" `
                    -and $hierMutation.parsed.global_update -eq "false" `
                    -and $hierMutation.parsed.instance_safe_write_used -eq "true" `
                    -and $hierMutation.parsed.blocker -eq "N/A" )
            {
                $hierarchyInstanceSafeOk = "true"
            }
            else
            {
                $hierarchyInstanceSafeOk = "false"
            }

            if( $readinessAfterOut -ne $readinessBeforeOut )
            {
                $hierarchyReadbackChangedOk = "true"
            }
            else
            {
                $hierarchyReadbackChangedOk = "false"
            }

            $hierRestore = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                instance_path = $hierTargetInstancePath
                footprint = ""
                scope = "hierarchy"
                dry_run = $false
                allow_overwrite = $true
                validate_footprint = $false
                expected_current_footprint = $hierAssignFootprint
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_restore_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_restore_output.log")

            if( $hierRestore.parsed.status_ok -and $hierRestore.parsed.ok -ne "true" )
            {
                $hierarchyInstanceSafeOk = "false"
            }

            $negInvalidScope = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                instance_path = $hierTargetInstancePath
                footprint = $hierAssignFootprint
                scope = "bad_scope"
                dry_run = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_neg_invalid_scope_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_neg_invalid_scope_output.log")

            $negMissingInstance = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                footprint = $hierAssignFootprint
                scope = "hierarchy"
                dry_run = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_neg_missing_instance_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_neg_missing_instance_output.log")

            $negInstanceNotFound = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = $hierTargetSymbolId
                instance_path = "/00000000-0000-0000-0000-000000000099/00000000-0000-0000-0000-000000000098"
                footprint = $hierAssignFootprint
                scope = "hierarchy"
                dry_run = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_neg_instance_not_found_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_neg_instance_not_found_output.log")

            $negInstanceMismatch = Invoke-AssignFootprintSession -Url $urlHier -ToolArgs @{
                schematic_filename = $hierSchAbs
                symbol_id = "00000000-0000-0000-0000-000000000001"
                instance_path = $hierTargetInstancePath
                footprint = $hierAssignFootprint
                scope = "hierarchy"
                dry_run = $true
            } -ArgsPath (Join-Path $runDir "assign_hierarchy_neg_instance_mismatch_args.json") `
              -LogPath (Join-Path $runDir "assign_hierarchy_neg_instance_mismatch_output.log")

            if( $negInvalidScope.parsed.status_ok -and $negInvalidScope.parsed.ok -eq "false" `
                    -and $negInvalidScope.parsed.blocker -eq "invalid_scope" )
            {
                $hierarchyNegInvalidScopeOk = "true"
            }
            else { $hierarchyNegInvalidScopeOk = "false" }

            if( $negMissingInstance.parsed.status_ok -and $negMissingInstance.parsed.ok -eq "false" `
                    -and $negMissingInstance.parsed.blocker -eq "missing_instance_selector" )
            {
                $hierarchyNegMissingInstanceOk = "true"
            }
            else { $hierarchyNegMissingInstanceOk = "false" }

            if( $negInstanceNotFound.parsed.status_ok -and $negInstanceNotFound.parsed.ok -eq "false" `
                    -and $negInstanceNotFound.parsed.blocker -eq "instance_path_not_found" )
            {
                $hierarchyNegInstanceNotFoundOk = "true"
            }
            else { $hierarchyNegInstanceNotFoundOk = "false" }

            if( $negInstanceMismatch.parsed.status_ok -and $negInstanceMismatch.parsed.ok -eq "false" `
                    -and $negInstanceMismatch.parsed.blocker -eq "symbol_instance_mismatch" )
            {
                $hierarchyNegInstanceMismatchOk = "true"
            }
            else { $hierarchyNegInstanceMismatchOk = "false" }
        }
        finally
        {
            if( $kicadProcHier -and -not $kicadProcHier.HasExited )
            {
                Stop-Process -Id $kicadProcHier.Id -Force -ErrorAction SilentlyContinue
            }
            Stop-KiCadProcesses
        }
    }

    $sharedSheetBlockedOk = "skipped"
    $sharedSheetOptInOk = "skipped"
    $sharedSheetReadbackUnchangedOk = "skipped"
    $sharedSheetHierarchyMutationOk = "skipped"

    $spikeFixtureSrc = Join-Path $RepoRoot "qa\data\eeschema\hierarchy\ipc_footprint_instance_write_spike"
    if( Test-Path -LiteralPath $spikeFixtureSrc )
    {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        Stop-KiCadProcesses
        Start-Sleep -Seconds 2
        $null = Clear-IpcSockets

        $spikeRunDir = Join-Path $runDir "shared-sheet-r146"
        New-Item -ItemType Directory -Force -Path $spikeRunDir | Out-Null
        Copy-Item -Path $spikeFixtureSrc -Destination (Join-Path $spikeRunDir "fixture") -Recurse -Force
        $spikeFixtureDir = Join-Path $spikeRunDir "fixture"
        $spikeProj = Join-Path $spikeFixtureDir "ipc_footprint_instance_write_spike.kicad_pro"
        $spikeRootSch = (Resolve-Path (Join-Path $spikeFixtureDir "ipc_footprint_instance_write_spike.kicad_sch")).Path
        $spikeSubSch = (Resolve-Path (Join-Path $spikeFixtureDir "sub_shared.kicad_sch")).Path
        $spikeSymbolId = "c1000005-0005-4005-8005-000000000005"
        $spikeFootprint = "Resistor_SMD:R_0201_0603Metric"
        $spikeFpLibTable = Join-Path $spikeFixtureDir "fp-lib-table"
        @(
            "(fp_lib_table",
            "  (version 7)",
            ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "R146 shared sheet guard"))'),
            ")"
        ) | Set-Content -Path $spikeFpLibTable -Encoding ASCII

        $kicadProcShared = Start-Process -FilePath $kicadExe -ArgumentList $spikeProj -WorkingDirectory $spikeFixtureDir -PassThru
        try
        {
            $resolvedShared = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProcShared.Id `
                -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
            if( -not $resolvedShared.Resolved ) { throw "ipc_url_unresolved_shared_sheet_r146" }
            $urlShared = $resolvedShared.Url
            if( -not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $urlShared -WorkingDirectory $RepoRoot) )
            {
                throw "project_not_loaded_shared_sheet_r146"
            }

            $openRootShared = @{ type = "sch"; filename = $spikeRootSch } | ConvertTo-Json -Compress
            $openRootSharedInput = Join-Path $spikeRunDir "open_root_shared.json"
            Set-Content -Path $openRootSharedInput -Value $openRootShared -Encoding UTF8
            $openRootSharedRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($urlShared, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openRootSharedInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            if( $openRootSharedRes.Text -notmatch 'status=1' ) { throw "qa_open_editor_frame_shared_root_failed" }

            $readinessSharedBefore = Invoke-FootprintReadinessSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeRootSch
                scope = "hierarchy"
                include_sheet_paths = $true
                include_symbols = $true
                max_symbols = 50
            } -ArgsPath (Join-Path $spikeRunDir "shared_readiness_before_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_readiness_before_output.log")

            $sharedInstanceA = ""
            $sharedInstanceB = ""
            if( $readinessSharedBefore -match 'key:\s+"instance_path"[\s\S]*?string_value:\s*"([^"]+)"' )
            {
                $sharedInstanceA = $Matches[1]
            }

            $sharedSheetHierarchyMutationOk = "skipped"

            $openSubShared = @{ type = "sch"; filename = $spikeSubSch } | ConvertTo-Json -Compress
            $openSubSharedInput = Join-Path $spikeRunDir "open_sub_shared.json"
            Set-Content -Path $openSubSharedInput -Value $openSubShared -Encoding UTF8
            $openSubSharedRes = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($urlShared, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openSubSharedInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            if( $openSubSharedRes.Text -notmatch 'status=1' ) { throw "qa_open_editor_frame_shared_sub_failed" }

            $sharedBlocked = Invoke-AssignFootprintSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeSubSch
                symbol_id = $spikeSymbolId
                footprint = $spikeFootprint
                scope = "current_sheet"
                dry_run = $false
                allow_overwrite = $true
                validate_footprint = $false
            } -ArgsPath (Join-Path $spikeRunDir "shared_blocked_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_blocked_output.log")

            $readinessSharedAfterBlocked = Invoke-FootprintReadinessSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeSubSch
                scope = "hierarchy"
                include_sheet_paths = $true
                include_symbols = $true
                max_symbols = 50
            } -ArgsPath (Join-Path $spikeRunDir "shared_readiness_after_blocked_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_readiness_after_blocked_sub_output.log")

            $sharedVerifyDryRun = Invoke-AssignFootprintSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeSubSch
                symbol_id = $spikeSymbolId
                footprint = $spikeFootprint
                scope = "current_sheet"
                dry_run = $true
                allow_overwrite = $true
                validate_footprint = $false
            } -ArgsPath (Join-Path $spikeRunDir "shared_verify_dry_run_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_verify_dry_run_output.log")

            if( $sharedBlocked.parsed.status_ok -and $sharedBlocked.parsed.ok -eq "false" `
                    -and $sharedBlocked.parsed.blocker -eq "shared_symbol_global_update_blocked" `
                    -and [int]$sharedBlocked.parsed.affected_instances_count -ge 2 `
                    -and $sharedBlocked.parsed.shared_sheet_detected -eq "true" `
                    -and $sharedBlocked.parsed.global_update -eq "true" `
                    -and $sharedBlocked.parsed.changed -eq "false" )
            {
                $sharedSheetBlockedOk = "true"
            }
            else { $sharedSheetBlockedOk = "false" }

            if( $sharedVerifyDryRun.parsed.status_ok -and $sharedVerifyDryRun.parsed.ok -eq "true" `
                    -and $sharedVerifyDryRun.parsed.old_footprint -eq "" )
            {
                $sharedSheetReadbackUnchangedOk = "true"
            }
            else { $sharedSheetReadbackUnchangedOk = "false" }

            $sharedOptIn = Invoke-AssignFootprintSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeSubSch
                symbol_id = $spikeSymbolId
                footprint = $spikeFootprint
                scope = "current_sheet"
                dry_run = $false
                allow_overwrite = $true
                validate_footprint = $false
                allow_shared_symbol_global_update = $true
            } -ArgsPath (Join-Path $spikeRunDir "shared_opt_in_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_opt_in_output.log")

            $sharedVerifyAfterOptIn = Invoke-AssignFootprintSession -Url $urlShared -ToolArgs @{
                schematic_filename = $spikeSubSch
                symbol_id = $spikeSymbolId
                footprint = $spikeFootprint
                scope = "current_sheet"
                dry_run = $true
                allow_overwrite = $true
                validate_footprint = $false
                expected_current_footprint = $spikeFootprint
            } -ArgsPath (Join-Path $spikeRunDir "shared_verify_after_opt_in_args.json") `
              -LogPath (Join-Path $spikeRunDir "shared_verify_after_opt_in_output.log")

            if( $sharedOptIn.parsed.status_ok -and $sharedOptIn.parsed.ok -eq "true" `
                    -and $sharedOptIn.parsed.changed -eq "true" `
                    -and [int]$sharedOptIn.parsed.affected_instances_count -ge 2 `
                    -and $sharedVerifyAfterOptIn.parsed.status_ok -and $sharedVerifyAfterOptIn.parsed.ok -eq "true" `
                    -and $sharedVerifyAfterOptIn.parsed.old_footprint -eq $spikeFootprint )
            {
                $sharedSheetOptInOk = "true"
            }
            else { $sharedSheetOptInOk = "false" }
        }
        finally
        {
            if( $kicadProcShared -and -not $kicadProcShared.HasExited )
            {
                Stop-Process -Id $kicadProcShared.Id -Force -ErrorAction SilentlyContinue
            }
            Stop-KiCadProcesses
        }
    }

    $dryRunOk = if( $dryRunCall.parsed.status_ok -and $dryRunCall.parsed.ok -eq "true" -and $dryRunCall.parsed.changed -eq "true" -and $dryRunCall.parsed.footprint_validated -eq "true" -and $dryRunCall.parsed.pcb_ready -eq "true" ) { "true" } else { "false" }
    $dryRunNoMutationOk = if( $verifyAfterDryRunCall.parsed.status_ok -and $verifyAfterDryRunCall.parsed.ok -eq "true" -and $verifyAfterDryRunCall.parsed.changed -eq "true" -and $verifyAfterDryRunCall.parsed.old_footprint -eq $oldFootprint ) { "true" } else { "false" }
    $mutationOk = if( $setCall.parsed.status_ok -and $setCall.parsed.ok -eq "true" -and $setCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterMutationOk = if( $verifyAfterSetCall.parsed.status_ok -and $verifyAfterSetCall.parsed.ok -eq "true" -and $verifyAfterSetCall.parsed.old_footprint -eq $newFootprintTarget ) { "true" } else { "false" }
    $restoreOk = if( $restoreCall.parsed.status_ok -and $restoreCall.parsed.ok -eq "true" -and $restoreCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterRestoreOk = if( $verifyAfterRestoreCall.parsed.status_ok -and $verifyAfterRestoreCall.parsed.ok -eq "true" -and $verifyAfterRestoreCall.parsed.old_footprint -eq $oldFootprint ) { "true" } else { "false" }
    $footprintValidatedOk = if( $setCall.parsed.footprint_validated -eq "true" ) { "true" } else { "false" }
    $pcbReadyOk = if( $setCall.parsed.pcb_ready -eq "true" ) { "true" } else { "false" }

    $negativeInvalidSymbolOk = if( $negInvalidSymbol.parsed.status_ok -and $negInvalidSymbol.parsed.ok -eq "false" -and $negInvalidSymbol.parsed.blocker -eq "invalid_symbol_id" ) { "true" } else { "false" }
    $negativeSymbolNotFoundOk = if( $negSymbolNotFound.parsed.status_ok -and $negSymbolNotFound.parsed.ok -eq "false" -and $negSymbolNotFound.parsed.blocker -eq "symbol_not_found" ) { "true" } else { "false" }
    $negativeInvalidFootprintOk = if( $negInvalidFootprint.parsed.status_ok -and $negInvalidFootprint.parsed.ok -eq "false" -and $negInvalidFootprint.parsed.blocker -eq "invalid_footprint_identifier" ) { "true" } else { "false" }
    $negativeFootprintNotFoundOk = if( $negFootprintNotFound.parsed.status_ok -and $negFootprintNotFound.parsed.ok -eq "false" -and $negFootprintNotFound.parsed.blocker -like "footprint_not_found_in_library:*" ) { "true" } else { "false" }
    $negativeOverwriteRefusedOk = if( $negOverwriteRefused.parsed.status_ok -and $negOverwriteRefused.parsed.ok -eq "false" -and $negOverwriteRefused.parsed.blocker -eq "footprint_already_assigned" ) { "true" } else { "false" }
    $negativeExpectedMismatchOk = if( $negExpectedMismatch.parsed.status_ok -and $negExpectedMismatch.parsed.ok -eq "false" -and $negExpectedMismatch.parsed.blocker -eq "expected_current_footprint_mismatch" ) { "true" } else { "false" }

    $blocker = "N/A"
    if( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoMutationOk -ne "true" ) { $blocker = "dry_run_no_mutation_failed" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_failed" }
    elseif( $readbackAfterMutationOk -ne "true" ) { $blocker = "readback_after_mutation_failed" }
    elseif( $restoreOk -ne "true" ) { $blocker = "restore_failed" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "readback_after_restore_failed" }
    elseif( $footprintValidatedOk -ne "true" ) { $blocker = "footprint_validated_not_ok" }
    elseif( $pcbReadyOk -ne "true" ) { $blocker = "pcb_ready_not_ok" }
    elseif( $negativeInvalidSymbolOk -ne "true" ) { $blocker = "negative_invalid_symbol_id_failed" }
    elseif( $negativeSymbolNotFoundOk -ne "true" ) { $blocker = "negative_symbol_not_found_failed" }
    elseif( $negativeInvalidFootprintOk -ne "true" ) { $blocker = "negative_invalid_footprint_failed" }
    elseif( $negativeFootprintNotFoundOk -ne "true" ) { $blocker = "negative_footprint_not_found_failed" }
    elseif( $negativeOverwriteRefusedOk -ne "true" ) { $blocker = "negative_overwrite_refused_failed" }
    elseif( $negativeExpectedMismatchOk -ne "true" ) { $blocker = "negative_expected_current_mismatch_failed" }
    elseif( $hierarchyDryRunOk -ne "true" -and $hierarchyDryRunOk -ne "skipped" ) { $blocker = "hierarchy_dry_run_failed" }
    elseif( $hierarchyInstanceSafeOk -ne "true" -and $hierarchyInstanceSafeOk -ne "skipped" ) { $blocker = "hierarchy_instance_safe_failed" }
    elseif( $hierarchyReadbackChangedOk -ne "true" -and $hierarchyReadbackChangedOk -ne "skipped" ) { $blocker = "hierarchy_readback_unchanged" }
    elseif( $hierarchyNegInvalidScopeOk -ne "true" -and $hierarchyNegInvalidScopeOk -ne "skipped" ) { $blocker = "hierarchy_neg_invalid_scope_failed" }
    elseif( $hierarchyNegMissingInstanceOk -ne "true" -and $hierarchyNegMissingInstanceOk -ne "skipped" ) { $blocker = "hierarchy_neg_missing_instance_failed" }
    elseif( $hierarchyNegInstanceNotFoundOk -ne "true" -and $hierarchyNegInstanceNotFoundOk -ne "skipped" ) { $blocker = "hierarchy_neg_instance_not_found_failed" }
    elseif( $hierarchyNegInstanceMismatchOk -ne "true" -and $hierarchyNegInstanceMismatchOk -ne "skipped" ) { $blocker = "hierarchy_neg_instance_mismatch_failed" }
    elseif( $sharedSheetBlockedOk -ne "true" -and $sharedSheetBlockedOk -ne "skipped" ) { $blocker = "shared_sheet_blocked_failed" }
    elseif( $sharedSheetReadbackUnchangedOk -ne "true" -and $sharedSheetReadbackUnchangedOk -ne "skipped" ) { $blocker = "shared_sheet_readback_changed" }
    elseif( $sharedSheetOptInOk -ne "true" -and $sharedSheetOptInOk -ne "skipped" ) { $blocker = "shared_sheet_opt_in_failed" }
    elseif( $sharedSheetHierarchyMutationOk -ne "true" -and $sharedSheetHierarchyMutationOk -ne "skipped" ) { $blocker = "shared_sheet_hierarchy_mutation_failed" }

    $familyGreen = ( $blocker -eq "N/A" )

    @(
        "schematic_assign_footprint_green=" + $(if($familyGreen){"true"}else{"false"})
        "dry_run_ok=$dryRunOk"
        "dry_run_no_mutation_ok=$dryRunNoMutationOk"
        "mutation_ok=$mutationOk"
        "readback_after_mutation_ok=$readbackAfterMutationOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "footprint_validated_ok=$footprintValidatedOk"
        "pcb_ready_ok=$pcbReadyOk"
        "negative_invalid_symbol_id_ok=$negativeInvalidSymbolOk"
        "negative_symbol_not_found_ok=$negativeSymbolNotFoundOk"
        "negative_invalid_footprint_ok=$negativeInvalidFootprintOk"
        "negative_footprint_not_found_ok=$negativeFootprintNotFoundOk"
        "negative_overwrite_refused_ok=$negativeOverwriteRefusedOk"
        "negative_expected_current_mismatch_ok=$negativeExpectedMismatchOk"
        "hierarchy_dry_run_ok=$hierarchyDryRunOk"
        "hierarchy_instance_safe_ok=$hierarchyInstanceSafeOk"
        "hierarchy_readback_changed_ok=$hierarchyReadbackChangedOk"
        "hierarchy_neg_invalid_scope_ok=$hierarchyNegInvalidScopeOk"
        "hierarchy_neg_missing_instance_ok=$hierarchyNegMissingInstanceOk"
        "hierarchy_neg_instance_not_found_ok=$hierarchyNegInstanceNotFoundOk"
        "hierarchy_neg_instance_mismatch_ok=$hierarchyNegInstanceMismatchOk"
        "shared_sheet_blocked_ok=$sharedSheetBlockedOk"
        "shared_sheet_readback_unchanged_ok=$sharedSheetReadbackUnchangedOk"
        "shared_sheet_opt_in_ok=$sharedSheetOptInOk"
        "shared_sheet_hierarchy_mutation_ok=$sharedSheetHierarchyMutationOk"
        "exact_blocker_if_any=$blocker"
        "symbol_id=$symbolId"
        "symbol_ref=$($dryRunCall.parsed.symbol_ref)"
        "old_footprint=$oldFootprint"
        "new_footprint=$newFootprintTarget"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_assign_footprint_session_contract_summary.txt") -Encoding UTF8

    if( -not $familyGreen ) { throw "schematic_assign_footprint_green=false exact_blocker_if_any=$blocker" }

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
