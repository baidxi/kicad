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
$sch      = $schBuild
$fixtureSource = "build_v1"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $schBuild)) { throw "Missing schematic at $schBuild" }
if( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-create-wire-segment-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

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

function Invoke-WireCreateTool {
    param(
        [string]$Url,
        [string]$ArgsFilePath,
        [hashtable]$ArgsObject,
        [string]$OutputLogPath
    )
    ($ArgsObject | ConvertTo-Json -Compress) | Set-Content -Path $ArgsFilePath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "schematic_create_wire_segment_session", ("@" + $ArgsFilePath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path $OutputLogPath -Encoding UTF8
    return $out
}

function Invoke-GetById {
    param(
        [string]$Url,
        [string]$Id,
        [string]$CaseName
    )
    $argsObj = @{
        schematic_filename = $sch
        ids = @($Id)
    }
    $argsPath = Join-Path $runDir ($CaseName + "_args.json")
    ($argsObj | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "get_sch_items_by_id", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path (Join-Path $runDir ($CaseName + "_output.log")) -Encoding UTF8
    return $out
}

function Invoke-DeleteById {
    param(
        [string]$Url,
        [string]$Id
    )
    $argsObj = @{
        schematic_filename = $sch
        ids = @($Id)
    }
    $argsPath = Join-Path $runDir "delete_sch_items_wire_restore_args.json"
    ($argsObj | ConvertTo-Json -Compress) | Set-Content -Path $argsPath -Encoding UTF8
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($Url, "mcp_call_tool_args", "delete_sch_items", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $out = $res.Text
    $out | Set-Content -Path (Join-Path $runDir "delete_sch_items_wire_restore_output.log") -Encoding UTF8
    return $out
}

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets
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

    $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
    $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "qa_open_editor_frame_input.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot
    $openOut = $openRes.Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8
    if( $openOut -notmatch 'status=1' ) { throw "qa_open_editor_frame did not return AS_OK" }

    $startXNm = 5000000
    $startYNm = 5000000
    $endXNm = 7000000
    $endYNm = 5000000

    $dryRunArgs = @{
        schematic_filename = $sch
        start = @{ x_nm = $startXNm; y_nm = $startYNm }
        end = @{ x_nm = $endXNm; y_nm = $endYNm }
        dry_run = $true
        ensure_fresh_connectivity = $true
        require_orthogonal = $true
    }
    $dryRunOut = Invoke-WireCreateTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_create_wire_segment_dry_run_args.json") -ArgsObject $dryRunArgs -OutputLogPath (Join-Path $runDir "schematic_create_wire_segment_dry_run_output.log")

    $dryRunOk = Get-BoolField -Text $dryRunOut -Field "ok" -Default "false"
    $dryRunChanged = Get-BoolField -Text $dryRunOut -Field "changed" -Default "false"
    $dryRunConnectivity = Get-BoolField -Text $dryRunOut -Field "connectivity_refresh_performed" -Default "true"
    $dryRunWireId = Get-StringField -Text $dryRunOut -Field "wire_id" -Default ""
    $dryRunNoChangeOk = if( $dryRunChanged -eq "true" -and $dryRunConnectivity -eq "false" -and ($dryRunWireId -eq "dry_run" -or $dryRunWireId -eq "") ) { "true" } else { "false" }

    $createArgs = @{
        schematic_filename = $sch
        start = @{ x_nm = $startXNm; y_nm = $startYNm }
        end = @{ x_nm = $endXNm; y_nm = $endYNm }
        dry_run = $false
        ensure_fresh_connectivity = $true
        require_orthogonal = $true
    }
    $createOut = Invoke-WireCreateTool -Url $url -ArgsFilePath (Join-Path $runDir "schematic_create_wire_segment_args.json") -ArgsObject $createArgs -OutputLogPath (Join-Path $runDir "schematic_create_wire_segment_output.log")
    $mutationOkField = Get-BoolField -Text $createOut -Field "ok" -Default "false"
    $mutationChanged = Get-BoolField -Text $createOut -Field "changed" -Default "false"
    $wireId = Get-StringField -Text $createOut -Field "wire_id" -Default ""
    $connectivityRefreshPerformed = Get-BoolField -Text $createOut -Field "connectivity_refresh_performed" -Default "false"
    $toolBlocker = Get-StringField -Text $createOut -Field "exact_blocker_if_any" -Default "absent"
    $outStartXNm = Get-Int64Field -Text $createOut -Field "start_x_nm" -Default 0
    $outStartYNm = Get-Int64Field -Text $createOut -Field "start_y_nm" -Default 0
    $outEndXNm = Get-Int64Field -Text $createOut -Field "end_x_nm" -Default 0
    $outEndYNm = Get-Int64Field -Text $createOut -Field "end_y_nm" -Default 0

    $mutationOk = if( $mutationOkField -eq "true" -and $mutationChanged -eq "true" ) { "true" } else { "false" }
    $wireCreatedOk = if( ![string]::IsNullOrWhiteSpace($wireId) -and $wireId -ne "dry_run" ) { "true" } else { "false" }

    $readbackAfterCreateOk = "false"
    $readbackAfterCreateOut = ""
    if( $wireCreatedOk -eq "true" )
    {
        $readbackAfterCreateOut = Invoke-GetById -Url $url -Id $wireId -CaseName "get_sch_items_by_id_after_create"
        if( $readbackAfterCreateOut -match [regex]::Escape($wireId) )
        {
            $readbackAfterCreateOk = "true"
        }
    }

    $restoreOk = "false"
    $deleteOut = ""
    if( $wireCreatedOk -eq "true" )
    {
        $deleteOut = Invoke-DeleteById -Url $url -Id $wireId
        if( $deleteOut -match 'status=1' ) { $restoreOk = "true" }
    }

    $readbackAfterRestoreOk = "false"
    if( $wireCreatedOk -eq "true" )
    {
        $readbackAfterRestoreOut = Invoke-GetById -Url $url -Id $wireId -CaseName "get_sch_items_by_id_after_restore"
        if( $readbackAfterRestoreOut -notmatch [regex]::Escape($wireId) )
        {
            $readbackAfterRestoreOk = "true"
        }
    }

    $visible = if( $lt -match 'schematic_create_wire_segment_session' ) { "true" } else { "false" }
    $callableWithMutation = if( $createOut -match 'status=1' ) { "true" } else { "false" }

    $blocker = "N/A"
    if( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callableWithMutation -ne "true" ) { $blocker = "call_failed" }
    elseif( $dryRunOk -ne "true" ) { $blocker = "dry_run_not_ok" }
    elseif( $dryRunNoChangeOk -ne "true" ) { $blocker = "dry_run_contract_failed" }
    elseif( $mutationOk -ne "true" ) { $blocker = "mutation_not_ok" }
    elseif( $wireCreatedOk -ne "true" ) { $blocker = "wire_id_missing" }
    elseif( $readbackAfterCreateOk -ne "true" ) { $blocker = "readback_after_create_failed" }
    elseif( $restoreOk -ne "true" ) { $blocker = "restore_delete_failed" }
    elseif( $readbackAfterRestoreOk -ne "true" ) { $blocker = "readback_after_restore_failed" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_with_mutation=$callableWithMutation"
        "dry_run_ok=$dryRunOk"
        "dry_run_no_change_ok=$dryRunNoChangeOk"
        "mutation_ok=$mutationOk"
        "wire_created_ok=$wireCreatedOk"
        "readback_after_create_ok=$readbackAfterCreateOk"
        "restore_ok=$restoreOk"
        "readback_after_restore_ok=$readbackAfterRestoreOk"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "wire_id=$wireId"
        "start_x_nm=$outStartXNm"
        "start_y_nm=$outStartYNm"
        "end_x_nm=$outEndXNm"
        "end_y_nm=$outEndYNm"
        "connectivity_refresh_performed=$connectivityRefreshPerformed"
        "fixture_source=$fixtureSource"
        "fixture_schematic=$sch"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_create_wire_segment_session_contract_summary.txt") -Encoding UTF8

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
