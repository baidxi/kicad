param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R170 — get_pcb_sync_readiness_session V1 read-only probe (empty / synced / orphan fixtures).

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
. $commonModule

$kicadExe = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$cfg10 = Join-Path $cfgRoot "10.0"
$qaResistorPretty = Join-Path $RepoRoot "qa\data\libraries\Resistor_SMD.pretty"

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe" }
if (!(Test-Path $qaResistorPretty)) { throw "Missing QA footprint library at $qaResistorPretty" }

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-sync-readiness-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library for pcb sync readiness"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-PcbSyncFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseName
    )
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $pcbSrc = Join-Path $RepoRoot ("qa\data\pcbnew\sync\ipc_pcb_sync_v1_" + $CaseName)
    $dst = Join-Path $BuildDir ("ipc-pcb-sync-v1-" + $CaseName + "-proj")
    if (!(Test-Path -LiteralPath $baseSrc)) { throw "Missing base fixture at $baseSrc" }
    if (!(Test-Path -LiteralPath $pcbSrc)) { throw "Missing pcb variant at $pcbSrc" }
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item -Path (Join-Path $baseSrc "*") -Destination $dst -Recurse -Force
    $pcbVariant = Join-Path $pcbSrc ("ipc_pcb_sync_v1_" + $CaseName + ".kicad_pcb")
    if (!(Test-Path -LiteralPath $pcbVariant)) { throw "Missing pcb variant file $pcbVariant" }
    Copy-Item -Path $pcbVariant -Destination (Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb") -Force
    $projectFpLibTable = Join-Path $dst "fp-lib-table"
    @(
        "(fp_lib_table",
        "  (version 7)",
        ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library"))'),
        ")"
    ) | Set-Content -Path $projectFpLibTable -Encoding ASCII
    return @{
        ProjDir = $dst
        Proj    = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pro"
        Sch     = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_sch"
        Pcb     = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb"
    }
}

function Parse-PcbSyncReadinessOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                         = ($Text -match 'status=1')
        pcb_sync_readiness_ok             = (& $parseLine "pcb_sync_readiness_ok")
        pcb_sync_readiness_read_only      = (& $parseLine "pcb_sync_readiness_read_only")
        pcb_sync_likely                   = (& $parseLine "pcb_sync_likely")
        needs_update_from_schematic       = (& $parseLine "needs_update_from_schematic")
        sch_symbols_count                 = (& $parseLine "sch_symbols_count")
        sch_symbols_with_footprint_count  = (& $parseLine "sch_symbols_with_footprint_count")
        pcb_footprints_count              = (& $parseLine "pcb_footprints_count")
        expected_footprints_count         = (& $parseLine "expected_footprints_count")
        matched_footprints_count          = (& $parseLine "matched_footprints_count")
        missing_footprints_count          = (& $parseLine "missing_footprints_count")
        orphan_footprints_count           = (& $parseLine "orphan_footprints_count")
        recommended_next_tool             = (& $parseLine "recommended_next_tool")
        recommended_next_action           = (& $parseLine "recommended_next_action")
        exact_blocker_if_any              = (& $parseLine "exact_blocker_if_any")
    }
}

function Invoke-PcbSyncReadinessCase {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Assert
    )
    Stop-KiCadProcesses
    Start-Sleep -Seconds 5
    $null = Clear-IpcSockets

    $schAbs = (Resolve-Path $Fixture.Sch).Path
    $pcbAbs = (Resolve-Path $Fixture.Pcb).Path
    $projAbs = (Resolve-Path $Fixture.Proj).Path

    $kicadProc = Start-Process -FilePath $kicadExe -ArgumentList $projAbs -WorkingDirectory $RepoRoot -PassThru
    try {
        $resolved = Resolve-IpcUrlFromSockets -SmokeExePath $smokeExe -KiCadPid $kicadProc.Id `
            -MaxIterations 120 -PingTimeoutMs 1500 -SleepBetweenIterationsMs 500 -WorkingDirectory $RepoRoot
        if (-not $resolved.Resolved) { throw "IPC URL unresolved for $CaseLabel" }
        $url = $resolved.Url
        if (-not (Wait-McpProjectLoaded -SmokeExePath $smokeExe -Url $url -WorkingDirectory $RepoRoot)) {
            throw "project_not_loaded for $CaseLabel"
        }
        Start-Sleep -Seconds 5

        foreach ($pair in @(
                @{ type = "sch"; filename = $schAbs; log = "open_sch" }
                @{ type = "pcb"; filename = $pcbAbs; log = "open_pcb" }
            )) {
            $openInput = Join-Path $runDir ("qa_open_editor_frame_" + $CaseLabel + "_" + $pair.log + ".json")
            (@{ type = $pair.type; filename = $pair.filename } | ConvertTo-Json -Compress) | Set-Content $openInput -Encoding UTF8
            $openSm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $openInput)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
            $openSm.Text | Set-Content (Join-Path $runDir ("qa_open_editor_frame_" + $CaseLabel + "_" + $pair.log + ".log")) -Encoding UTF8
            if ($openSm.Text -notmatch 'status=1') { throw "qa_open_editor_frame $($pair.type) failed for $CaseLabel" }
            Start-Sleep -Seconds 3
        }

        $argsPath = Join-Path $runDir ("pcb_sync_readiness_" + $CaseLabel + ".args.json")
        (@{
            board_filename     = $pcbAbs
            schematic_filename = $schAbs
            match_mode         = "reference_or_path"
            include_items      = $true
            max_items          = 50
        } | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8

        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_pcb_sync_readiness_session", ("@" + $argsPath)) -TimeoutMs 120000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_sync_readiness_" + $CaseLabel + ".log")) -Encoding UTF8
        $parsed = Parse-PcbSyncReadinessOutput -Text $sm.Text
        & $Assert $parsed
        return $parsed
    }
    finally {
        if ($kicadProc -and !$kicadProc.HasExited) {
            Stop-Process -Id $kicadProc.Id -Force -ErrorAction SilentlyContinue
        }
        Stop-KiCadProcesses
    }
}

$syncedFx = Sync-PcbSyncFixture -CaseName "synced"
$orphanFx = Sync-PcbSyncFixture -CaseName "orphan"
$emptyFx = Sync-PcbSyncFixture -CaseName "empty"

$emptyParsed = Invoke-PcbSyncReadinessCase -Fixture $emptyFx -CaseLabel "empty" -Assert {
    param($p)
    if ($p.pcb_sync_readiness_ok -ne "true") { throw "empty: pcb_sync_readiness_ok expected true" }
    if ($p.pcb_sync_readiness_read_only -ne "true") { throw "empty: read_only expected true" }
    if ($p.missing_footprints_count -ne "2") { throw "empty: missing_footprints_count expected 2 got $($p.missing_footprints_count)" }
    if ($p.pcb_sync_likely -ne "false") { throw "empty: pcb_sync_likely expected false" }
    if ($p.needs_update_from_schematic -ne "true") { throw "empty: needs_update expected true" }
    if ($p.exact_blocker_if_any -ne "N/A") { throw "empty: blocker expected N/A" }
}

$syncedParsed = Invoke-PcbSyncReadinessCase -Fixture $syncedFx -CaseLabel "synced" -Assert {
    param($p)
    if ($p.pcb_sync_readiness_ok -ne "true") { throw "synced: ok expected true" }
    if ($p.matched_footprints_count -ne "2") { throw "synced: matched expected 2 got $($p.matched_footprints_count)" }
    if ($p.missing_footprints_count -ne "0") { throw "synced: missing expected 0" }
    if ($p.orphan_footprints_count -ne "0") { throw "synced: orphan expected 0" }
    if ($p.pcb_sync_likely -ne "true") { throw "synced: pcb_sync_likely expected true" }
}

$orphanParsed = Invoke-PcbSyncReadinessCase -Fixture $orphanFx -CaseLabel "orphan" -Assert {
    param($p)
    if ($p.pcb_sync_readiness_ok -ne "true") { throw "orphan: ok expected true" }
    if ($p.matched_footprints_count -ne "2") { throw "orphan: matched expected 2 got $($p.matched_footprints_count)" }
    if ($p.orphan_footprints_count -ne "1") { throw "orphan: orphan_footprints_count expected 1 got $($p.orphan_footprints_count)" }
    if ($p.pcb_sync_likely -ne "false") { throw "orphan: pcb_sync_likely expected false" }
    if ($p.recommended_next_action -ne "review_orphans") { throw "orphan: recommended_next_action expected review_orphans" }
}

$summaryLines = @(
    "pcb_sync_readiness_green=true"
    "empty_case_ok=true"
    "synced_case_ok=true"
    "orphan_case_ok=true"
    "missing_footprints_count_ok=true"
    "matched_footprints_count_ok=true"
    "orphan_footprints_count_ok=true"
    "read_only_ok=true"
    "exact_blocker_if_any=N/A"
    "empty_missing_footprints_count=$($emptyParsed.missing_footprints_count)"
    "synced_matched_footprints_count=$($syncedParsed.matched_footprints_count)"
    "orphan_footprints_count=$($orphanParsed.orphan_footprints_count)"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_pcb_sync_readiness_targeted_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
Write-Host ($summaryLines -join "`n")
exit 0
