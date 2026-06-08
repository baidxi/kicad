param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj     = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro"
$sch      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$spicePreviewSch = Join-Path $RepoRoot "qa\data\eeschema\spice_netlists\directives\directives.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$apiLog   = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\api.log"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }
if (!(Test-Path $spicePreviewSch)) { throw "Missing simulation schematic at $spicePreviewSch" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-sch-restore-after-sim-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$pythonHomeDir = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"
$pythonExe = Join-Path $pythonHomeDir "python.exe"
$apiConfig = @{ enable_server = $true }
if( Test-Path $pythonExe )
{
    $apiConfig.interpreter_path = $pythonExe
}

$common = @{
    meta = @{ version = 5 }
    api  = $apiConfig
    system = @{
        local_history_enabled = $false
        working_dir           = $RepoRoot
    }
    auto_backup = @{
        enabled          = $false
        limit_total_size = 104857600
    }
    do_not_show_again = @{
        data_collection_prompt      = $true
        update_check_prompt         = $true
        env_var_overwrite_warning   = $true
        scaled_3d_models_warning    = $true
        zone_fill_warning           = $true
    }
} | ConvertTo-Json -Depth 10

$kicadCfg = @{
    meta   = @{ version = 0 }
    system = @{
        first_run_shown = $true
        open_projects   = @()
    }
} | ConvertTo-Json -Depth 6

$common | Set-Content -Path (Join-Path $cfg10 "kicad_common.json") -Encoding UTF8
$kicadCfg | Set-Content -Path (Join-Path $cfg10 "kicad.json") -Encoding UTF8
"EnableAPILogging=1" | Set-Content -Path (Join-Path $cfg10 "kicad_advanced") -Encoding ASCII

$env:KICAD_CONFIG_HOME = $cfgRoot
$env:KICAD_RUN_FROM_BUILD_DIR = "1"
$env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
$env:PYTHONHOME = $pythonHomeDir
$pathPrefix = @(
    (Join-Path $BuildDir "kicad"),
    (Join-Path $BuildDir "eeschema"),
    (Join-Path $BuildDir "pcbnew"),
    (Join-Path $BuildDir "common"),
    (Join-Path $BuildDir "common\gal"),
    (Join-Path $BuildDir "api"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3")
) -join ";"
$env:PATH = $pathPrefix + ";" + $env:PATH

function Call-McpToolArgsAndLog([string]$runDir, [string]$toolName, [string]$caseLabel, [hashtable]$argsObj,
                                [string]$url, [string]$smokeExe) {
    $argsJson = $argsObj | ConvertTo-Json -Compress -Depth 30
    $inputPath = Join-Path $runDir ("mcp_call_tool_args_" + $toolName + "_" + $caseLabel + "_input.json")
    $argsJson | Set-Content -Path $inputPath -Encoding UTF8
    $logPath = Join-Path $runDir ("mcp_call_tool_args_" + $toolName + "_" + $caseLabel + "_output.log")
    $errPath = Join-Path $runDir ("mcp_call_tool_args_" + $toolName + "_" + $caseLabel + "_stderr.log")
    $timeoutMs = 15000

    $proc = Start-Process -FilePath $smokeExe `
            -ArgumentList @($url, "mcp_call_tool_args", $toolName, ("@" + $inputPath)) `
            -NoNewWindow -PassThru `
            -RedirectStandardOutput $logPath `
            -RedirectStandardError $errPath

    $exited = $proc.WaitForExit($timeoutMs)
    if( -not $exited )
    {
        Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
        ("timeout_ms=" + $timeoutMs + "`n" +
         "exact_blocker_if_any=call_timeout`n") | Add-Content -Path $logPath -Encoding UTF8
    }

    $out = ""
    if( Test-Path $logPath )
    {
        $out = Get-Content -Path $logPath -Raw
    }
    if( Test-Path $errPath )
    {
        $err = Get-Content -Path $errPath -Raw
        if( -not [string]::IsNullOrWhiteSpace( $err ) )
        {
            $out += ("`n" + $err)
        }
    }

    return $out
}

Stop-Process -Name kicad -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

try
{
    $url = $null
    for( $i = 0; $i -lt 120; $i++ )
    {
        if( Test-Path $apiLog )
        {
            $lines = Get-Content $apiLog -Tail 400
            for( $j = $lines.Count - 1; $j -ge 0; $j-- )
            {
                if( $lines[$j] -match 'started at (ipc://.+) ---' )
                {
                    $url = $Matches[1]
                    break
                }
            }
            if( $url ) { break }
        }
        Start-Sleep -Milliseconds 500
    }
    if( -not $url ) { throw "Unable to resolve API URL from api.log" }
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    for( $i = 0; $i -lt 30; $i++ )
    {
        $pingOut = ""
        try
        {
            $pingOut = & $smokeExe $url ping 2>&1 | Out-String
        }
        catch
        {
            $pingOut = $_ | Out-String
        }
        $pingOut | Set-Content -Path (Join-Path $runDir ("ping_" + $i + ".log")) -Encoding UTF8
        if( $pingOut -match 'status=1' ) { break }
        Start-Sleep -Milliseconds 250
    }

    # Reproduce sequence: open simulation SCH -> run sim -> restore stub SCH.
    $simOpenArgs = @{ type = "sch"; filename = $spicePreviewSch }
    Call-McpToolArgsAndLog -runDir $runDir -toolName "qa_open_editor_frame" -caseLabel "open_sim_sch" `
            -argsObj $simOpenArgs -url $url -smokeExe $smokeExe | Out-Null

    $simXArgs = @{ timeout_ms = 4000 }
    $outSimX = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_xaxis_probe_session" `
            -caseLabel "simulation_xaxis_before_restore" -argsObj $simXArgs -url $url -smokeExe $smokeExe
    $sim_call_ok = if( $outSimX -match 'status=1' ) { "true" } else { "false" }

    $probeArgs = @{ schematic_filename = $sch }
    $probeBefore = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
            -caseLabel "restore_probe_before" -argsObj $probeArgs -url $url -smokeExe $smokeExe
    $restore_needed = if( $probeBefore -match 'status=1' ) { "false" } else { "true" }

    $restore_attempted = "false"
    $restore_call_ok = "not_attempted"
    $restore_attempts = 0
    if( $restore_needed -eq "true" )
    {
        for( $ri = 0; $ri -lt 20; $ri++ )
        {
            $pingBeforeRestore = ""
            try
            {
                $pingBeforeRestore = & $smokeExe $url ping 2>&1 | Out-String
            }
            catch
            {
                $pingBeforeRestore = $_ | Out-String
            }
            $pingBeforeRestore | Set-Content -Path (Join-Path $runDir ("ping_before_restore_" + $ri + ".log")) -Encoding UTF8
            if( $pingBeforeRestore -match 'status=1' ) { break }
            Start-Sleep -Milliseconds 250
        }

        $restore_attempted = "true"
        $restore_call_ok = "false"

        $restoreArgs = @{ type = "sch"; filename = $sch }
        $restoreInputPath = Join-Path $runDir "mcp_call_tool_args_qa_open_editor_frame_restore_after_sim_input.json"
        ($restoreArgs | ConvertTo-Json -Compress) | Set-Content -Path $restoreInputPath -Encoding UTF8
        $restoreTimeoutMs = 15000

        for( $ra = 1; $ra -le 3; $ra++ )
        {
            $restore_attempts = $ra
            $restoreOutPath = Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_restore_after_sim_attempt_" + $ra + "_output.log")
            $restoreErrPath = Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_restore_after_sim_attempt_" + $ra + "_stderr.log")

            $restoreProc = Start-Process -FilePath $smokeExe `
                    -ArgumentList @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $restoreInputPath)) `
                    -NoNewWindow -PassThru `
                    -RedirectStandardOutput $restoreOutPath `
                    -RedirectStandardError $restoreErrPath

            $restoreExited = $restoreProc.WaitForExit($restoreTimeoutMs)
            if( -not $restoreExited )
            {
                Stop-Process -Id $restoreProc.Id -Force -ErrorAction SilentlyContinue
                ("restore_call_timeout_ms=" + $restoreTimeoutMs + "`n" +
                 "attempt=" + $ra + "`n") | Add-Content -Path $restoreOutPath -Encoding UTF8
            }

            $restoreRaw = ""
            if( Test-Path $restoreOutPath )
            {
                $restoreRaw = Get-Content -Path $restoreOutPath -Raw
            }
            if( Test-Path $restoreErrPath )
            {
                $restoreErrRaw = Get-Content -Path $restoreErrPath -Raw
                if( -not [string]::IsNullOrWhiteSpace( $restoreErrRaw ) )
                {
                    $restoreRaw += ("`n" + $restoreErrRaw)
                }
            }

            if( $restoreRaw -match 'status=1' )
            {
                $restore_call_ok = "true"
                break
            }

            Start-Sleep -Milliseconds 350
        }
    }

    $probeAfter = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
            -caseLabel "restore_probe_after" -argsObj $probeArgs -url $url -smokeExe $smokeExe
    $restore_anchor_ok = if( $probeAfter -match 'status=1' ) { "true" } else { "false" }

    $exact_blocker = "N/A"
    if( $sim_call_ok -ne "true" ) { $exact_blocker = "simulation_xaxis_call_failed" }
    elseif( $restore_needed -eq "true" -and $restore_attempted -ne "true" ) { $exact_blocker = "restore_needed_but_not_attempted" }
    elseif( $restore_attempted -eq "true" -and $restore_call_ok -ne "true" ) { $exact_blocker = "restore_call_failed" }
    elseif( $restore_anchor_ok -ne "true" ) { $exact_blocker = "restore_anchor_probe_failed" }

    @(
        "sim_call_ok=$sim_call_ok"
        "restore_needed=$restore_needed"
        "restore_attempted=$restore_attempted"
        "restore_call_ok=$restore_call_ok"
        "restore_attempts=$restore_attempts"
        "restore_anchor_ok=$restore_anchor_ok"
        "exact_blocker_if_any=$exact_blocker"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_sch_restore_after_sim_contract_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
finally
{
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
