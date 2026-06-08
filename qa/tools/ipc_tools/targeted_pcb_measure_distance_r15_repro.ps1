# Mission R15: characterize pcb_measure_distance_session item pairs (timing + blocker).
# Does not gate CI; writes a summary under ipc-v1-artifacts.

param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [int]$TimeoutMs = 130000
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
$pcb      = Join-Path $BuildDir "ipc-v1-proj\v1.kicad_pcb"
$cfgRoot  = Join-Path $BuildDir "ipc-v1-config"
$cfg10    = Join-Path $cfgRoot "10.0"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }
if (!(Test-Path $proj)) { throw "Missing project at $proj" }
if (!(Test-Path $pcb)) { throw "Missing board at $pcb" }

$pcbAbs = (Resolve-Path $pcb).Path

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-measure-r15-repro")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

function Get-ProtoBlocker {
    param([string]$Text)
    if ($Text -match 'key:\s+"exact_blocker_if_any"[\s\S]{0,1200}?string_value:\s*"([^"]*)"') { return $Matches[1] }
    return ""
}

function Invoke-MeasureTimed {
    param([string]$ApiUrl, [hashtable]$ArgsTable, [string]$Label)
    $jsonPath = Join-Path $runDir ("r15_" + $Label + ".args.json")
    ($ArgsTable | ConvertTo-Json -Compress -Depth 8) | Set-Content -Path $jsonPath -Encoding UTF8
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $res = Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "pcb_measure_distance_session", ("@" + $jsonPath)) -TimeoutMs $TimeoutMs -WorkingDirectory $RepoRoot
    $sw.Stop()
    $res.Text | Set-Content -Path (Join-Path $runDir ("r15_" + $Label + ".log")) -Encoding UTF8
    $statusOk = ($res.Text -match 'status=1')
    $timeout = ($res.Text -eq "timeout")
    $okLine = "false"
    if ($res.Text -match 'key:\s+"ok"[\s\S]{0,1200}?bool_value:\s*(true|false)') { $okLine = $Matches[1] }
    return [pscustomobject]@{
        Label       = $Label
        ElapsedMs   = $sw.ElapsedMilliseconds
        TimeoutHit  = $timeout
        StatusOk    = $statusOk
        OkResult    = $okLine
        Blocker     = (Get-ProtoBlocker -Text $res.Text)
        LogPath     = (Join-Path $runDir ("r15_" + $Label + ".log"))
    }
}

function Get-FirstItemId {
    param([string]$ApiUrl, [string]$Label, [string[]]$ItemTypes)
    $argsPath = Join-Path $runDir ("r15_board_items_" + $Label + ".args.json")
    $payload = @{ board_filename = $pcbAbs; max_items = 1 }
    if ($null -ne $ItemTypes -and $ItemTypes.Count -gt 0) { $payload.item_types = $ItemTypes }
    ($payload | ConvertTo-Json -Compress -Depth 6) | Set-Content -Path $argsPath -Encoding UTF8
    $txt = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($ApiUrl, "mcp_call_tool_args", "pcb_get_board_items_session", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot).Text
    $txt | Set-Content -Path (Join-Path $runDir ("r15_board_items_" + $Label + ".log")) -Encoding UTF8
    foreach ($line in ($txt -split "`r?`n")) {
        if ($line -match '^first_item_id=(.+)$') { return $Matches[1].Trim() }
    }
    return $null
}

Stop-KiCadProcesses
Start-Sleep -Seconds 2
$null = Clear-IpcSockets

$kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $proj -WorkingDirectory $RepoRoot -PassThru
try {
    $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
        -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
    $url = if ($resolved.Resolved) { $resolved.Url } else { $null }
    if (-not $url) {
        @("r15_repro_failed=ipc_url_unresolved") | Set-Content -Path (Join-Path $runDir "r15_repro_summary.txt") -Encoding UTF8
        Write-Output ("TARGET_RUN_DIR=" + $runDir)
        exit 1
    }
    $null = Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot

    $openArgs = @{ type = "pcb"; filename = $pcbAbs } | ConvertTo-Json -Compress
    $openInput = Join-Path $runDir "r15_qa_open_editor_frame.json"
    $openArgs | Set-Content -Path $openInput -Encoding UTF8
    $openOut = (Invoke-SmokeBounded -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot).Text
    $openOut | Set-Content -Path (Join-Path $runDir "r15_qa_open_editor_frame.log") -Encoding UTF8
    Start-Sleep -Seconds 3

    $idFp  = Get-FirstItemId -ApiUrl $url -Label "fp" -ItemTypes @("footprints")
    $idPad = Get-FirstItemId -ApiUrl $url -Label "pad" -ItemTypes @("pads")
    $idDrw = Get-FirstItemId -ApiUrl $url -Label "drw" -ItemTypes @("drawings")

    @(
        "fixture_board=$pcbAbs"
        "timeout_ms=$TimeoutMs"
        "id_footprint=$idFp"
        "id_pad=$idPad"
        "id_drawing=$idDrw"
        ""
    ) | Set-Content -Path (Join-Path $runDir "r15_ids.txt") -Encoding UTF8

    $rows = New-Object System.Collections.Generic.List[object]

    if ($idFp -and $idPad) {
        $rows.Add((Invoke-MeasureTimed -ApiUrl $url -Label "footprint_plus_child_pad_a_fp_b_pad" -ArgsTable @{
                board_filename = $pcbAbs
                a              = @{ item_id = $idFp }
                b              = @{ item_id = $idPad }
            }))
    }
    if ($idFp -and $idDrw) {
        $rows.Add((Invoke-MeasureTimed -ApiUrl $url -Label "footprint_plus_drawing_a_fp_b_drw" -ArgsTable @{
                board_filename = $pcbAbs
                a              = @{ item_id = $idFp }
                b              = @{ item_id = $idDrw }
            }))
    }
    if ($idPad -and $idDrw) {
        $rows.Add((Invoke-MeasureTimed -ApiUrl $url -Label "pad_plus_drawing_a_pad_b_drw" -ArgsTable @{
                board_filename = $pcbAbs
                a              = @{ item_id = $idPad }
                b              = @{ item_id = $idDrw }
            }))
    }
    if ($idPad) {
        $rows.Add((Invoke-MeasureTimed -ApiUrl $url -Label "pad_plus_pad_same_id" -ArgsTable @{
                board_filename = $pcbAbs
                a              = @{ item_id = $idPad }
                b              = @{ item_id = $idPad }
            }))
    }

    $lines = New-Object System.Collections.Generic.List[string]
    foreach ($r in $rows) {
        $lines.Add(($r.Label + "|elapsed_ms=" + $r.ElapsedMs + "|timeout=" + $r.TimeoutHit + "|status_ok=" + $r.StatusOk + "|ok=" + $r.OkResult + "|exact_blocker_if_any=" + $r.Blocker))
    }
    $lines | Set-Content -Path (Join-Path $runDir "r15_repro_summary.txt") -Encoding UTF8

    foreach ($r in $rows) {
        Write-Output ($r.Label + " elapsed_ms=" + $r.ElapsedMs + " timeout=" + $r.TimeoutHit + " blocker=" + $r.Blocker)
    }

    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 0
}
finally {
    Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
}
