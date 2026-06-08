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
$projIssue19090 = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_pro"
$sch      = $schBuild
$fixtureSource = "build_v1"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $schBuild)) { throw "Missing schematic at $schBuild" }
# When using the issue19090 schematic, open its project so Manager + Kiway match the file
# (opening a foreign .kicad_sch under a different .kicad_pro can block or fail in GUI).
if( (Test-Path -LiteralPath $schIssue19090) -and (Test-Path -LiteralPath $projIssue19090) )
{
    $sch = $schIssue19090
    $proj = $projIssue19090
    $fixtureSource = "issue19090"
}
elseif( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-move-symbol-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$diagTempLog = Join-Path $env:TEMP "kicad\schematic_move_symbol_diag.log"

function Copy-MoveDiagLog {
    param([string]$DestinationDir)
    if( Test-Path -LiteralPath $diagTempLog )
    {
        Copy-Item -LiteralPath $diagTempLog -Destination (Join-Path $DestinationDir "schematic_move_symbol_diag.log") -Force
    }
}

function Write-BootstrapTimeoutSummary {
    param(
        [string]$SummaryPath,
        [string]$Visible,
        [string]$SymbolId,
        [string]$FixtureSource,
        [string]$SchematicPath
    )
    @(
        "visible_in_context_if_applicable=$Visible"
        "callable_with_mutation=false"
        "symbol_id_discovery_ok=true"
        "dry_run_ok=false"
        "dry_run_no_change_ok=false"
        "mutation_ok=false"
        "readback_after_move_ok=false"
        "restore_ok=false"
        "readback_after_restore_ok=false"
        "exact_blocker_if_any=bootstrap_timeout"
        "tool_exact_blocker_field=absent"
        "symbol_id=$SymbolId"
        "old_position_x_nm="
        "old_position_y_nm="
        "temporary_position_x_nm="
        "temporary_position_y_nm="
        "restored_position_x_nm="
        "restored_position_y_nm="
        "connectivity_refresh_performed=false"
        "fixture_source=$FixtureSource"
        "fixture_schematic=$SchematicPath"
        ""
    ) | Set-Content -Path $SummaryPath -Encoding UTF8
}

function Write-OpenEditorFailureSummary {
    param(
        [string]$SummaryPath,
        [string]$Visible,
        [string]$FixtureSource,
        [string]$SchematicPath
    )
    @(
        "visible_in_context_if_applicable=$Visible"
        "callable_with_mutation=false"
        "symbol_id_discovery_ok=false"
        "dry_run_ok=false"
        "dry_run_no_change_ok=false"
        "mutation_ok=false"
        "readback_after_move_ok=false"
        "restore_ok=false"
        "readback_after_restore_ok=false"
        "exact_blocker_if_any=open_editor_timeout"
        "tool_exact_blocker_field=absent"
        "symbol_id="
        "old_position_x_nm="
        "old_position_y_nm="
        "temporary_position_x_nm="
        "temporary_position_y_nm="
        "restored_position_x_nm="
        "restored_position_y_nm="
        "connectivity_refresh_performed=false"
        "fixture_source=$FixtureSource"
        "fixture_schematic=$SchematicPath"
        ""
    ) | Set-Content -Path $SummaryPath -Encoding UTF8
}

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

    $symbolId = ""
    $method = "none"

    $typedSchSymId = [regex]::Matches(
        $ItemsOut,
        'SCH_SYMBOL_T[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
    $symbolId = Take-FirstValid $typedSchSymId
    if( $symbolId ) { $method = "typed_sch_symbol_t_id_value" }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        $typedKotId = [regex]::Matches(
            $ItemsOut,
            'KOT_SCH_SYMBOL[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $symbolId = Take-FirstValid $typedKotId
        if( $symbolId ) { $method = "typed_kot_sch_symbol_id_value" }
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
        $flatCandidates = [regex]::Matches(
            $ItemsOut,
            '(?:\bid=|\buuid=)([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})' )
        $symbolId = Take-FirstValid $flatCandidates
        if( $symbolId ) { $method = "flat_id_or_uuid" }
    }

    if( [string]::IsNullOrWhiteSpace( $symbolId ) )
    {
        return [pscustomobject]@{ SymbolId = ""; Method = $method; Error = "symbol_id_not_found_in_get_sch_items_output" }
    }
    return [pscustomobject]@{ SymbolId = $symbolId; Method = $method; Error = "N/A" }
}

function Get-BoolField {
    param([string]$Text,[string]$Field,[string]$Default="false")
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?bool_value:\s*(true|false)') ) { return $Matches[1] }
    return $Default
}

function Get-StringField {
    param([string]$Text,[string]$Field,[string]$Default="")
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?string_value:\s*"([^"]*)"') ) { return $Matches[1] }
    return $Default
}

function Get-PositionField {
    param([string]$Text,[string]$Field)
    $rx = 'key:\s+"' + [regex]::Escape($Field) + '"[\s\S]{0,1200}?key:\s+"x_nm"[\s\S]{0,300}?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)' +
          '[\s\S]{0,600}?key:\s+"y_nm"[\s\S]{0,300}?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)'
    if( $Text -match $rx )
    {
        return [pscustomobject]@{
            ok = $true
            x_nm = [int64][double]$Matches[1]
            y_nm = [int64][double]$Matches[2]
        }
    }
    return [pscustomobject]@{ ok = $false; x_nm = 0; y_nm = 0 }
}

function Get-ScalarPositionField {
    param([string]$Text,[string]$Prefix)
    $x = $null
    $y = $null
    if( $Text -match ('key:\s+"' + [regex]::Escape($Prefix + "_x_nm") + '"[\s\S]*?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)') )
    {
        $x = [int64][double]$Matches[1]
    }
    if( $Text -match ('key:\s+"' + [regex]::Escape($Prefix + "_y_nm") + '"[\s\S]*?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)') )
    {
        $y = [int64][double]$Matches[1]
    }
    if( $null -ne $x -and $null -ne $y )
    {
        return [pscustomobject]@{ ok = $true; x_nm = $x; y_nm = $y }
    }
    return [pscustomobject]@{ ok = $false; x_nm = 0; y_nm = 0 }
}

function Get-MovePositionField {
    param([string]$Text,[string]$Field)
    $nested = Get-PositionField -Text $Text -Field $Field
    if( $nested.ok ) { return $nested }
    return Get-ScalarPositionField -Text $Text -Prefix $Field
}

function Invoke-MoveTool {
    param(
        [string]$Url,
        [string]$ArgsFilePath,
        [hashtable]$ArgsObject,
        [string]$OutputLogPath
    )
    ($ArgsObject | ConvertTo-Json -Compress) | Set-Content -Path $ArgsFilePath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_move_symbol_session", ("@" + $ArgsFilePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $OutputLogPath -Encoding UTF8
    return $out
}

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

if( Test-Path -LiteralPath $diagTempLog ) { Remove-Item -LiteralPath $diagTempLog -Force -ErrorAction SilentlyContinue }
$previousDiagEnv = $env:KICAD_IPC_STARTUP_DIAG
$env:KICAD_IPC_STARTUP_DIAG = "1"
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
        $pingOut = $pingRes.Text
        $pingOut | Set-Content -Path (Join-Path $runDir "ping_probe.log") -Encoding UTF8
        if( $pingOut -match 'status=1' )
        {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 250
    }
    if( -not $ready ) { throw "KiCad API did not reach AS_OK on ping" }

    if( -not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot) )
    {
        throw "MCP get_project_info did not report loaded=true before qa_open_editor_frame"
    }

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = ""
    $openAttempts = 0
    $openFinalStatus = "failed"
    $openLastBlocker = "open_editor_unknown"
    for( $attempt = 1; $attempt -le 2; $attempt++ )
    {
        $openAttempts = $attempt
        $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
        $openOut = $openRes.Text
        $openOut | Set-Content -Path (Join-Path $runDir ("qa_open_editor_frame_attempt" + $attempt + "_output.log")) -Encoding UTF8
        $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8

        if( $openOut -match 'status=1' )
        {
            $openFinalStatus = "ok"
            $openLastBlocker = "N/A"
            break
        }

        if( $openOut.Trim() -eq "timeout" )
        {
            $openLastBlocker = "open_editor_timeout"
        }
        else
        {
            $openLastBlocker = "open_editor_not_ok"
        }

        if( $attempt -lt 2 )
        {
            Start-Sleep -Milliseconds 750
        }
    }

    @(
        "attempts=$openAttempts"
        "final_status=$openFinalStatus"
        "last_blocker=$openLastBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_attempts.txt") -Encoding UTF8

    if( $openFinalStatus -ne "ok" )
    {
        $visible = if( $lt -match 'schematic_move_symbol_session' ) { "true" } else { "false" }
        Write-OpenEditorFailureSummary -SummaryPath (Join-Path $runDir "qa_schematic_move_symbol_session_contract_summary.txt") `
            -Visible $visible -FixtureSource $fixtureSource -SchematicPath $sch
        Copy-MoveDiagLog -DestinationDir $runDir
        Write-TargetRunDir -RunDir $runDir
        return
    }

    $itemsArgs = @{ schematic_filename = $sch; types = @("KOT_SCH_SYMBOL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_symbols_args.json"
    $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
    $itemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $itemsOut = $itemsRes.Text
    $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_symbols_output.log") -Encoding UTF8

    $discovery = Resolve-SymbolIdFromSchItemsText -ItemsOut $itemsOut
    $symbolId = $discovery.SymbolId
    $symbolIdDiscoveryOk = if( $symbolId ) { "true" } else { "false" }
    if( [string]::IsNullOrWhiteSpace( $symbolId ) ) { throw "symbol_id discovery failed from get_sch_items output" }

    $bootstrapArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_position = @{ x_nm = 0; y_nm = 0 }
        dry_run = $true
    }
    $bootstrapOut = Invoke-MoveTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_move_symbol_bootstrap_args.json") -ArgsObject $bootstrapArgs -OutputLogPath (Join-Path $runDir "schematic_move_symbol_bootstrap_output.log")
    if( $bootstrapOut.Trim() -eq "timeout" )
    {
        $visible = if( $lt -match 'schematic_move_symbol_session' ) { "true" } else { "false" }
        Write-BootstrapTimeoutSummary -SummaryPath (Join-Path $runDir "qa_schematic_move_symbol_session_contract_summary.txt") `
            -Visible $visible -SymbolId $symbolId -FixtureSource $fixtureSource -SchematicPath $sch
        Copy-MoveDiagLog -DestinationDir $runDir
        Write-TargetRunDir -RunDir $runDir
        return
    }
    if( $bootstrapOut -notmatch 'status=1' ) { throw "bootstrap dry_run call failed" }

    $oldPos = Get-MovePositionField -Text $bootstrapOut -Field "old_position"
    if( -not $oldPos.ok ) { throw "failed to parse old_position from bootstrap output" }
    $oldXNm = [int64]$oldPos.x_nm
    $oldYNm = [int64]$oldPos.y_nm

    $temporaryXNm = $oldXNm + 1000000
    $temporaryYNm = $oldYNm

    $dryRunArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_position = @{ x_nm = $temporaryXNm; y_nm = $temporaryYNm }
        expected_old_position = @{ x_nm = $oldXNm; y_nm = $oldYNm }
        dry_run = $true
        ensure_fresh_connectivity = $true
        position_tolerance_nm = 0
    }
    $dryRunOut = Invoke-MoveTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_move_symbol_dry_run_args.json") -ArgsObject $dryRunArgs -OutputLogPath (Join-Path $runDir "schematic_move_symbol_dry_run_output.log")

    $dryRunOk = Get-BoolField -Text $dryRunOut -Field "ok" -Default "false"
    $dryRunChanged = Get-BoolField -Text $dryRunOut -Field "changed" -Default "false"
    $dryRunReadback = Get-MovePositionField -Text $dryRunOut -Field "readback_position"
    $dryRunNoChangeOk = if( $dryRunReadback.ok -and $dryRunReadback.x_nm -eq $oldXNm -and $dryRunReadback.y_nm -eq $oldYNm ) { "true" } else { "false" }
    $dryRunConnectivity = Get-BoolField -Text $dryRunOut -Field "connectivity_refresh_performed" -Default "true"
    if( $dryRunConnectivity -eq "true" ) { $dryRunNoChangeOk = "false" }
    if( $dryRunChanged -ne "true" ) { $dryRunNoChangeOk = "false" }

    $moveArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_position = @{ x_nm = $temporaryXNm; y_nm = $temporaryYNm }
        expected_old_position = @{ x_nm = $oldXNm; y_nm = $oldYNm }
        dry_run = $false
        ensure_fresh_connectivity = $true
        position_tolerance_nm = 0
    }
    $moveOut = Invoke-MoveTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_move_symbol_args.json") -ArgsObject $moveArgs -OutputLogPath (Join-Path $runDir "schematic_move_symbol_output.log")
    $mutationOkField = Get-BoolField -Text $moveOut -Field "ok" -Default "false"
    $mutationChanged = Get-BoolField -Text $moveOut -Field "changed" -Default "false"
    $connectivityRefreshPerformed = Get-BoolField -Text $moveOut -Field "connectivity_refresh_performed" -Default "false"
    $moveReadback = Get-MovePositionField -Text $moveOut -Field "readback_position"
    $mutationOk = if( $mutationOkField -eq "true" -and $mutationChanged -eq "true" ) { "true" } else { "false" }
    $readbackAfterMoveOk = if( $moveReadback.ok -and $moveReadback.x_nm -eq $temporaryXNm -and $moveReadback.y_nm -eq $temporaryYNm ) { "true" } else { "false" }

    $restoreArgs = @{
        schematic_filename = $sch
        symbol_id = $symbolId
        new_position = @{ x_nm = $oldXNm; y_nm = $oldYNm }
        expected_old_position = @{ x_nm = $temporaryXNm; y_nm = $temporaryYNm }
        dry_run = $false
        ensure_fresh_connectivity = $true
        position_tolerance_nm = 0
    }
    $restoreOut = Invoke-MoveTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_move_symbol_restore_args.json") -ArgsObject $restoreArgs -OutputLogPath (Join-Path $runDir "schematic_move_symbol_restore_output.log")
    $restoreOkField = Get-BoolField -Text $restoreOut -Field "ok" -Default "false"
    $restoreChanged = Get-BoolField -Text $restoreOut -Field "changed" -Default "false"
    $restoreReadback = Get-MovePositionField -Text $restoreOut -Field "readback_position"
    $restoreOk = if( $restoreOkField -eq "true" -and $restoreChanged -eq "true" ) { "true" } else { "false" }
    $readbackAfterRestoreOk = if( $restoreReadback.ok -and $restoreReadback.x_nm -eq $oldXNm -and $restoreReadback.y_nm -eq $oldYNm ) { "true" } else { "false" }

    $visible = if( $lt -match 'schematic_move_symbol_session' ) { "true" } else { "false" }
    $callableWithMutation = if( $moveOut -match 'status=1' ) { "true" } else { "false" }
    $toolBlocker = Get-StringField -Text $moveOut -Field "exact_blocker_if_any" -Default "absent"

    $blocker = "N/A"
    if( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callableWithMutation -ne "true" ) { $blocker = "call_failed" }
    elseif( $symbolIdDiscoveryOk -ne "true" ) { $blocker = "symbol_id_discovery_failed" }
    elseif( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $blocker = "dry_run_changed_state" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_not_ok" }
    elseif( $readbackAfterMoveOk -ne "true" ) { $blocker = "move_readback_mismatch" }
    elseif( $restoreOk -ne "true" ) { $blocker = "restore_not_ok" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "restore_readback_mismatch" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "symbol_id_discovery_ok=$symbolIdDiscoveryOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "readback_after_move_ok=$readbackAfterMoveOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "symbol_id=$symbolId"
        "old_position_x_nm=$oldXNm"
        "old_position_y_nm=$oldYNm"
        "temporary_position_x_nm=$temporaryXNm"
        "temporary_position_y_nm=$temporaryYNm"
        "restored_position_x_nm=$($restoreReadback.x_nm)"
        "restored_position_y_nm=$($restoreReadback.y_nm)"
        "connectivity_refresh_performed=$connectivityRefreshPerformed"
        "fixture_source=$fixtureSource"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_move_symbol_session_contract_summary.txt") -Encoding UTF8

    Copy-MoveDiagLog -DestinationDir $runDir
    Write-TargetRunDir -RunDir $runDir
}
catch
{
    Copy-MoveDiagLog -DestinationDir $runDir
    Write-TargetRunDir -RunDir $runDir
    throw
}
finally
{
    if( $null -eq $previousDiagEnv )
    {
        Remove-Item Env:KICAD_IPC_STARTUP_DIAG -ErrorAction SilentlyContinue
    }
    else
    {
        $env:KICAD_IPC_STARTUP_DIAG = $previousDiagEnv
    }
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
