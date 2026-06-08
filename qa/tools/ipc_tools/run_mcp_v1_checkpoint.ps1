param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [ValidateSet("smoke", "core", "full")]
    [string]$Profile = "full",
    [switch]$IncludeLibrary
)

if( $IncludeLibrary ) { $Profile = "full" }

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-mcp-v1-checkpoint")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Get-PrefixedFamilyBlocker {
    param(
        [Parameter(Mandatory = $true)]
        $Result,
        [Parameter(Mandatory = $true)]
        [string]$Prefix
    )
    if( $null -eq $Result ) { return $null }
    $ec = $Result.ExitCode
    if( $ec -is [string] -and $ec -eq "skipped" ) { return $null }
    if( $ec -ne 0 ) {
        return "${Prefix}:runner_exit_code_$ec"
    }
    if( $Result.ExactBlocker -ne "N/A" ) {
        return "${Prefix}:$($Result.ExactBlocker)"
    }
    return $null
}

function Get-SmokeCheckpointBlocker {
    param($Simulation, $SchActions)
    $b = Get-PrefixedFamilyBlocker $Simulation "simulation_v1_1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $SchActions "sch_actions_v1"
    if( $null -ne $b ) { return $b }
    return "N/A"
}

function Get-CoreCheckpointBlocker {
    param($Simulation, $SchConnectivity, $SchActions)
    $b = Get-PrefixedFamilyBlocker $Simulation "simulation_v1_1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $SchConnectivity "sch_connectivity_v1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $SchActions "sch_actions_v1"
    if( $null -ne $b ) { return $b }
    return "N/A"
}

function Get-FullCheckpointBlocker {
    param($Simulation, $Library, $SchConnectivity, $SchActions)
    $b = Get-PrefixedFamilyBlocker $Simulation "simulation_v1_1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $Library "library_v1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $SchConnectivity "sch_connectivity_v1"
    if( $null -ne $b ) { return $b }
    $b = Get-PrefixedFamilyBlocker $SchActions "sch_actions_v1"
    if( $null -ne $b ) { return $b }
    return "N/A"
}

function Invoke-FamilyRunner {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RunnerScriptName,
        [Parameter(Mandatory = $true)]
        [string]$SummaryName
    )

    $runnerPath = Join-Path $ScriptDir $RunnerScriptName

    if( !(Test-Path -LiteralPath $runnerPath) )
    {
        return [pscustomobject]@{
            Runner = $RunnerScriptName
            ExitCode = -1
            TargetRunDir = "absent"
            Ok = $false
            ExactBlocker = "runner_script_missing"
        }
    }

    # R112 — spawn via Invoke-IpcQaChildScript ; green/summary/blockers inchangés.
    $famSw = [System.Diagnostics.Stopwatch]::StartNew()

    $child = Invoke-IpcQaChildScript `
        -ScriptPath $runnerPath `
        -BuildDir $BuildDir `
        -LogDir $runDir `
        -LogBaseName $RunnerScriptName `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $SummaryName

    $famSw.Stop()

    $targetRunDir = $child.TargetRunDir

    $exactBlocker = "N/A"
    $ok = $false

    if( $targetRunDir -eq "absent" )
    {
        $exactBlocker = "runner_target_run_dir_missing"
    }
    else
    {
        $childSummaryPath = $child.SummaryPath
        if( !(Test-Path -LiteralPath $childSummaryPath) )
        {
            $exactBlocker = "summary_missing"
        }
        else
        {
            Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir $SummaryName) -Force
            $summaryContent = Get-Content -LiteralPath $childSummaryPath
            $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1
            if( -not [string]::IsNullOrWhiteSpace( $blockerLine ) )
            {
                $exactBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
            }
            else
            {
                $exactBlocker = "exact_blocker_field_missing"
            }

            if( $exactBlocker -eq "N/A" -and $child.ExitCode -eq 0 )
            {
                $ok = $true
            }
        }
    }

    if( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" )
    {
        $exactBlocker = "runner_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        Runner = $RunnerScriptName
        ExitCode = $child.ExitCode
        TargetRunDir = $targetRunDir
        Ok = $ok
        ExactBlocker = $exactBlocker
    }
}

$simulation = Invoke-FamilyRunner -RunnerScriptName "run_simulation_v1_1_targeted_probes.ps1" -SummaryName "qa_simulation_v1_1_family_summary.txt"

if( $Profile -eq "full" )
{
    $library = Invoke-FamilyRunner -RunnerScriptName "run_library_v1_targeted_probes.ps1" -SummaryName "qa_library_v1_family_summary.txt"
}
else
{
    $library = [pscustomobject]@{
        Runner = "run_library_v1_targeted_probes.ps1"
        ExitCode = "skipped"
        TargetRunDir = "skipped"
        Ok = $false
        ExactBlocker = "skipped"
    }
}

if( $Profile -eq "core" -or $Profile -eq "full" )
{
    $schConnectivity = Invoke-FamilyRunner -RunnerScriptName "run_sch_connectivity_v1_targeted_probes.ps1" -SummaryName "qa_sch_connectivity_v1_family_summary.txt"
}
else
{
    $schConnectivity = [pscustomobject]@{
        Runner = "run_sch_connectivity_v1_targeted_probes.ps1"
        ExitCode = "skipped"
        TargetRunDir = "skipped"
        Ok = $false
        ExactBlocker = "skipped"
    }
}

$schActions = Invoke-FamilyRunner -RunnerScriptName "run_sch_actions_v1_targeted_probes.ps1" -SummaryName "qa_sch_actions_v1_family_summary.txt"

$simulationOk = if( $simulation.Ok ) { "true" } else { "false" }
$libraryOk = if( $Profile -eq "full" ) { $(if( $library.Ok ) { "true" } else { "false" }) } else { "skipped" }
$schConnectivityOk = if( $Profile -eq "smoke" ) { "skipped" } elseif( $schConnectivity.Ok ) { "true" } else { "false" }
$schActionsOk = if( $schActions.Ok ) { "true" } else { "false" }

$libraryIncluded = ( $Profile -eq "full" )
$schConnectivityIncluded = ( $Profile -eq "core" -or $Profile -eq "full" )

$smokeCheckpointGreen = ( $simulation.Ok -and $schActions.Ok )
$coreCheckpointGreen = ( $Profile -ne "smoke" ) -and $simulation.Ok -and $schConnectivity.Ok -and $schActions.Ok
$fullCheckpointGreen = ( $Profile -eq "full" ) -and $simulation.Ok -and $library.Ok -and $schConnectivity.Ok -and $schActions.Ok

$mcpV1SmokeCheckpointGreen = if( $smokeCheckpointGreen ) { "true" } else { "false" }
$mcpV1CoreCheckpointGreen = if( $Profile -eq "smoke" ) {
    "skipped"
} elseif( $coreCheckpointGreen ) {
    "true"
} else {
    "false"
}
$mcpV1FullCheckpointGreen = if( $Profile -eq "smoke" -or $Profile -eq "core" ) {
    "skipped"
} elseif( $fullCheckpointGreen ) {
    "true"
} else {
    "false"
}

$mcpV1CheckpointGreen = switch( $Profile )
{
    "smoke" { $mcpV1SmokeCheckpointGreen }
    "core" { $mcpV1CoreCheckpointGreen }
    "full" { $mcpV1FullCheckpointGreen }
}

$exactBlockerSmokeIfAny = Get-SmokeCheckpointBlocker $simulation $schActions

$exactBlockerCoreIfAny = if( $Profile -eq "smoke" ) {
    "skipped"
} else {
    Get-CoreCheckpointBlocker $simulation $schConnectivity $schActions
}

$exactBlockerLibraryIfAny = if( $Profile -eq "smoke" -or $Profile -eq "core" ) {
    "skipped"
} else {
    $lb = Get-PrefixedFamilyBlocker $library "library_v1"
    if( $null -eq $lb ) { "N/A" } else { $lb }
}

$exactBlockerIfAny = switch( $Profile )
{
    "smoke" { $exactBlockerSmokeIfAny }
    "core" { Get-CoreCheckpointBlocker $simulation $schConnectivity $schActions }
    "full" { Get-FullCheckpointBlocker $simulation $library $schConnectivity $schActions }
}

@(
    "mcp_v1_checkpoint_profile=$Profile"
    "mcp_v1_smoke_checkpoint_green=$mcpV1SmokeCheckpointGreen"
    "mcp_v1_core_checkpoint_green=$mcpV1CoreCheckpointGreen"
    "mcp_v1_full_checkpoint_green=$mcpV1FullCheckpointGreen"
    "library_v1_included=$(if($libraryIncluded){'true'}else{'false'})"
    "sch_connectivity_v1_included=$(if($schConnectivityIncluded){'true'}else{'false'})"
    "simulation_v1_1_ok=$simulationOk"
    "library_v1_ok=$libraryOk"
    "sch_connectivity_v1_ok=$schConnectivityOk"
    "sch_actions_v1_ok=$schActionsOk"
    "exact_blocker_smoke_if_any=$exactBlockerSmokeIfAny"
    "exact_blocker_core_if_any=$exactBlockerCoreIfAny"
    "exact_blocker_library_if_any=$exactBlockerLibraryIfAny"
    "exact_blocker_if_any=$exactBlockerIfAny"
    "simulation_v1_1_target_run_dir=" + $simulation.TargetRunDir
    "library_v1_target_run_dir=" + $library.TargetRunDir
    "sch_connectivity_v1_target_run_dir=" + $schConnectivity.TargetRunDir
    "sch_actions_v1_target_run_dir=" + $schActions.TargetRunDir
    "simulation_v1_1_exit_code=" + $simulation.ExitCode
    "library_v1_exit_code=" + $library.ExitCode
    "sch_connectivity_v1_exit_code=" + $schConnectivity.ExitCode
    "sch_actions_v1_exit_code=" + $schActions.ExitCode
    "mcp_v1_checkpoint_green=$mcpV1CheckpointGreen"
    ""
) | Set-Content -Path (Join-Path $runDir "qa_mcp_v1_checkpoint_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
