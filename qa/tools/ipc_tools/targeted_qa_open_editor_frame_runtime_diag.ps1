param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
# Match qa/tools/ipc_tools/run_open_frame_ipc_smoke.ps1 / run_ipc_v1_windows.ps1 so KiCad finds
# kifaces, Python, and plugins when KICAD_RUN_FROM_BUILD_DIR=1 (avoids late DLL failures / API issues).
if ($env:OS -eq "Windows_NT") {
    $pyHome = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"
    if (Test-Path $pyHome) {
        $env:PYTHONHOME = $pyHome
        $env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
    }
    $pathPrefixDirs = @(
        (Join-Path $BuildDir "kicad"),
        (Join-Path $BuildDir "eeschema"),
        (Join-Path $BuildDir "pcbnew"),
        (Join-Path $BuildDir "common"),
        (Join-Path $BuildDir "common\gal"),
        (Join-Path $BuildDir "api"),
        (Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"),
        $pyHome
    ) | Where-Object { Test-Path $_ }
    if ($pathPrefixDirs.Count -gt 0) {
        $env:PATH = ($pathPrefixDirs -join [System.IO.Path]::PathSeparator) +
            [System.IO.Path]::PathSeparator + $env:PATH
    }
}
else {
    $vcpkgBin = Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"
    if (Test-Path $vcpkgBin) {
        $env:PATH = $vcpkgBin + [System.IO.Path]::PathSeparator + $env:PATH
    }
    $kicadBin = Join-Path $BuildDir "kicad"
    if (Test-Path $kicadBin) {
        $env:PATH = $kicadBin + [System.IO.Path]::PathSeparator + $env:PATH
    }
}
$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro"
$schRestore = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$schSim = Join-Path $RepoRoot "qa\data\eeschema\spice_netlists\directives\directives.kicad_sch"
$apiLog = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\api.log"
$diagLog = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\qa_open_editor_frame_diag.log"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe: $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe: $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project: $proj" }
if (!(Test-Path $schRestore)) { throw "Missing restore SCH: $schRestore" }
if (!(Test-Path $schSim)) { throw "Missing simulation SCH: $schSim" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-qa-open-editor-runtime-diag")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

# KiCad config: prefer the same bundled tree as run_ipc_v1_windows.ps1 / run_open_frame_ipc_smoke.ps1.
# A minimal generated kicad.json kept KiCad alive but the API server never bound (ping refused); full ipc-v1-config works.
$cfgRootBundled = Join-Path $BuildDir "ipc-v1-config"
$cfgRootGenerated = Join-Path $BuildDir "ipc-v1-qa-open-editor-diag-config"
$cfg10Bundled = Join-Path $cfgRootBundled "10.0"
$useBundledConfig = (Test-Path -LiteralPath (Join-Path $cfg10Bundled "kicad_common.json")) -and
    (Test-Path -LiteralPath (Join-Path $cfg10Bundled "kicad.json"))
if ($useBundledConfig) {
    $cfgRoot = $cfgRootBundled
    $cfg10 = $cfg10Bundled
}
else {
    $cfgRoot = $cfgRootGenerated
    $cfg10 = Join-Path $cfgRoot "10.0"
    New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
    $pythonExeForApi = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3\python.exe"
    $apiConfig = @{ enable_server = $true }
    if (Test-Path -LiteralPath $pythonExeForApi) {
        $apiConfig["interpreter_path"] = $pythonExeForApi
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
}
Copy-Item -Path (Join-Path $cfg10 "kicad_common.json") -Destination (Join-Path $runDir "kicad_common_injected.json") -Force
Copy-Item -Path (Join-Path $cfg10 "kicad.json") -Destination (Join-Path $runDir "kicad_injected.json") -Force
$env:KICAD_CONFIG_HOME = $cfgRoot
$env:KICAD_RUN_FROM_BUILD_DIR = "1"

$kicadSockDir = if ($env:OS -eq "Windows_NT") {
    Join-Path ([System.IO.Path]::GetTempPath().TrimEnd([char]0x005C)) "kicad"
}
elseif (($PSVersionTable.PSVersion.Major -ge 6) -and $IsMacOS) {
    Join-Path "/tmp" "kicad"
}
else {
    Join-Path ([System.IO.Path]::GetTempPath().TrimEnd([char]0x002F)) "kicad"
}

$kicadLaunchInvocationPath = Join-Path $runDir "kicad_launch_invocation.txt"
$kicadLaunchTimelinePath = Join-Path $runDir "kicad_launch_health_timeline.txt"
$kicadStdoutPath = Join-Path $runDir "kicad_stdout.log"
$kicadStderrPath = Join-Path $runDir "kicad_stderr.log"
$kicadConfigDumpPath = Join-Path $runDir "ipc_launch_config_effective_dump.txt"
$ipcLaunchVerdictPath = Join-Path $runDir "ipc_launch_verdict.txt"
$script:anyApiSockSeen = $false

function Get-ApiSockListingText {
    param([string]$SockDir)
    if (-not (Test-Path -LiteralPath $SockDir)) {
        return "(directory_missing: $SockDir)"
    }
    $items = @(Get-ChildItem -LiteralPath $SockDir -Filter "api*.sock" -ErrorAction SilentlyContinue)
    if ($items.Count -eq 0) {
        return "(none)"
    }
    return ($items | ForEach-Object { $_.FullName + " | LastWriteTimeUtc=" + $_.LastWriteTimeUtc.ToString("o") }) -join "`n"
}

function Write-KiCadLaunchTimeline {
    param([string]$Label, $KicadProc, [string]$SockDir)
    if ($null -eq $KicadProc) {
        Add-Content -Path $kicadLaunchTimelinePath -Value ("`n=== $Label ===`n(kicad process object is null)`n") -Encoding UTF8
        return
    }
    try { $KicadProc.Refresh() } catch { }
    $sockBlock = Get-ApiSockListingText -SockDir $SockDir
    if ($sockBlock -ne "(none)" -and $sockBlock -notmatch "directory_missing") {
        $script:anyApiSockSeen = $true
    }
    $exitDisp = "n/a"
    try {
        if ($KicadProc.HasExited) {
            try { [void]$KicadProc.WaitForExit(0) } catch { }
            $exitDisp = $KicadProc.ExitCode.ToString()
        }
    }
    catch { $exitDisp = "unknown" }
    $aliveByLookup = $false
    try {
        $aliveByLookup = $null -ne (Get-Process -Id $KicadProc.Id -ErrorAction SilentlyContinue)
    }
    catch { $aliveByLookup = $false }
    $block = @(
        "",
        "=== $Label ===",
        "utc=" + (Get-Date).ToUniversalTime().ToString("o"),
        "kicad_pid=" + $KicadProc.Id,
        "PassThru_HasExited=" + $KicadProc.HasExited,
        "PassThru_ExitCode=" + $exitDisp,
        "GetProcess_Id_StillAlive=" + $aliveByLookup,
        "api_sock_dir=" + $SockDir,
        "api_sock_files:",
        $sockBlock
    ) -join "`n"
    Add-Content -Path $kicadLaunchTimelinePath -Value $block -Encoding UTF8
}

function Write-IpcLaunchVerdict {
    param([string]$Code, [string]$Detail)
    @(
        "verdict_code=$Code",
        "detail=$Detail",
        "utc=" + (Get-Date).ToUniversalTime().ToString("o")
    ) | Set-Content -Path $ipcLaunchVerdictPath -Encoding UTF8
}

$commonPathForDump = Join-Path $cfg10 "kicad_common.json"
$kicadJsonPathForDump = Join-Path $cfg10 "kicad.json"
@(
    "--- kicad_common.json (effective path: $commonPathForDump) ---",
    (Get-Content -LiteralPath $commonPathForDump -Raw -ErrorAction SilentlyContinue),
    "",
    "--- kicad.json (effective path: $kicadJsonPathForDump) ---",
    (Get-Content -LiteralPath $kicadJsonPathForDump -Raw -ErrorAction SilentlyContinue)
) -join "`n" | Set-Content -Path $kicadConfigDumpPath -Encoding UTF8

function Format-SmokeArgumentLine {
    param([string[]]$SmokeArgs)
    $parts = @()
    foreach ($a in $SmokeArgs) {
        if ($null -eq $a) { continue }
        # ProcessStartInfo.Arguments: wrap in double-quotes; internal " → ""
        # Always quote ipc:// URLs: unquoted paths can be misparsed by the Windows C runtime (colon rules).
        if ($a.StartsWith("ipc://") -or $a -match '[\s"]') {
            $parts += '"' + ($a.Replace('"', '""')) + '"'
        }
        else {
            $parts += $a
        }
    }
    return ($parts -join ' ')
}

function New-SmokeProcessStartInfo {
    param([string[]]$SmokeArgs)
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $smokeExe
    $psi.Arguments = (Format-SmokeArgumentLine -SmokeArgs $SmokeArgs)
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $RepoRoot
    return $psi
}

function Run-SmokeBounded {
    param(
        [string[]]$SmokeArgs,
        [string]$OutPath,
        [string]$ErrPath,
        [int]$TimeoutMs
    )

    # Avoid Start-Process -ArgumentList array quirks (wrong argv order under Windows PowerShell 5.x).
    $psi = New-SmokeProcessStartInfo -SmokeArgs $SmokeArgs
    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $proc.Start() | Out-Null

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $proc.HasExited) {
        if ($sw.ElapsedMilliseconds -ge $TimeoutMs) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            return @{ Exited = $false; ExitCode = -1 }
        }
        Start-Sleep -Milliseconds 25
    }

    if (Test-Path $OutPath) { Remove-Item $OutPath -Force -ErrorAction SilentlyContinue }
    if (Test-Path $ErrPath) { Remove-Item $ErrPath -Force -ErrorAction SilentlyContinue }
    $outText = ""
    $errText = ""
    try { $outText = $proc.StandardOutput.ReadToEnd() } catch { $outText = "" }
    try { $errText = $proc.StandardError.ReadToEnd() } catch { $errText = "" }
    $outText | Set-Content -Path $OutPath -Encoding UTF8
    $errText | Set-Content -Path $ErrPath -Encoding UTF8
    return @{
        Exited = $true
        ExitCode = $proc.ExitCode
    }
}

Stop-Process -Name kicad -Force -ErrorAction SilentlyContinue
Stop-Process -Name qa_ipc_smoke -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
if (Test-Path $diagLog) { Remove-Item $diagLog -Force -ErrorAction SilentlyContinue }

# If both api.sock and api-<pid>.sock exist from a prior crash, KICAD_API_SERVER::Start() aborts (api_server.cpp).
if (Test-Path -LiteralPath $kicadSockDir) {
    Get-ChildItem -LiteralPath $kicadSockDir -Filter "api*.sock" -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

@(
    "1_exact_powershell_invocation (Start-Process):",
    "  Start-Process ``",
    "    -FilePath ``" + $kicadExe + "``",
    "    -ArgumentList ``" + $proj + "``",
    "    -WorkingDirectory ``" + $RepoRoot + "``",
    "    -PassThru ``",
    "    -WindowStyle Normal",
    "",
    "1b_no_stdout_stderr_redirect (Windows):",
    "  kicad.exe is a WIN32 GUI subsystem binary. Start-Process -RedirectStandardOutput/Error",
    "  forces inherited console handles and this build exits immediately (verdict 2, empty logs).",
    "  Same launch without redirect keeps KiCad alive; see qa/tools/ipc_tools/run_open_frame_ipc_smoke.ps1.",
    "  Placeholder notes are still written to kicad_stdout.log / kicad_stderr.log for artifact layout.",
    "",
    "2_equivalent_cmd_style_one_line:",
    "  """ + $kicadExe + """ """ + $proj + """",
    "  (cwd should be: " + $RepoRoot + ")",
    "",
    "3_environment_child_inherits:",
    "  KICAD_CONFIG_HOME=" + $cfgRoot,
    "  KICAD_CONFIG_SOURCE=" + $(if ($useBundledConfig) { "bundled_ipc_v1_config" } else { "generated_minimal" }),
    "  KICAD_RUN_FROM_BUILD_DIR=" + $env:KICAD_RUN_FROM_BUILD_DIR,
    "  PYTHONHOME=" + $(if ($null -ne $env:PYTHONHOME -and $env:PYTHONHOME -ne "") { $env:PYTHONHOME } else { "(unset)" }),
    "  KICAD_USE_EXTERNAL_PYTHONHOME=" + $(if ($null -ne $env:KICAD_USE_EXTERNAL_PYTHONHOME -and $env:KICAD_USE_EXTERNAL_PYTHONHOME -ne "") { $env:KICAD_USE_EXTERNAL_PYTHONHOME } else { "(unset)" }),
    "  PATH_head_512=" + $(if ($null -ne $env:PATH) { $env:PATH.Substring(0, [Math]::Min(512, $env:PATH.Length)) } else { "(unset)" }),
    "",
    "4_expected_api_socket_directory (matches KICAD_API_SERVER::Start):",
    "  " + $kicadSockDir,
    ""
) -join "`n" | Set-Content -Path $kicadLaunchInvocationPath -Encoding UTF8

$kicadProc = $null
try {
$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj `
    -WorkingDirectory $RepoRoot -PassThru `
    -WindowStyle Normal

if ($null -eq $kicadProc) {
    Write-IpcLaunchVerdict -Code "1" -Detail "Start-Process returned null (KiCad did not start)."
    throw "KiCad Start-Process returned null; see kicad_launch_invocation.txt and ipc_launch_verdict.txt"
}

$kicadProc.Refresh()
if ($kicadProc.HasExited) {
    # Same pattern as run_ipc_v1_windows.ps1: initial PID can exit while another kicad remains (hand-off).
    $adopt = Get-Process -Name kicad -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -ne $adopt) {
        Add-Content -Path $kicadLaunchInvocationPath -Value ("6_adopted_kicad_pid=" + $adopt.Id + " (PassThru pid had exited)") -Encoding UTF8
        $kicadProc = $adopt
    }
}

Set-Content -Path $kicadStdoutPath -Encoding UTF8 -Value @'
stdout not captured: kicad.exe is WIN32 GUI; Start-Process -RedirectStandard* forces immediate exit on this Windows build. See kicad_launch_invocation.txt section 1b.
'@
Set-Content -Path $kicadStderrPath -Encoding UTF8 -Value @'
stderr not captured: same reason as kicad_stdout.log (see kicad_launch_invocation.txt).
'@

Add-Content -Path $kicadLaunchInvocationPath -Value ("5_pid_after_start=" + $kicadProc.Id) -Encoding UTF8
Set-Content -Path $kicadLaunchTimelinePath -Value ("ipc_launch_health_timeline`nrepo_root=$RepoRoot`n") -Encoding UTF8
Write-KiCadLaunchTimeline -Label "T+0s_immediately_after_Start-Process" -KicadProc $kicadProc -SockDir $kicadSockDir

Start-Sleep -Seconds 5
Write-KiCadLaunchTimeline -Label "T+5s_after_Start-Process" -KicadProc $kicadProc -SockDir $kicadSockDir

    $kicadProc.Refresh()
    if ($kicadProc.HasExited) {
        $ex = $null
        try {
            try { [void]$kicadProc.WaitForExit(0) } catch { }
            $ex = $kicadProc.ExitCode.ToString()
        }
        catch { $ex = "unknown" }
        Write-IpcLaunchVerdict -Code "2" -Detail ("KiCad process exited before IPC resolve; ExitCode=" + $ex)
        throw "KiCad exited before IPC resolve (exit_code=$ex). See kicad_launch_health_timeline.txt, kicad_stdout.log, kicad_stderr.log, ipc_launch_verdict.txt"
    }
    $pidSockPath = Join-Path $kicadSockDir ("api-" + $kicadProc.Id + ".sock")
    $defaultSockPath = Join-Path $kicadSockDir "api.sock"
    [string[]]$candidates = @(
        "ipc://$pidSockPath",
        "ipc://$defaultSockPath"
    )
    @(
        "ipc_url_resolve_source=computed_KICAD_API_SERVER_Start",
        "kicad_socket_dir=$kicadSockDir",
        "ipc_url_candidate_count=$($candidates.Length)",
        "ipc_url_candidates=" + ($candidates -join " | "),
        "kicad_pid=$($kicadProc.Id)"
    ) | Set-Content -Path (Join-Path $runDir "ipc_url_resolve.txt") -Encoding UTF8

    $url = $null
    :resolveOuter for ($i = 0; $i -lt 100; $i++) {
        $cidx = 0
        foreach ($candidate in $candidates) {
            $pOut = Join-Path $runDir ("ipc_resolve_try_" + $i + "_" + $cidx + "_out.log")
            $pErr = Join-Path $runDir ("ipc_resolve_try_" + $i + "_" + $cidx + "_err.log")
            $pr = Run-SmokeBounded -SmokeArgs @($candidate, "ping") -OutPath $pOut -ErrPath $pErr -TimeoutMs 1500
            $pingText = ""
            if (Test-Path $pOut) { $pingText += (Get-Content $pOut -Raw) }
            if (Test-Path $pErr) { $pingText += (Get-Content $pErr -Raw) }
            if ($pr.Exited -and $pingText -match 'status=1') {
                $url = $candidate
                break resolveOuter
            }
            $cidx++
        }
        # Timeline + socket listing during resolve (every iteration documents %TEMP%\kicad\api*.sock state).
        Write-KiCadLaunchTimeline -Label ("ipc_resolve_iteration_" + $i) -KicadProc $kicadProc -SockDir $kicadSockDir
        Start-Sleep -Milliseconds 250
    }
    if (-not $url) {
        $kicadProc.Refresh()
        Write-KiCadLaunchTimeline -Label "T_fail_after_resolve_loop_exhausted" -KicadProc $kicadProc -SockDir $kicadSockDir
        $kp = Get-Process -Name kicad -ErrorAction SilentlyContinue |
            Select-Object Id, ProcessName, StartTime, Responding |
            Format-List | Out-String
        ("KICAD_CONFIG_HOME=" + $cfgRoot + "`n" +
         "kicad_process_probe:`n" + $kp) |
            Set-Content -Path (Join-Path $runDir "ipc_resolve_failure_probe.txt") -Encoding UTF8

        $commonDumpRaw = Get-Content -LiteralPath $commonPathForDump -Raw -ErrorAction SilentlyContinue
        $apiOffInDump = $false
        if ($commonDumpRaw -match '"enable_server"\s*:\s*false') { $apiOffInDump = $true }

        if ($kicadProc.HasExited) {
            $exf = "unknown"
            try { $exf = $kicadProc.ExitCode.ToString() } catch { }
            Write-IpcLaunchVerdict -Code "2" -Detail ("KiCad exited during IPC resolve loop; ExitCode=" + $exf + "; see timeline and kicad_stderr.log")
        }
        elseif ($apiOffInDump) {
            Write-IpcLaunchVerdict -Code "3" -Detail "Effective kicad_common.json shows api.enable_server=false (API server expected off)."
        }
        elseif (-not $script:anyApiSockSeen) {
            Write-IpcLaunchVerdict -Code "4" -Detail "KiCad still running (or was) but no api*.sock appeared under expected temp dir; see kicad_launch_health_timeline.txt"
        }
        else {
            Write-IpcLaunchVerdict -Code "4" -Detail "api*.sock files appeared but all ping attempts failed (e.g. nng_dial refused); see ipc_resolve_try_* logs"
        }
        throw "Unable to resolve API IPC URL (computed candidates ping failed; see ipc_url_resolve.txt, ipc_resolve_try_* logs, ipc_resolve_failure_probe.txt, ipc_launch_verdict.txt)"
    }
    Write-IpcLaunchVerdict -Code "5" -Detail ("IPC ping OK; resolved_url=" + $url)
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    # Open simulation schematic first.
    $simInput = Join-Path $runDir "open_sim_input.json"
    (@{ type="sch"; filename=$schSim } | ConvertTo-Json -Compress) | Set-Content -Path $simInput -Encoding UTF8
    $simOpenOut = Join-Path $runDir "open_sim_out.log"
    $simOpenErr = Join-Path $runDir "open_sim_err.log"
    [void](Run-SmokeBounded -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $simInput)) `
            -OutPath $simOpenOut -ErrPath $simOpenErr -TimeoutMs 12000)

    # Warm simulation path.
    $simProbeInput = Join-Path $runDir "sim_xaxis_input.json"
    (@{ timeout_ms=3000 } | ConvertTo-Json -Compress) | Set-Content -Path $simProbeInput -Encoding UTF8
    $simProbeOut = Join-Path $runDir "sim_xaxis_out.log"
    $simProbeErr = Join-Path $runDir "sim_xaxis_err.log"
    [void](Run-SmokeBounded -SmokeArgs @($url, "mcp_call_tool_args", "simulation_xaxis_probe_session", ("@" + $simProbeInput)) `
            -OutPath $simProbeOut -ErrPath $simProbeErr -TimeoutMs 15000)

    # Trigger restore and keep it running to probe liveness.
    $restoreInput = Join-Path $runDir "restore_input.json"
    (@{ type="sch"; filename=$schRestore } | ConvertTo-Json -Compress) | Set-Content -Path $restoreInput -Encoding UTF8
    $restoreOut = Join-Path $runDir "restore_out.log"
    $restoreErr = Join-Path $runDir "restore_err.log"
    # Comma-separated -ArgumentList avoids PS 5.x argv reordering seen with @(...) arrays.
    $restoreProc = Start-Process -FilePath $smokeExe `
            -ArgumentList $url, 'mcp_call_tool_args', 'qa_open_editor_frame', ('@' + $restoreInput) `
            -NoNewWindow -PassThru `
            -RedirectStandardOutput $restoreOut `
            -RedirectStandardError $restoreErr

    $pingWhileRestore = @()
    for($i=0; $i -lt 8; $i++) {
        Start-Sleep -Milliseconds 900
        $pOut = Join-Path $runDir ("ping_while_restore_" + $i + "_out.log")
        $pErr = Join-Path $runDir ("ping_while_restore_" + $i + "_err.log")
        $pr = Run-SmokeBounded -SmokeArgs @($url, "ping") -OutPath $pOut -ErrPath $pErr -TimeoutMs 1200
        $pText = ""
        if(Test-Path $pOut){ $pText += (Get-Content $pOut -Raw) }
        if(Test-Path $pErr){ $pText += (Get-Content $pErr -Raw) }
        $pingWhileRestore += ("ping_" + $i + "_ok=" + ($(if($pr.Exited -and $pText -match 'status=1'){"true"}else{"false"})))
    }

    $restoreExited = $restoreProc.WaitForExit(1000)
    if(-not $restoreExited) {
        Stop-Process -Id $restoreProc.Id -Force -ErrorAction SilentlyContinue
    }

    $kicadAlive = $(if (Get-Process -Id $kicadProc.Id -ErrorAction SilentlyContinue) { "true" } else { "false" })
    $restoreOutText = $(if(Test-Path $restoreOut){ Get-Content $restoreOut -Raw } else { "" })
    $restoreErrText = $(if(Test-Path $restoreErr){ Get-Content $restoreErr -Raw } else { "" })
    $restoreResponseSeen = $(if($restoreOutText -match 'status=' -or $restoreErrText -match 'status='){"true"}else{"false"})

    $apiTailPath = Join-Path $runDir "api_tail.log"
    if (Test-Path $apiLog) {
        Get-Content $apiLog -Tail 200 | Set-Content -Path $apiTailPath -Encoding UTF8
    }
    $diagTailPath = Join-Path $runDir "qa_open_editor_frame_diag_tail.log"
    if (Test-Path $diagLog) {
        Get-Content $diagLog -Tail 200 | Set-Content -Path $diagTailPath -Encoding UTF8
    }

    @(
        "ipc_url_resolve_source=computed_KICAD_API_SERVER_Start"
        "resolved_ipc_url=" + $url
        "restore_process_exited=" + $(if($restoreExited){"true"}else{"false"})
        "restore_response_seen=" + $restoreResponseSeen
        "kicad_alive_during_nonreturn=" + $kicadAlive
        "api_ping_during_restore=" + ($pingWhileRestore -join ";")
        "diag_log_present=" + $(if(Test-Path $diagLog){"true"}else{"false"})
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_runtime_diag_summary.txt") -Encoding UTF8

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
}
catch {
    if (-not (Test-Path -LiteralPath $ipcLaunchVerdictPath)) {
        Write-IpcLaunchVerdict -Code "1" -Detail ("Unhandled exception before verdict was written: " + $_.Exception.Message)
    }
    throw
}
finally
{
    Stop-Process -Name qa_ipc_smoke -Force -ErrorAction SilentlyContinue
    if ($null -ne $kicadProc) {
        Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
    }
}
