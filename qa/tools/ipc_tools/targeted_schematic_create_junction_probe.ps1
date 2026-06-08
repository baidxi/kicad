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
$sch      = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic fixture at $sch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-create-junction-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

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

function Get-Int64Field {
    param([string]$Text,[string]$Field,[int64]$Default=0)
    if( $Text -match ('key:\s+"' + [regex]::Escape($Field) + '"[\s\S]*?number_value:\s*([-]?[0-9]+(?:\.[0-9]+)?)') ) { return [int64][double]$Matches[1] }
    return $Default
}

function Invoke-Tool {
    param(
        [string]$Url,
        [string]$ToolName,
        [hashtable]$ArgsObject,
        [string]$CaseName,
        [int]$TimeoutMs = 90000
    )
    $argsPath = Join-Path $runDir ($CaseName + "_args.json")
    ($ArgsObject | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $outPath = Join-Path $runDir ($CaseName + "_output.log")
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", $ToolName, ("@" + $argsPath)) -TimeoutMs $TimeoutMs -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $outPath -Encoding UTF8
    return $out
}

function Invoke-OpenEditorWithRetry {
    param([string]$Url)
    for( $i = 1; $i -le 3; $i++ )
    {
        $out = Invoke-Tool -Url $Url -ToolName "qa_open_editor_frame" -ArgsObject @{ type="sch"; filename=$sch } -CaseName ("qa_open_editor_frame_attempt" + $i) -TimeoutMs 120000
        if( $out -match 'status=1' ) { return $true }
        Start-Sleep -Milliseconds (500 * $i)
    }
    return $false
}

function Restart-KiCadSession {
    Stop-KiCadProcesses
    Clear-IpcSockets | Out-Null
    Start-Sleep -Milliseconds 250
    $proc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
    Start-Sleep -Milliseconds 1500
    $resolve = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $proc.Id -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 250 -WorkingDirectory $RepoRoot
    if( -not $resolve.Resolved ) { return @{ ok = $false; url = "" } }
    return @{ ok = $true; url = $resolve.Url }
}

$visible = "false"
$callableWithMutation = "false"
$fixtureSetupOk = "false"
$dryRunOk = "false"
$dryRunNoChangeOk = "false"
$mutationOk = "false"
$junctionCreatedOk = "false"
$readbackAfterCreateOk = "false"
$restoreOk = "false"
$readbackAfterRestoreOk = "false"
$exactBlocker = "N/A"
$toolBlocker = "N/A"
$junctionId = ""
$junctionNeeded = "false"
$requireNeeded = "true"
$connectivityRefreshPerformed = "false"
$positionXNm = 6000000
$positionYNm = 6000000
$wireAId = ""
$wireBId = ""
$wireCId = ""
$mutationOut = ""
$pingBeforeListToolsOk = "false"
$listToolsAttempts = 0
$listToolsFinalStatus = "failed"
$toolVisibleStatus = "unknown"
$lastBlockerStatus = "N/A"
$toolVisibilityMode = "list_tools"
$directCallAttempted = "false"
$directCallStatus = "not_run"
$junctionNeededDryRun = "false"
$setupRestorePartial = "false"
$dryRunExactBlocker = "N/A"
$wireARestoreOk = "true"
$wireBRestoreOk = "true"
$wireCRestoreOk = "true"
$junctionRestoreOk = "skipped"
$validationStrategy = "explicit_junction_create_no_recalc"
$setupWiresCreated = "false"
$junctionNeededExpected = "false"
$v1RequiresJunctionNeeded = "true"
$negativeIsolatedPointTestOk = "false"
$negativeIsolatedPointBlocker = "junction_not_needed"
$positiveFixtureAvailable = "false"

function Write-V1ScopeStatus {
    @(
        "v1_requires_junction_needed=$v1RequiresJunctionNeeded"
        "negative_isolated_point_test_ok=$negativeIsolatedPointTestOk"
        "negative_isolated_point_blocker=$negativeIsolatedPointBlocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "junction_v1_scope_status.txt") -Encoding UTF8
}

function Write-ValidationStrategyStatus {
    @(
        "validation_strategy=$validationStrategy"
        "require_needed=$requireNeeded"
        "ensure_fresh_connectivity=false"
        "setup_wires_created=$setupWiresCreated"
        "junction_needed_expected=$junctionNeededExpected"
        ""
    ) | Set-Content -Path (Join-Path $runDir "junction_validation_strategy_status.txt") -Encoding UTF8
}

function Write-RestoreStatus {
    @(
        "wire_a_restore_ok=$wireARestoreOk"
        "wire_b_restore_ok=$wireBRestoreOk"
        "wire_c_restore_ok=$wireCRestoreOk"
        "junction_restore_ok=$junctionRestoreOk"
        "setup_restore_partial=$setupRestorePartial"
        ""
    ) | Set-Content -Path (Join-Path $runDir "junction_setup_restore_status.txt") -Encoding UTF8
}

function Write-Summary {
    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "fixture_setup_ok=$fixtureSetupOk"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "junction_created_ok=$junctionCreatedOk"
        "readback_after_create_ok=$readbackAfterCreateOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "exact_blocker_if_any=$exactBlocker"
        "tool_exact_blocker_field=$toolBlocker"
        "junction_id=$junctionId"
        "position_x_nm=$positionXNm"
        "position_y_nm=$positionYNm"
        "junction_needed=$junctionNeeded"
        "require_needed=$requireNeeded"
        "connectivity_refresh_performed=$connectivityRefreshPerformed"
        "fixture_source=issue19090"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_create_junction_session_contract_summary.txt") -Encoding UTF8
}

function Write-ListToolsStatus {
    @(
        "ping_before_list_tools_ok=$pingBeforeListToolsOk"
        "list_tools_attempts=$listToolsAttempts"
        "list_tools_final_status=$listToolsFinalStatus"
        "tool_visible=$toolVisibleStatus"
        "last_blocker=$lastBlockerStatus"
        ""
    ) | Set-Content -Path (Join-Path $runDir "mcp_list_tools_status.txt") -Encoding UTF8
}

function Write-ToolVisibilityStatus {
    @(
        "ping_before_list_tools_ok=$pingBeforeListToolsOk"
        "list_tools_final_status=$listToolsFinalStatus"
        "tool_visibility_mode=$toolVisibilityMode"
        "tool_visible=$toolVisibleStatus"
        "direct_call_attempted=$directCallAttempted"
        "direct_call_status=$directCallStatus"
        ""
    ) | Set-Content -Path (Join-Path $runDir "junction_tool_visibility_status.txt") -Encoding UTF8
}

try {
    Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -EnableAutomation | Out-Null
    Stop-KiCadProcesses
    Clear-IpcSockets | Out-Null

    $session = Restart-KiCadSession
    if( -not $session.ok ) { $exactBlocker = "ipc_url_not_resolved"; return }
    $url = $session.url

    $pingBeforeRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "ping") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
    $pingBeforeText = $pingBeforeRes.Text
    $pingBeforeText | Set-Content -Path (Join-Path $runDir "ping_before_list_tools.log") -Encoding UTF8
    if( $pingBeforeText -match 'status=1' )
    {
        $pingBeforeListToolsOk = "true"
    }
    else
    {
        $pingBeforeListToolsOk = "false"
        $listToolsAttempts = 0
        $listToolsFinalStatus = "failed"
        $toolVisibleStatus = "unknown"
        $lastBlockerStatus = "ipc_ping_failed"
        $exactBlocker = "ipc_ping_failed"
        return
    }

    $toolsText = ""
    $toolsTimedOut = $false
    $toolsSucceeded = $false
    for( $i = 1; $i -le 3; $i++ )
    {
        $listToolsRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_list_tools") -TimeoutMs 20000 -WorkingDirectory $RepoRoot
        $toolsText = $listToolsRes.Text
        $toolsText | Set-Content -Path (Join-Path $runDir ("mcp_list_tools_attempt" + $i + ".log")) -Encoding UTF8
        $listToolsAttempts = $i

        if( $toolsText -match '^(?i)\s*timeout\s*$' )
        {
            $toolsTimedOut = $true
            if( $i -lt 3 ) { Start-Sleep -Milliseconds 750 }
            continue
        }

        $toolsSucceeded = $true
        break
    }

    if( -not $toolsSucceeded )
    {
        if( $toolsTimedOut )
        {
            $listToolsFinalStatus = "timeout"
            $toolVisibleStatus = "unknown"
            $lastBlockerStatus = "mcp_list_tools_timeout"
            $toolVisibilityMode = "direct_call_fallback"
        }
        else
        {
            $listToolsFinalStatus = "failed"
            $toolVisibleStatus = "unknown"
            $lastBlockerStatus = "mcp_list_tools_timeout"
            $exactBlocker = "mcp_list_tools_timeout"
            $directCallStatus = "failed"
            return
        }
    }
    else
    {
        $listToolsFinalStatus = "ok"
        if( $toolsText -match "schematic_create_junction_session" )
        {
            $visible = "true"
            $toolVisibleStatus = "true"
            $lastBlockerStatus = "N/A"
            $toolVisibilityMode = "list_tools"
        }
        else
        {
            $visible = "false"
            $toolVisibleStatus = "false"
            $lastBlockerStatus = "tool_not_listed"
            $toolVisibilityMode = "direct_call_fallback"
        }
    }

    if( -not (Invoke-OpenEditorWithRetry -Url $url) ) { $exactBlocker = "qa_open_editor_frame_failed"; return }

    $fixtureSetupOk = "true"
    $requireNeeded = "false"
    Write-ValidationStrategyStatus
    if( $toolVisibilityMode -eq "direct_call_fallback" ) { $directCallAttempted = "true" }

    $dryRunOut = Invoke-Tool -Url $url -ToolName "schematic_create_junction_session" -CaseName "schematic_create_junction_dry_run" -ArgsObject @{
        schematic_filename = $sch
        position = @{ x_nm = $positionXNm; y_nm = $positionYNm }
        require_needed = $false
        dry_run = $true
        ensure_fresh_connectivity = $false
    }
    $dryRunOkField = Get-BoolField -Text $dryRunOut -Field "ok" -Default "false"
    $dryRunChanged = Get-BoolField -Text $dryRunOut -Field "changed" -Default "false"
    $dryRunJunctionId = Get-StringField -Text $dryRunOut -Field "junction_id" -Default ""
    $dryRunExactBlocker = Get-StringField -Text $dryRunOut -Field "exact_blocker_if_any" -Default "N/A"
    $junctionNeededDryRun = Get-BoolField -Text $dryRunOut -Field "junction_needed" -Default "false"
    $junctionNeeded = $junctionNeededDryRun
    if( $toolVisibilityMode -eq "direct_call_fallback" )
    {
        if( $dryRunOut -match '^(?i)\s*timeout\s*$' )
        {
            $directCallStatus = "timeout"
            $exactBlocker = "direct_call_timeout"
        }
        elseif( $dryRunOut -match '(?i)unknown tool|tool not found|unknown_tool|not registered' )
        {
            $directCallStatus = "tool_not_found"
            $exactBlocker = "tool_not_registered"
        }
        elseif( $dryRunOut -match 'status=1' )
        {
            $directCallStatus = "ok"
            $visible = "unknown_list_tools_timeout_but_direct_call_ok"
            $toolVisibleStatus = "unknown"
            $lastBlockerStatus = "N/A"
        }
        else { $directCallStatus = "failed" }
    }
    $negativeIsolatedPointTestOk = if(
        $dryRunOkField -eq "false" -and
        $dryRunChanged -eq "false" -and
        $dryRunExactBlocker -eq "junction_not_needed" -and
        $junctionNeededDryRun -eq "false" -and
        $dryRunJunctionId -eq ""
    ) { "true" } else { "false" }
    $dryRunOk = $negativeIsolatedPointTestOk
    $dryRunNoChangeOk = $negativeIsolatedPointTestOk
    $negativeIsolatedPointBlocker = $dryRunExactBlocker
    Write-V1ScopeStatus

    if( $positiveFixtureAvailable -eq "true" -and $exactBlocker -eq "N/A" )
    {
        $mutationOut = Invoke-Tool -Url $url -ToolName "schematic_create_junction_session" -CaseName "schematic_create_junction" -ArgsObject @{
            schematic_filename = $sch
            position = @{ x_nm = $positionXNm; y_nm = $positionYNm }
            require_needed = $false
            dry_run = $false
            ensure_fresh_connectivity = $false
        }
        $mutationOut | Set-Content -Path (Join-Path $runDir "schematic_create_junction_output.log") -Encoding UTF8
        $callableWithMutation = if( $mutationOut -match 'status=1' ) { "true" } else { "false" }
        $mutationOkField = Get-BoolField -Text $mutationOut -Field "ok" -Default "false"
        $mutationChanged = Get-BoolField -Text $mutationOut -Field "changed" -Default "false"
        $toolBlocker = Get-StringField -Text $mutationOut -Field "exact_blocker_if_any" -Default "N/A"
        $junctionId = Get-StringField -Text $mutationOut -Field "junction_id" -Default ""
        $junctionNeeded = Get-BoolField -Text $mutationOut -Field "junction_needed" -Default "false"
        $connectivityRefreshPerformed = Get-BoolField -Text $mutationOut -Field "connectivity_refresh_performed" -Default "false"
        $positionXNm = Get-Int64Field -Text $mutationOut -Field "position_x_nm" -Default $positionXNm
        $positionYNm = Get-Int64Field -Text $mutationOut -Field "position_y_nm" -Default $positionYNm
        $mutationOk = if( $mutationOkField -eq "true" -and $mutationChanged -eq "true" -and $toolBlocker -eq "N/A" ) { "true" } else { "false" }
        $junctionCreatedOk = if( $junctionId -ne "" -and $junctionId -ne "dry_run" ) { "true" } else { "false" }

        if( $junctionCreatedOk -eq "true" )
        {
            $readCreateOut = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -CaseName "get_sch_items_by_id_after_create" -ArgsObject @{
                schematic_filename = $sch
                ids = @($junctionId)
            }
            if( $readCreateOut -match '^(?i)\s*timeout\s*$' )
            {
                Start-Sleep -Milliseconds 500
                $readCreateOut = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -CaseName "get_sch_items_by_id_after_create_retry" -ArgsObject @{
                    schematic_filename = $sch
                    ids = @($junctionId)
                }
            }
            if( ($readCreateOut -match [regex]::Escape($junctionId)) -and ($readCreateOut -match '(?i)junction|type') ) { $readbackAfterCreateOk = "true" }
            elseif( $readCreateOut -match [regex]::Escape($junctionId) ) { $readbackAfterCreateOk = "true" }
        }
    }

    $junctionDeleteOk = if( $junctionCreatedOk -eq "true" ) { "false" } else { "true" }
    if( $junctionCreatedOk -eq "true" )
    {
        $delJOut = Invoke-Tool -Url $url -ToolName "delete_sch_items" -CaseName "delete_sch_items_junction_restore" -ArgsObject @{
            schematic_filename = $sch
            ids = @($junctionId)
        }
        if( $delJOut -match 'status=1' ) { $junctionDeleteOk = "true"; $junctionRestoreOk = "true" }
        else { $junctionRestoreOk = "false"; $setupRestorePartial = "true"; $setupRestoreNote = "junction_delete_failed" }
    }

    if( $junctionCreatedOk -eq "false" ) { $junctionRestoreOk = "skipped" }

    if( $junctionCreatedOk -eq "true" -and $junctionDeleteOk -eq "true" ) { $restoreOk = "true" }

    if( $junctionCreatedOk -eq "true" -and $junctionDeleteOk -eq "true" )
    {
        $readRestoreOut = Invoke-Tool -Url $url -ToolName "get_sch_items_by_id" -CaseName "get_sch_items_by_id_after_restore" -ArgsObject @{
            schematic_filename = $sch
            ids = @($junctionId)
        }
        if( $readRestoreOut -notmatch [regex]::Escape($junctionId) -or $readRestoreOut -match '(?i)none of the requested IDs were found or valid' ) { $readbackAfterRestoreOk = "true" }
    }

    if( $exactBlocker -eq "N/A" -and $negativeIsolatedPointTestOk -eq "true" -and $positiveFixtureAvailable -ne "true" )
    {
        $exactBlocker = "junction_positive_fixture_unavailable"
        $toolBlocker = "junction_not_needed"
    }
    elseif( $visible -ne "true" -and $visible -ne "unknown_list_tools_timeout_but_direct_call_ok" ) { $exactBlocker = "tool_not_listed" }
    elseif( $callableWithMutation -ne "true" ) { $exactBlocker = "call_failed" }
    elseif( $fixtureSetupOk -ne "true" ) { $exactBlocker = "fixture_setup_failed" }
    elseif( $dryRunOk -ne "true" ) { $exactBlocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $exactBlocker = "dry_run_contract_failed" }
    elseif( $mutationOk -ne "true" ) { $exactBlocker = "mutation_failed" }
    elseif( $junctionCreatedOk -ne "true" ) { $exactBlocker = "junction_id_missing" }
    elseif( $readbackAfterCreateOk -ne "true" ) { $exactBlocker = "readback_after_create_failed" }
    elseif( $restoreOk -ne "true" ) { $exactBlocker = "restore_failed" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $exactBlocker = "readback_after_restore_failed" }
    else { $exactBlocker = "N/A" }
}
catch {
    if( $exactBlocker -eq "N/A" ) { $exactBlocker = "exception" }
    $_ | Out-String | Set-Content -Path (Join-Path $runDir "probe_exception.log") -Encoding UTF8
}
finally {
    if( $exactBlocker -eq "N/A" )
    {
        $lastBlockerStatus = "N/A"
    }
    elseif( $lastBlockerStatus -eq "N/A" -or $lastBlockerStatus -eq "" )
    {
        $lastBlockerStatus = $exactBlocker
    }
    Write-ListToolsStatus
    Write-ToolVisibilityStatus
    Write-ValidationStrategyStatus
    Write-V1ScopeStatus
    Write-RestoreStatus
    Write-Summary
    Write-TargetRunDir -RunDir $runDir
}
