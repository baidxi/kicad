param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [int]$ToolTimeoutMs = 180000,

    [int]$PingTimeoutMs = 20000,

    # One call only: GND, ensure_fresh_connectivity=false, max_nodes=1 (instrumentation checkpoint run).
    [switch]$SingleInstrumentedCall,

    # Restarts KiCad per mode with KICAD_MCP_NET_INSPECT_PAYLOAD_MODE set (requires rebuilt eeschema).
    # Implies -SingleInstrumentedCall.
    [switch]$NetInspectPayloadDiagSweep
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
if (!(Test-Path -LiteralPath $commonModule)) { throw "Missing $commonModule" }
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$proj     = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pro"
$sch      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"
$apiLog   = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\api.log"

if (!(Test-Path $kicadExe)) { throw "Missing $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing $proj" }
if (!(Test-Path $sch)) { throw "Missing $sch" }

if ($NetInspectPayloadDiagSweep -and -not $SingleInstrumentedCall) {
    $SingleInstrumentedCall = $true
}

$runStamp = (Get-Date -Format "yyyyMMdd-HHmmssfff")
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

$pythonHomeDir = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"
$pythonExe = Join-Path $pythonHomeDir "python.exe"
$apiConfig = @{ enable_server = $true }
if (Test-Path $pythonExe) { $apiConfig.interpreter_path = $pythonExe }

$common = @{
    meta = @{ version = 5 }
    api  = $apiConfig
    system = @{
        local_history_enabled = $false
        working_dir           = $RepoRoot
    }
    auto_backup = @{ enabled = $false; limit_total_size = 104857600 }
    do_not_show_again = @{
        data_collection_prompt    = $true
        update_check_prompt       = $true
        env_var_overwrite_warning = $true
        scaled_3d_models_warning  = $true
        zone_fill_warning         = $true
    }
} | ConvertTo-Json -Depth 10

$kicadCfg = @{
    meta   = @{ version = 0 }
    system = @{ first_run_shown = $true; open_projects = @() }
} | ConvertTo-Json -Depth 6

$common | Set-Content -Path (Join-Path $cfg10 "kicad_common.json") -Encoding UTF8
$kicadCfg | Set-Content -Path (Join-Path $cfg10 "kicad.json") -Encoding UTF8
"EnableAPILogging=1" | Set-Content -Path (Join-Path $cfg10 "kicad_advanced") -Encoding ASCII

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation
$env:KICAD_IPC_STARTUP_DIAG = "1"

function Get-ApiLogTail {
    param([int]$Lines = 80)
    if (Test-Path -LiteralPath $apiLog) {
        return (Get-Content -LiteralPath $apiLog -Tail $Lines -ErrorAction SilentlyContinue | Out-String)
    }
    return "(api.log absent at $apiLog)"
}

function Invoke-SmokeProcess {
    param(
        [string[]]$SmokeArgumentList,
        [int]$TimeoutMs,
        [string]$WorkDir
    )
    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $smokeExe
    $parts = @()
    foreach ($a in $SmokeArgumentList) {
        if ($null -eq $a) { continue }
        if ($a.StartsWith("ipc://") -or $a -match '[\s"]') {
            $parts += '"' + ($a.Replace('"', '""')) + '"'
        } else {
            $parts += $a
        }
    }
    $psi.Arguments = ($parts -join ' ')
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $WorkDir

    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    $null = $p.Start()
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while (-not $p.HasExited) {
        if ($sw.ElapsedMilliseconds -ge $TimeoutMs) {
            try { $p.Kill() } catch { }
            return @{
                Exited    = $false
                ExitCode  = -1
                StdOut    = ""
                StdErr    = ""
                TimedOut  = $true
            }
        }
        Start-Sleep -Milliseconds 30
    }
    $out = ""
    $err = ""
    try { $out = $p.StandardOutput.ReadToEnd() } catch { $out = "" }
    try { $err = $p.StandardError.ReadToEnd() } catch { $err = "" }
    return @{
        Exited    = $true
        ExitCode  = $p.ExitCode
        StdOut    = $out
        StdErr    = $err
        TimedOut  = $false
    }
}

function Test-KiCadAlive {
    param([int]$ProcessId)
    try {
        $null = Get-Process -Id $ProcessId -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Count-ApiLogMarkers {
    param([string]$Text)
    $req = ([regex]::Matches($Text, "Request:")).Count
    $res = ([regex]::Matches($Text, "Response:")).Count
    return @{ Request = $req; Response = $res }
}

$payloadIterations = if ($NetInspectPayloadDiagSweep) {
    @('scalar_only', 'empty_nodes', 'nodes_summary_string')
} else {
    @([string]$null)
}

foreach ($payloadDiagMode in $payloadIterations) {
    if ($payloadDiagMode) {
        $env:KICAD_MCP_NET_INSPECT_PAYLOAD_MODE = $payloadDiagMode
    } else {
        Remove-Item Env:\KICAD_MCP_NET_INSPECT_PAYLOAD_MODE -ErrorAction SilentlyContinue
    }

    $modeSuffix = if ($payloadDiagMode) { "-payload-" + $payloadDiagMode } else { "" }
    $runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + $runStamp + "-diag-schematic-net-inspect-timeout" + $modeSuffix)
    New-Item -ItemType Directory -Force -Path $runDir | Out-Null
    "KICAD_MCP_NET_INSPECT_PAYLOAD_MODE=$payloadDiagMode" | Set-Content (Join-Path $runDir "payload_env.txt") -Encoding UTF8

    Stop-KiCadProcesses
    Start-Sleep -Seconds 2
    $null = Clear-IpcSockets

    $kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru

    try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    if (-not $resolved.Resolved) { throw "IPC URL not resolved" }
    $url = $resolved.Url
    $url | Set-Content -Path (Join-Path $runDir "url.txt") -Encoding UTF8

    $ping0 = Invoke-SmokeProcess -SmokeArgumentList @($url, "ping") -TimeoutMs $PingTimeoutMs -WorkDir $RepoRoot
    @{ phase = "ping_initial" } | ConvertTo-Json -Compress | Set-Content (Join-Path $runDir "ping_initial.meta.json") -Encoding UTF8
    Set-Content -Path (Join-Path $runDir "ping_initial.stdout.log") -Value $ping0.StdOut -Encoding UTF8
    Set-Content -Path (Join-Path $runDir "ping_initial.stderr.log") -Value $ping0.StdErr -Encoding UTF8
    "exit=$($ping0.ExitCode);timedOut=$($ping0.TimedOut)" | Set-Content (Join-Path $runDir "ping_initial.exit.txt") -Encoding UTF8

    $openArgs = @{ type = "sch"; filename = $sch } | ConvertTo-Json -Compress
    $openPath = Join-Path $runDir "qa_open_editor_frame.json"
    $openArgs | Set-Content -Path $openPath -Encoding UTF8
    $openRes = Invoke-SmokeProcess -SmokeArgumentList @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openPath)) -TimeoutMs 30000 -WorkDir $RepoRoot
    Set-Content -Path (Join-Path $runDir "qa_open_editor_frame.stdout.log") -Value $openRes.StdOut -Encoding UTF8
    Set-Content -Path (Join-Path $runDir "qa_open_editor_frame.stderr.log") -Value $openRes.StdErr -Encoding UTF8
    "exit=$($openRes.ExitCode);timedOut=$($openRes.TimedOut)" | Set-Content (Join-Path $runDir "qa_open_editor_frame.exit.txt") -Encoding UTF8

    $apiBefore = Get-ApiLogTail 120
    $apiBefore | Set-Content (Join-Path $runDir "api_log_tail_before_variants.log") -Encoding UTF8

    $diagLogPath = Join-Path ([System.IO.Path]::GetTempPath().TrimEnd( [char]0x005C, [char]0x002F )) "kicad\schematic_net_inspect_diag.log"
    if (Test-Path -LiteralPath $diagLogPath) {
        Remove-Item -LiteralPath $diagLogPath -Force -ErrorAction SilentlyContinue
    }

    $variants = @(
        @{ id = "1_nominal"; obj = @{ schematic_filename = $sch; net_name = "GND"; ensure_fresh_connectivity = $true; max_nodes = 200 } },
        @{ id = "2_no_recalc"; obj = @{ schematic_filename = $sch; net_name = "GND"; ensure_fresh_connectivity = $false; max_nodes = 200 } },
        @{ id = "3_cap1"; obj = @{ schematic_filename = $sch; net_name = "GND"; ensure_fresh_connectivity = $false; max_nodes = 1 } },
        @{ id = "4_bad_net"; obj = @{ schematic_filename = $sch; net_name = "__MCP_NET_DOES_NOT_EXIST__"; ensure_fresh_connectivity = $false; max_nodes = 1 } }
    )

    if ($SingleInstrumentedCall) {
        $variants = @(
            @{ id = "single_gnd_no_recalc_cap1"; obj = @{ schematic_filename = $sch; net_name = "GND"; ensure_fresh_connectivity = $false; max_nodes = 1 } }
        )
    }

    $summaryLines = @()

    foreach ($v in $variants) {
        $id = $v.id
        $jsonPath = Join-Path $runDir ("variant_" + $id + "_args.json")
        ($v.obj | ConvertTo-Json -Compress) | Set-Content -Path $jsonPath -Encoding UTF8

        $tailBefore = Get-ApiLogTail 200
        $markerBefore = Count-ApiLogMarkers -Text $tailBefore

        $r = Invoke-SmokeProcess -SmokeArgumentList @($url, "mcp_call_tool_args", "schematic_net_inspect_session", ("@" + $jsonPath)) -TimeoutMs $ToolTimeoutMs -WorkDir $RepoRoot

        $tailAfter = Get-ApiLogTail 400
        $markerAfter = Count-ApiLogMarkers -Text $tailAfter

        Set-Content (Join-Path $runDir ("variant_" + $id + ".stdout.log")) -Value $r.StdOut -Encoding UTF8
        Set-Content (Join-Path $runDir ("variant_" + $id + ".stderr.log")) -Value $r.StdErr -Encoding UTF8
        "exit=$($r.ExitCode);timedOut=$($r.TimedOut)" | Set-Content (Join-Path $runDir ("variant_" + $id + ".exit.txt")) -Encoding UTF8

        $alive = Test-KiCadAlive -ProcessId $kicadProc.Id

        $pingPost = Invoke-SmokeProcess -SmokeArgumentList @($url, "ping") -TimeoutMs $PingTimeoutMs -WorkDir $RepoRoot
        if ($SingleInstrumentedCall) {
            Set-Content (Join-Path $runDir ("variant_" + $id + "_post_ping.stdout.log")) -Value $pingPost.StdOut -Encoding UTF8
            "post_ping_exit=$($pingPost.ExitCode);post_ping_timeout=$($pingPost.TimedOut);kicad_alive=$alive" |
                Set-Content (Join-Path $runDir ("variant_" + $id + "_post_checks.exit.txt")) -Encoding UTF8
        }
        else {
            $listPost = Invoke-SmokeProcess -SmokeArgumentList @($url, "mcp_list_tools") -TimeoutMs $PingTimeoutMs -WorkDir $RepoRoot
            $gItemsArgs = @{ schematic_filename = $sch; types = @("SCH_SYMBOL_T") } | ConvertTo-Json -Compress
            $gItemsPath = Join-Path $runDir ("variant_" + $id + "_get_sch_items.json")
            $gItemsArgs | Set-Content -Path $gItemsPath -Encoding UTF8
            $gItemsPost = Invoke-SmokeProcess -SmokeArgumentList @($url, "mcp_call_tool_args", "get_sch_items", ("@" + $gItemsPath)) -TimeoutMs 60000 -WorkDir $RepoRoot

            Set-Content (Join-Path $runDir ("variant_" + $id + "_post_ping.stdout.log")) -Value $pingPost.StdOut -Encoding UTF8
            Set-Content (Join-Path $runDir ("variant_" + $id + "_post_mcp_list.stdout.log")) -Value $listPost.StdOut -Encoding UTF8
            Set-Content (Join-Path $runDir ("variant_" + $id + "_post_get_sch_items.stdout.log")) -Value $gItemsPost.StdOut -Encoding UTF8
            "post_ping_exit=$($pingPost.ExitCode);post_ping_timeout=$($pingPost.TimedOut);post_list_exit=$($listPost.ExitCode);post_items_exit=$($gItemsPost.ExitCode)" |
                Set-Content (Join-Path $runDir ("variant_" + $id + "_post_checks.exit.txt")) -Encoding UTF8
        }

        if (Test-Path -LiteralPath $diagLogPath) {
            Copy-Item -LiteralPath $diagLogPath -Destination (Join-Path $runDir "schematic_net_inspect_diag.log") -Force
        }

        $reqDelta = $markerAfter.Request - $markerBefore.Request
        $resDelta = $markerAfter.Response - $markerBefore.Response

        $line = "${id}: timedOut=$($r.TimedOut); exit=$($r.ExitCode); kicad_alive=$alive; api_req_delta=$reqDelta; api_res_delta=$resDelta; stdout_len=$($r.StdOut.Length); stderr_len=$($r.StdErr.Length)"
        $summaryLines += $line
    }

        $summaryLines | Set-Content (Join-Path $runDir "variant_summary.txt") -Encoding UTF8
        Write-Output ("DIAG_RUN_DIR=" + $runDir)
    }
    finally {
        if ($null -ne $kicadProc) {
            Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        }
    }
}
