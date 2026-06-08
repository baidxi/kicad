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
$schWithKnownLocalLabel = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }

$fixtureSource = "build_ipc_v1_schematic"
if( Test-Path -LiteralPath $schWithKnownLocalLabel )
{
    $sch = $schWithKnownLocalLabel
    $fixtureSource = "qa_data_issue19090_with_local_label"
}

function Ensure-LocalLabelFixture {
    param([string]$SchematicPath)

    $raw = Get-Content -LiteralPath $SchematicPath -Raw
    $qaLabelId = "44444444-4444-4444-4444-444444444444"
    $canonicalLabel = @"
	(label "MCP_LABEL_ORIG"
		(at 5 0 0)
		(fields_autoplaced yes)
		(effects
			(font (size 1.27 1.27))
			(justify left bottom)
		)
		(uuid "$qaLabelId")
	)
"@

    $qaBlockPattern = '(?s)\(label\s+"MCP_LABEL_ORIG".*?\n\t\)'
    if( $raw -match $qaBlockPattern )
    {
        $raw = [regex]::Replace( $raw, $qaBlockPattern, ($canonicalLabel.TrimEnd()), 1 )
        Set-Content -LiteralPath $SchematicPath -Value $raw -Encoding UTF8
        return [pscustomobject]@{
            Injected = $false
            LabelId = $qaLabelId
        }
    }

    $qaLabelPattern = '(?s)\(label\s+"MCP_LABEL_ORIG".*?\(uuid\s+([0-9a-fA-F\-]{36})\)'
    if( $raw -match $qaLabelPattern )
    {
        return [pscustomobject]@{
            Injected = $false
            LabelId = $Matches[1]
        }
    }

    if( $raw -match '(?m)^\s*\(label\s+"' )
    {
        $anyLabelPattern = '(?s)\(label\s+"[^"]*".*?\(uuid\s+([0-9a-fA-F\-]{36})\)'
        $anyLabelId = ""
        if( $raw -match $anyLabelPattern )
        {
            $anyLabelId = $Matches[1]
        }

        return [pscustomobject]@{
            Injected = $false
            LabelId = $anyLabelId
        }
    }

    $injection = $canonicalLabel

    $anchor = '(?m)^\s*\(sheet_instances'
    if( $raw -match $anchor )
    {
        $raw = [regex]::Replace( $raw, $anchor, ($injection + "`r`n" + '$0'), 1 )
    }
    else
    {
        $raw = $raw.TrimEnd() + "`r`n" + $injection + "`r`n"
    }

    Set-Content -LiteralPath $SchematicPath -Value $raw -Encoding UTF8
    return [pscustomobject]@{
        Injected = $true
        LabelId = $qaLabelId
    }
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-set-label-text-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation
$fixtureStatus = [pscustomobject]@{
    Injected = $false
    LabelId = ""
}

if( $fixtureSource -eq "build_ipc_v1_schematic" )
{
    $fixtureStatus = Ensure-LocalLabelFixture -SchematicPath $sch
}

("fixture_source=" + $fixtureSource +
 ";fixture_schematic=" + $sch +
 ";fixture_local_label_injected=" + $(if($fixtureStatus.Injected){"true"}else{"false"}) +
 ";fixture_injected_label_id=" + $fixtureStatus.LabelId) | Set-Content -Path (Join-Path $runDir "fixture_local_label_status.txt") -Encoding UTF8

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

function Parse-SetLabelResult {
    param([string]$OutputText)

    $statusOk = ($OutputText -match 'status=1')
    $ok = "false"
    if( $OutputText -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }
    $changed = "false"
    if( $OutputText -match 'key:\s+"changed"[\s\S]*?bool_value:\s*(true|false)' ) { $changed = $Matches[1] }
    $blocker = "absent"
    if( $OutputText -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $blocker = $Matches[1] }
    $oldText = ""
    if( $OutputText -match 'key:\s+"old_text"[\s\S]*?string_value:\s*"([^"]*)"' ) { $oldText = $Matches[1] }
    $newText = ""
    if( $OutputText -match 'key:\s+"new_text"[\s\S]*?string_value:\s*"([^"]*)"' ) { $newText = $Matches[1] }
    $labelType = ""
    if( $OutputText -match 'key:\s+"label_type"[\s\S]*?string_value:\s*"([^"]*)"' ) { $labelType = $Matches[1] }
    $refreshPerformed = "false"
    if( $OutputText -match 'key:\s+"connectivity_refresh_performed"[\s\S]*?bool_value:\s*(true|false)' ) { $refreshPerformed = $Matches[1] }

    return @{
        status_ok = $statusOk
        ok = $ok
        changed = $changed
        blocker = $blocker
        old_text = $oldText
        new_text = $newText
        label_type = $labelType
        connectivity_refresh_performed = $refreshPerformed
    }
}

function Invoke-SetLabelSession {
    param(
        [string]$Url,
        [hashtable]$ToolArgs,
        [string]$ArgsPath,
        [string]$LogPath
    )

    ($ToolArgs | ConvertTo-Json -Compress) | Set-Content -Path $ArgsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_set_label_text_session", ("@" + $ArgsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $LogPath -Encoding UTF8
    $parsed = Parse-SetLabelResult -OutputText $out
    return @{
        raw = $out
        parsed = $parsed
    }
}

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
                                          -MaxIterations 120 -PingTimeoutMs 1500 `
                                          -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if( $resolved.Resolved ) { $resolved.Url } else { $null }

    if( Test-Path -LiteralPath $sockDir )
    {
        $pidSock = Join-Path $sockDir ("api-" + $kicadProc.Id + ".sock")
        $defaultSock = Join-Path $sockDir "api.sock"
        ("resolved=" + $(if($resolved.Resolved){"true"}else{"false"}) +
         ";url=" + $url +
         ";pid_sock=" + $pidSock +
         ";default_sock=" + $defaultSock) | Set-Content -Path (Join-Path $runDir "ipc_url_resolve.txt") -Encoding UTF8
    }

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

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 20000 -WorkingDirectory $RepoRoot
    $openRes.Text | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8

    $debugItemsArgs = @{ schematic_filename = $sch } | ConvertTo-Json -Compress
    $debugItemsInput = Join-Path $runDir "get_sch_items_all_for_label_debug_args.json"
    $debugItemsArgs | Set-Content -Path $debugItemsInput -Encoding UTF8
    try
    {
        $debugItemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $debugItemsInput)) -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $debugItemsRes.Text | Set-Content -Path (Join-Path $runDir "get_sch_items_all_for_label_debug.log") -Encoding UTF8
    }
    catch
    {
        ("debug_get_sch_items_failed=" + $_.Exception.Message) | Set-Content -Path (Join-Path $runDir "get_sch_items_all_for_label_debug.log") -Encoding UTF8
    }

    $itemsArgs = @{ schematic_filename = $sch; types = @("KOT_SCH_LABEL") } | ConvertTo-Json -Compress
    $itemsInput = Join-Path $runDir "get_sch_items_labels_args.json"
    $itemsArgs | Set-Content -Path $itemsInput -Encoding UTF8
    $itemsOut = ""
    try
    {
        $itemsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $itemsInput)) -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $itemsOut = $itemsRes.Text
        $itemsOut | Set-Content -Path (Join-Path $runDir "get_sch_items_for_label_id_output.log") -Encoding UTF8
    }
    catch
    {
        ("label_discovery_get_sch_items_failed=" + $_.Exception.Message) | Set-Content -Path (Join-Path $runDir "get_sch_items_for_label_id_output.log") -Encoding UTF8
    }

    $labelId = ""
    if( -not [string]::IsNullOrWhiteSpace( $itemsOut ) )
    {
        $typedIdCandidates = [regex]::Matches(
            $itemsOut,
            'KOT_SCH_LABEL[\s\S]{0,2000}?key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
        $labelId = Take-FirstValid $typedIdCandidates

        if( [string]::IsNullOrWhiteSpace( $labelId ) )
        {
            $typedUuidCandidates = [regex]::Matches(
                $itemsOut,
                'KOT_SCH_LABEL[\s\S]{0,2000}?key:\s+"uuid"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
            $labelId = Take-FirstValid $typedUuidCandidates
        }

        if( [string]::IsNullOrWhiteSpace( $labelId ) )
        {
            $idValueCandidates = [regex]::Matches(
                $itemsOut,
                'key:\s+"id"[\s\S]{0,600}?key:\s+"value"[\s\S]{0,600}?string_value:\s*"([^"]+)"' )
            $labelId = Take-FirstValid $idValueCandidates
        }
    }

    if( [string]::IsNullOrWhiteSpace( $labelId ) -and -not [string]::IsNullOrWhiteSpace( $fixtureStatus.LabelId ) )
    {
        $labelId = $fixtureStatus.LabelId
    }

    if( [string]::IsNullOrWhiteSpace( $labelId ) )
    {
        throw "label_id discovery failed from get_sch_items output and no injected local label fallback available"
    }

    $probeOldArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = ""
        dry_run = $true
    }
    $probeOldCall = Invoke-SetLabelSession -Url $url -ToolArgs $probeOldArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_probe_old_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_probe_old_output.log")

    if( $probeOldCall.parsed.ok -ne "true" -or $probeOldCall.parsed.blocker -ne "N/A" )
    {
        throw "unable to obtain old_text using probe-old dry run"
    }

    $oldText = $probeOldCall.parsed.old_text
    $temporaryText = "MCP_LABEL_TMP_" + (Get-Date -Format "yyyyMMddHHmmssfff")

    $dryRunArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $temporaryText
        expected_old_text = $oldText
        dry_run = $true
        ensure_fresh_connectivity = $true
    }
    $dryRunCall = Invoke-SetLabelSession -Url $url -ToolArgs $dryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_dry_run_output.log")

    $verifyAfterDryRunArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $temporaryText
        expected_old_text = $oldText
        dry_run = $true
        ensure_fresh_connectivity = $true
    }
    $verifyAfterDryRunCall = Invoke-SetLabelSession -Url $url -ToolArgs $verifyAfterDryRunArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_verify_after_dry_run_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_verify_after_dry_run_output.log")

    $setArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $temporaryText
        expected_old_text = $oldText
        dry_run = $false
        ensure_fresh_connectivity = $true
    }
    $setCall = Invoke-SetLabelSession -Url $url -ToolArgs $setArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_set_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_output.log")

    $verifyAfterSetArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $temporaryText
        expected_old_text = $temporaryText
        dry_run = $true
        ensure_fresh_connectivity = $true
    }
    $verifyAfterSetCall = Invoke-SetLabelSession -Url $url -ToolArgs $verifyAfterSetArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_verify_after_set_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_verify_after_set_output.log")

    $restoreArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $oldText
        expected_old_text = $temporaryText
        dry_run = $false
        ensure_fresh_connectivity = $true
    }
    $restoreCall = Invoke-SetLabelSession -Url $url -ToolArgs $restoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_restore_output.log")

    $verifyAfterRestoreArgs = @{
        schematic_filename = $sch
        label_id = $labelId
        new_text = $oldText
        expected_old_text = $oldText
        dry_run = $true
        ensure_fresh_connectivity = $true
    }
    $verifyAfterRestoreCall = Invoke-SetLabelSession -Url $url -ToolArgs $verifyAfterRestoreArgs `
        -ArgsPath (Join-Path $runDir "schematic_set_label_text_verify_after_restore_args.json") `
        -LogPath (Join-Path $runDir "schematic_set_label_text_verify_after_restore_output.log")

    $visible = if( $lt -match 'schematic_set_label_text_session' ) { "true" } else { "false" }
    $callableWithMutation = if( $setCall.parsed.status_ok ) { "true" } else { "false" }

    $dryRunOk = if( $dryRunCall.parsed.ok -eq "true" ) { "true" } else { "false" }
    $dryRunNoChangeOk = if( $verifyAfterDryRunCall.parsed.ok -eq "true" -and $verifyAfterDryRunCall.parsed.blocker -eq "N/A" ) { "true" } else { "false" }
    $mutationOk = if( $setCall.parsed.ok -eq "true" -and $setCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterSetOk = if( $verifyAfterSetCall.parsed.ok -eq "true" -and $verifyAfterSetCall.parsed.blocker -eq "N/A" ) { "true" } else { "false" }
    $restoreOk = if( $restoreCall.parsed.ok -eq "true" -and $restoreCall.parsed.changed -eq "true" ) { "true" } else { "false" }
    $readbackAfterRestoreOk = if( $verifyAfterRestoreCall.parsed.ok -eq "true" -and $verifyAfterRestoreCall.parsed.blocker -eq "N/A" ) { "true" } else { "false" }

    $toolBlocker = $setCall.parsed.blocker
    if( [string]::IsNullOrWhiteSpace( $toolBlocker ) ) { $toolBlocker = "absent" }

    $blocker = "N/A"
    if( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callableWithMutation -ne "true" ) { $blocker = "call_failed" }
    elseif( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $blocker = "dry_run_modified_or_changed" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_not_ok" }
    elseif( $readbackAfterSetOk -ne "true" ) { $blocker = "set_readback_mismatch" }
    elseif( $restoreOk -ne "true" ) { $blocker = "restore_not_ok" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "restore_readback_mismatch" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "readback_after_set_ok=$readbackAfterSetOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "label_id=$labelId"
        "old_text=$oldText"
        "temporary_text=$temporaryText"
        "restored_text=$oldText"
        "connectivity_refresh_performed=" + $setCall.parsed.connectivity_refresh_performed
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_set_label_text_session_contract_summary.txt") -Encoding UTF8

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
