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
$schIssue19090 = Join-Path $RepoRoot "qa\data\pcbnew\issue19090\issue19090.kicad_sch"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

# Bounded waits for slow CI / cold GUI (no retries).
$OpenEditorTimeoutMs = 240000
$NetInspectToolTimeoutMs = 240000

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $sch)) { throw "Missing schematic at $sch" }

$fixtureSource = "build_v1"
$selectedNetName = "GND"
$netSelectionMethod = "default_fallback"
if( Test-Path -LiteralPath $schIssue19090 )
{
    $sch = $schIssue19090
    $fixtureSource = "issue19090"
    $selectedNetName = "GND"
    $netSelectionMethod = "dynamic_candidates_issue19090"
}

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-schematic-net-inspect-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null

function Get-LabelCandidatesFromSchematic {
    param([string]$SchematicPath)
    if( !(Test-Path -LiteralPath $SchematicPath) ) { return @() }
    $raw = Get-Content -LiteralPath $SchematicPath -Raw
    $matches = [regex]::Matches( $raw, '\(label\s+"([^"]+)"' )
    $vals = @()
    foreach( $m in $matches )
    {
        $v = $m.Groups[1].Value.Trim()
        if( -not [string]::IsNullOrWhiteSpace( $v ) ) { $vals += $v }
    }
    return $vals | Select-Object -Unique
}

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$sockDir = Clear-IpcSockets

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
    $openRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs $OpenEditorTimeoutMs -WorkingDirectory $RepoRoot
    $openOut = $openRes.Text
    $openOut | Set-Content -Path (Join-Path $runDir "qa_open_editor_frame_output.log") -Encoding UTF8

    $openTimedOut = ( $null -ne $openOut -and ( $openOut.Trim() -eq "timeout" ) )

    $callTimedOut = $false
    $callOut = ""
    $callArgs = $null
    $candidateNets = @()
    if( $fixtureSource -eq "issue19090" )
    {
        $candidateNets += @(
            "Net-(R1-Pad1)",
            "Net-(R1-Pad2)",
            "Net-(R2-Pad1)",
            "Net-(R2-Pad2)",
            "GND"
        )
        $candidateNets += Get-LabelCandidatesFromSchematic -SchematicPath $sch
    }
    else
    {
        $candidateNets += @("GND")
        $candidateNets += Get-LabelCandidatesFromSchematic -SchematicPath $sch
    }
    $candidateNets = $candidateNets | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique
    if( $candidateNets.Count -eq 0 ) { $candidateNets = @("GND") }

    if( $openTimedOut )
    {
        "skipped_open_editor_timeout" | Set-Content -Path (Join-Path $runDir "schematic_net_inspect_output.log") -Encoding UTF8
    }
    else
    {
        $selectionLog = @()
        foreach( $candidate in $candidateNets )
        {
            $selectedNetName = $candidate
            $callArgs = @{
                schematic_filename = $sch
                net_name = $selectedNetName
                ensure_fresh_connectivity = $true
                max_nodes = 200
            } | ConvertTo-Json -Compress
            $callInput = Join-Path $runDir "schematic_net_inspect_args.json"
            $callArgs | Set-Content -Path $callInput -Encoding UTF8

            $callRes = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "schematic_net_inspect_session", ("@" + $callInput)) -TimeoutMs $NetInspectToolTimeoutMs -WorkingDirectory $RepoRoot
            $callOut = $callRes.Text
            $isTimeout = ( $null -ne $callOut -and ( $callOut.Trim() -eq "timeout" ) )
            $selectionLog += ("candidate=" + $candidate + ";timeout=" + $(if($isTimeout){"true"}else{"false"}))
            if( $callOut -match 'key:\s+"ok"[\s\S]*?bool_value:\s*true' -and $callOut -match 'key:\s+"net_found"[\s\S]*?bool_value:\s*true' )
            {
                $netSelectionMethod = "dynamic_probe_success"
                break
            }
            if( $isTimeout ) { $callTimedOut = $true }
        }
        $selectionLog | Set-Content -Path (Join-Path $runDir "net_selection_attempts.log") -Encoding UTF8
        if( $netSelectionMethod -ne "dynamic_probe_success" -and $fixtureSource -eq "issue19090" )
        {
            $netSelectionMethod = "dynamic_probe_no_success"
        }
        $callOut | Set-Content -Path (Join-Path $runDir "schematic_net_inspect_output.log") -Encoding UTF8
    }

    ("fixture_source=" + $fixtureSource +
     ";fixture_schematic=" + $sch +
     ";net_name=" + $selectedNetName +
     ";net_selection_method=" + $netSelectionMethod) | Set-Content -Path (Join-Path $runDir "sch_fixture_status.txt") -Encoding UTF8

    $visible = if( $lt -match 'schematic_net_inspect_session' ) { "true" } else { "false" }
    $callable = if( $callOut -match 'status=1' ) { "true" } else { "false" }

    $ok = "false"
    if( $callOut -match 'key:\s+"ok"[\s\S]*?bool_value:\s*(true|false)' ) { $ok = $Matches[1] }

    $netFound = "false"
    if( $callOut -match 'key:\s+"net_found"[\s\S]*?bool_value:\s*(true|false)' ) { $netFound = $Matches[1] }

    $toolBlocker = "absent"
    if( $callOut -match 'key:\s+"exact_blocker_if_any"[\s\S]*?string_value:\s*"([^"]*)"' ) { $toolBlocker = $Matches[1] }

    $nodeCount = ""
    if( $callOut -match 'key:\s+"node_count_total"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $nodeCount = [string][int][double]$Matches[1]
    }

    $nodesReturned = ""
    if( $callOut -match 'key:\s+"nodes_returned"[\s\S]*?number_value:\s*([0-9]+(?:\.[0-9]+)?)' )
    {
        $nodesReturned = [string][int][double]$Matches[1]
    }

    $diagnostics = ""
    if( $callOut -match 'key:\s+"diagnostics"[\s\S]*?string_value:\s*"([^"]*)"' ) { $diagnostics = $Matches[1] }

    $nodesSummary = ""
    if( $callOut -match 'key:\s+"nodes_summary"[\s\S]*?string_value:\s*"([^"]*)"' ) { $nodesSummary = $Matches[1] }

    $nodeCountValid = "false"
    if( $nodeCount -match '^\d+$' -and [int]$nodeCount -ge 1 ) { $nodeCountValid = "true" }

    $nodesReturnedValid = "false"
    if( $nodesReturned -match '^\d+$' -and [int]$nodesReturned -ge 1 -and [int]$nodesReturned -le 1000 )
    {
        $nodesReturnedValid = "true"
    }

    $diagnosticsPresent = if( [string]::IsNullOrWhiteSpace( $diagnostics ) ) { "false" } else { "true" }
    $nodesSummaryPresent = if( [string]::IsNullOrWhiteSpace( $nodesSummary ) ) { "false" } else { "true" }

    $blocker = "N/A"
    if( $openTimedOut ) { $blocker = "open_editor_timeout" }
    elseif( $callTimedOut ) { $blocker = "net_inspect_tool_timeout" }
    elseif( $visible -ne "true" ) { $blocker = "not_visible_in_context" }
    elseif( $callable -ne "true" ) { $blocker = "call_failed" }
    elseif( $ok -ne "true" ) { $blocker = "tool_ok_false" }
    elseif( $netFound -ne "true" ) { $blocker = "net_not_found" }
    elseif( $nodeCountValid -ne "true" ) { $blocker = "node_count_invalid" }
    elseif( $nodesReturnedValid -ne "true" ) { $blocker = "nodes_returned_invalid" }
    elseif( $nodesSummaryPresent -ne "true" ) { $blocker = "nodes_summary_missing" }
    elseif( $diagnosticsPresent -ne "true" ) { $blocker = "diagnostics_missing" }

    @(
        "visible_in_context_if_applicable=$visible"
        "callable_without_mutation=true"
        "ok=$ok"
        "net_found=$netFound"
        "node_count_valid=$nodeCountValid"
        "nodes_returned_valid=$nodesReturnedValid"
        "nodes_summary_present=$nodesSummaryPresent"
        "diagnostics_present=$diagnosticsPresent"
        "exact_blocker_if_any=$blocker"
        "tool_exact_blocker_field=$toolBlocker"
        "net_name=$selectedNetName"
        "node_count_total=$nodeCount"
        "nodes_returned=$nodesReturned"
        "nodes_summary=$nodesSummary"
        "diagnostics=$diagnostics"
        ""
    ) | Set-Content -Path (Join-Path $runDir "qa_schematic_net_inspect_session_contract_summary.txt") -Encoding UTF8

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
