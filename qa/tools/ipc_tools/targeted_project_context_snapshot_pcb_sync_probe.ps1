param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R172 — get_project_context_snapshot pcb_sync_summary opt-in probe (empty / synced / orphan fixtures).

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-project-context-snapshot-pcb-sync-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library for snapshot pcb sync"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-PcbSyncSnapshotFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseName
    )
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $pcbSrc = Join-Path $RepoRoot ("qa\data\pcbnew\sync\ipc_pcb_sync_v1_" + $CaseName)
    $dst = Join-Path $BuildDir ("ipc-snapshot-pcb-sync-v1-" + $CaseName + "-proj")
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
        Proj = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pro"
        Sch  = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_sch"
        Pcb  = Join-Path $dst "ipc_pcb_sync_v1_empty.kicad_pcb"
    }
}

function Parse-SnapshotPcbSyncOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|0|1|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                                   = ($Text -match 'status=1')
        snapshot_pcb_sync_summary_present           = (& $parseLine "snapshot_pcb_sync_summary_present")
        snapshot_pcb_sync_likely                      = (& $parseLine "snapshot_pcb_sync_likely")
        snapshot_needs_update_from_schematic          = (& $parseLine "snapshot_needs_update_from_schematic")
        snapshot_pcb_sync_missing_footprints_count    = (& $parseLine "snapshot_pcb_sync_missing_footprints_count")
        snapshot_pcb_sync_orphan_footprints_count     = (& $parseLine "snapshot_pcb_sync_orphan_footprints_count")
        snapshot_pcb_sync_preview_available           = (& $parseLine "snapshot_pcb_sync_preview_available")
        snapshot_pcb_update_needed                    = (& $parseLine "snapshot_pcb_update_needed")
        snapshot_pcb_sync_would_add_footprints_count  = (& $parseLine "snapshot_pcb_sync_would_add_footprints_count")
        snapshot_pcb_sync_recommended_next_tool       = (& $parseLine "snapshot_pcb_sync_recommended_next_tool")
        snapshot_pcb_sync_recommended_next_action     = (& $parseLine "snapshot_pcb_sync_recommended_next_action")
        snapshot_pcb_sync_exact_blocker_if_any        = (& $parseLine "snapshot_pcb_sync_exact_blocker_if_any")
    }
}

function Invoke-SnapshotPcbSyncCase {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel,
        [Parameter(Mandatory = $true)]
        [hashtable]$SnapshotArgs,
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

        $payload = @{
            include_nets = $false
        }
        foreach ($key in $SnapshotArgs.Keys) { $payload[$key] = $SnapshotArgs[$key] }

        $argsPath = Join-Path $runDir ("snapshot_pcb_sync_" + $CaseLabel + ".args.json")
        ($payload | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8

        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "get_project_context_snapshot", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("snapshot_pcb_sync_" + $CaseLabel + ".log")) -Encoding UTF8
        $parsed = Parse-SnapshotPcbSyncOutput -Text $sm.Text
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

$emptyFx = Sync-PcbSyncSnapshotFixture -CaseName "empty"
$syncedFx = Sync-PcbSyncSnapshotFixture -CaseName "synced"
$orphanFx = Sync-PcbSyncSnapshotFixture -CaseName "orphan"

$defaultParsed = Invoke-SnapshotPcbSyncCase -Fixture $emptyFx -CaseLabel "default_no_sync" -SnapshotArgs @{} -Assert {
    param($p)
    if ($p.snapshot_pcb_sync_summary_present -ne "0") {
        throw "default: snapshot_pcb_sync_summary_present expected 0 got $($p.snapshot_pcb_sync_summary_present)"
    }
    if ($p.snapshot_pcb_sync_likely -ne "absent") {
        throw "default: snapshot_pcb_sync_likely should be absent"
    }
}

$emptyReadinessParsed = Invoke-SnapshotPcbSyncCase -Fixture $emptyFx -CaseLabel "empty_readiness" -SnapshotArgs @{
    include_pcb_sync_summary = $true
    pcb_sync_include_preview = $false
} -Assert {
    param($p)
    if ($p.snapshot_pcb_sync_summary_present -ne "1") { throw "empty_readiness: summary_present expected 1" }
    if ($p.snapshot_pcb_sync_missing_footprints_count -ne "2") {
        throw "empty_readiness: missing expected 2 got $($p.snapshot_pcb_sync_missing_footprints_count)"
    }
    if ($p.snapshot_needs_update_from_schematic -ne "true") {
        throw "empty_readiness: needs_update expected true"
    }
    if ($p.snapshot_pcb_sync_preview_available -ne "false") {
        throw "empty_readiness: preview_available expected false"
    }
    if ($p.snapshot_pcb_update_needed -ne "null") {
        throw "empty_readiness: pcb_update_needed expected null got $($p.snapshot_pcb_update_needed)"
    }
    if ($p.snapshot_pcb_sync_exact_blocker_if_any -ne "N/A") {
        throw "empty_readiness: blocker expected N/A"
    }
}

$emptyPreviewParsed = Invoke-SnapshotPcbSyncCase -Fixture $emptyFx -CaseLabel "empty_preview" -SnapshotArgs @{
    include_pcb_sync_summary = $true
    pcb_sync_include_preview = $true
} -Assert {
    param($p)
    if ($p.snapshot_pcb_sync_summary_present -ne "1") { throw "empty_preview: summary_present expected 1" }
    if ($p.snapshot_pcb_sync_preview_available -ne "true") {
        throw "empty_preview: preview_available expected true got $($p.snapshot_pcb_sync_preview_available)"
    }
    if ([int]$p.snapshot_pcb_sync_would_add_footprints_count -lt 2) {
        throw "empty_preview: would_add expected >= 2 got $($p.snapshot_pcb_sync_would_add_footprints_count)"
    }
    if ($p.snapshot_pcb_update_needed -ne "true") {
        throw "empty_preview: pcb_update_needed expected true"
    }
}

$syncedParsed = Invoke-SnapshotPcbSyncCase -Fixture $syncedFx -CaseLabel "synced" -SnapshotArgs @{
    include_pcb_sync_summary = $true
    pcb_sync_include_preview = $true
} -Assert {
    param($p)
    if ($p.snapshot_pcb_sync_likely -ne "true") {
        throw "synced: pcb_sync_likely expected true got $($p.snapshot_pcb_sync_likely)"
    }
    if ($p.snapshot_needs_update_from_schematic -ne "false") {
        throw "synced: needs_update expected false got $($p.snapshot_needs_update_from_schematic)"
    }
    if ($p.snapshot_pcb_update_needed -ne "false") {
        throw "synced: pcb_update_needed expected false got $($p.snapshot_pcb_update_needed)"
    }
}

$orphanParsed = Invoke-SnapshotPcbSyncCase -Fixture $orphanFx -CaseLabel "orphan" -SnapshotArgs @{
    include_pcb_sync_summary = $true
    pcb_sync_include_preview = $false
} -Assert {
    param($p)
    if ([int]$p.snapshot_pcb_sync_orphan_footprints_count -ne 1) {
        throw "orphan: orphan_footprints_count expected 1 got $($p.snapshot_pcb_sync_orphan_footprints_count)"
    }
    if ($p.snapshot_pcb_sync_recommended_next_action -ne "review_orphans") {
        throw "orphan: recommended_next_action expected review_orphans got $($p.snapshot_pcb_sync_recommended_next_action)"
    }
}

$summaryLines = @(
    "snapshot_pcb_sync_green=true"
    "default_no_sync_ok=true"
    "empty_readiness_only_ok=true"
    "empty_preview_ok=true"
    "synced_ok=true"
    "orphan_ok=true"
    "exact_blocker_if_any=N/A"
    "empty_missing_footprints_count=$($emptyReadinessParsed.snapshot_pcb_sync_missing_footprints_count)"
    "empty_preview_would_add_footprints_count=$($emptyPreviewParsed.snapshot_pcb_sync_would_add_footprints_count)"
    "orphan_footprints_count=$($orphanParsed.snapshot_pcb_sync_orphan_footprints_count)"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_project_context_snapshot_pcb_sync_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
Write-Host ($summaryLines -join "`n")
exit 0
