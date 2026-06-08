param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

# R171 — pcb_update_from_schematic_preview_session V1 dry-run probe (empty / synced / orphan fixtures).

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

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-update-preview-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null
New-Item -ItemType Directory -Force -Path $cfg10 | Out-Null
$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation

$qaResistorPrettyUnix = $qaResistorPretty.Replace('\', '/')
$globalFpLibTable = Join-Path $cfg10 "fp-lib-table"
@(
    "(fp_lib_table",
    "  (version 7)",
    ('  (lib (name "Resistor_SMD")(type "KiCad")(uri "' + $qaResistorPrettyUnix + '")(options "")(descr "QA resistor library for pcb update preview"))'),
    ")"
) | Set-Content -Path $globalFpLibTable -Encoding ASCII

function Sync-PcbPreviewFixture {
    param(
        [Parameter(Mandatory = $true)]
        [string]$CaseName
    )
    $baseSrc = Join-Path $RepoRoot "qa\data\pcbnew\sync\ipc_pcb_sync_v1_empty"
    $pcbSrc = Join-Path $RepoRoot ("qa\data\pcbnew\sync\ipc_pcb_sync_v1_" + $CaseName)
    $dst = Join-Path $BuildDir ("ipc-pcb-preview-v1-" + $CaseName + "-proj")
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

function Parse-PcbUpdatePreviewOutput {
    param([string]$Text)
    $parseLine = {
        param($Key)
        if ($Text -match ($Key + '=(true|false|null|absent|[0-9]+|[^\r\n]+)')) { return $Matches[1].Trim() }
        return "absent"
    }
    return @{
        status_ok                              = ($Text -match 'status=1')
        pcb_update_preview_ok                  = (& $parseLine "pcb_update_preview_ok")
        dry_run                                = (& $parseLine "dry_run")
        read_only                              = (& $parseLine "read_only")
        pcb_update_needed                      = (& $parseLine "pcb_update_needed")
        would_add_footprints_count             = (& $parseLine "would_add_footprints_count")
        would_remove_orphan_footprints_count   = (& $parseLine "would_remove_orphan_footprints_count")
        would_update_nets_count                = (& $parseLine "would_update_nets_count")
        would_replace_footprints_count         = (& $parseLine "would_replace_footprints_count")
        actions_count                          = (& $parseLine "actions_count")
        returned_actions_count                 = (& $parseLine "returned_actions_count")
        board_mutation_detected                = (& $parseLine "board_mutation_detected")
        recommended_next_action                = (& $parseLine "recommended_next_action")
        exact_blocker_if_any                   = (& $parseLine "exact_blocker_if_any")
    }
}

function Invoke-PcbUpdatePreviewCase {
    param(
        [Parameter(Mandatory = $true)]
        [hashtable]$Fixture,
        [Parameter(Mandatory = $true)]
        [string]$CaseLabel,
        [Parameter(Mandatory = $true)]
        [hashtable]$ToolArgs,
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

        $argsPath = Join-Path $runDir ("pcb_update_preview_" + $CaseLabel + ".args.json")
        $payload = @{
            board_filename     = $pcbAbs
            schematic_filename = $schAbs
            match_mode         = "reference_or_path"
            include_actions    = $true
            max_actions        = 100
        }
        foreach ($key in $ToolArgs.Keys) { $payload[$key] = $ToolArgs[$key] }
        ($payload | ConvertTo-Json -Compress) | Set-Content $argsPath -Encoding UTF8

        $sm = Invoke-SmokeBoundedIpcRetry -SmokeExePath $smokeExe -SmokeArgs @($url, "mcp_call_tool_args", "pcb_update_from_schematic_preview_session", ("@" + $argsPath)) -TimeoutMs 180000 -WorkingDirectory $RepoRoot -MaxAttempts 3 -RetryDelayMs 400
        $sm.Text | Set-Content (Join-Path $runDir ("pcb_update_preview_" + $CaseLabel + ".log")) -Encoding UTF8
        $parsed = Parse-PcbUpdatePreviewOutput -Text $sm.Text
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

$syncedFx = Sync-PcbPreviewFixture -CaseName "synced"
$orphanFx = Sync-PcbPreviewFixture -CaseName "orphan"
$emptyFx = Sync-PcbPreviewFixture -CaseName "empty"

$emptyParsed = Invoke-PcbUpdatePreviewCase -Fixture $emptyFx -CaseLabel "empty" -ToolArgs @{
    delete_orphans = $false
    replace_footprints = $true
} -Assert {
    param($p)
    if ($p.pcb_update_preview_ok -ne "true") { throw "empty: pcb_update_preview_ok expected true" }
    if ($p.dry_run -ne "true") { throw "empty: dry_run expected true" }
    if ($p.read_only -ne "true") { throw "empty: read_only expected true" }
    if ([int]$p.would_add_footprints_count -lt 2) { throw "empty: would_add_footprints_count expected >= 2 got $($p.would_add_footprints_count)" }
    if ($p.pcb_update_needed -ne "true") { throw "empty: pcb_update_needed expected true" }
    if ($p.board_mutation_detected -ne "false") { throw "empty: board_mutation_detected expected false" }
    if ($p.exact_blocker_if_any -ne "N/A") { throw "empty: blocker expected N/A" }
    if ([int]$p.actions_count -lt 2) { throw "empty: actions_count expected >= 2 got $($p.actions_count)" }
}

$syncedParsed = Invoke-PcbUpdatePreviewCase -Fixture $syncedFx -CaseLabel "synced" -ToolArgs @{
    delete_orphans = $false
    replace_footprints = $true
} -Assert {
    param($p)
    if ($p.pcb_update_preview_ok -ne "true") { throw "synced: ok expected true" }
    if ([int]$p.would_add_footprints_count -ne 0) { throw "synced: would_add_footprints_count expected 0 got $($p.would_add_footprints_count)" }
    if ([int]$p.would_remove_orphan_footprints_count -ne 0) { throw "synced: would_remove_orphan expected 0" }
    if ([int]$p.would_replace_footprints_count -ne 0) { throw "synced: would_replace expected 0" }
    if ($p.pcb_update_needed -ne "false") { throw "synced: pcb_update_needed expected false got $($p.pcb_update_needed)" }
    if ($p.board_mutation_detected -ne "false") { throw "synced: board_mutation_detected expected false" }
}

$orphanParsed = Invoke-PcbUpdatePreviewCase -Fixture $orphanFx -CaseLabel "orphan" -ToolArgs @{
    delete_orphans = $true
    replace_footprints = $true
} -Assert {
    param($p)
    if ($p.pcb_update_preview_ok -ne "true") { throw "orphan: ok expected true" }
    if ([int]$p.would_remove_orphan_footprints_count -lt 1) {
        throw "orphan: would_remove_orphan_footprints_count expected >= 1 got $($p.would_remove_orphan_footprints_count)"
    }
    if ($p.recommended_next_action -ne "review_orphans") {
        throw "orphan: recommended_next_action expected review_orphans got $($p.recommended_next_action)"
    }
    if ($p.board_mutation_detected -ne "false") { throw "orphan: board_mutation_detected expected false" }
}

$dryRunNoMutationOk = ($emptyParsed.board_mutation_detected -eq "false") -and `
    ($syncedParsed.board_mutation_detected -eq "false") -and `
    ($orphanParsed.board_mutation_detected -eq "false")

$actionsStructuredOk = ([int]$emptyParsed.returned_actions_count -ge 2) -and `
    ([int]$emptyParsed.actions_count -ge 2)

$summaryLines = @(
    "pcb_update_preview_green=true"
    "empty_preview_ok=true"
    "empty_would_add_footprints_ok=true"
    "synced_preview_ok=true"
    "synced_no_update_needed_ok=true"
    "orphan_preview_ok=true"
    "dry_run_no_board_mutation_ok=$($dryRunNoMutationOk.ToString().ToLower())"
    "actions_structured_ok=$($actionsStructuredOk.ToString().ToLower())"
    "exact_blocker_if_any=N/A"
    "empty_would_add_footprints_count=$($emptyParsed.would_add_footprints_count)"
    "synced_actions_count=$($syncedParsed.actions_count)"
    "orphan_would_remove_orphan_footprints_count=$($orphanParsed.would_remove_orphan_footprints_count)"
    "target_run_dir=$runDir"
)
$summaryPath = Join-Path $runDir "qa_pcb_update_preview_targeted_summary.txt"
$summaryLines | Set-Content -Path $summaryPath -Encoding UTF8
Write-Host ($summaryLines -join "`n")
exit 0
