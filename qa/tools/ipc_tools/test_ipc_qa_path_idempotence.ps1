param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [int]$InitRepeatCount = 10
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if( $InitRepeatCount -lt 2 )
{
    throw "InitRepeatCount must be at least 2 (first init + repeated calls)"
}

$BuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
$RepoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$commonModule = Join-Path $PSScriptRoot "ipc_qa_common.ps1"
if( !(Test-Path -LiteralPath $commonModule) )
{
    throw "Missing common IPC QA module at $commonModule"
}
. $commonModule

$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-ipc-path-idempotence")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$requiredRelativePaths = @(
    "kicad",
    "eeschema",
    "pcbnew",
    "common",
    "common\gal",
    "api",
    "vcpkg_installed\x64-windows\bin",
    "vcpkg_installed\x64-windows\tools\python3"
)

function Test-RequiredIpcQaPathPrefixesPresent {
    param(
        [string]$ResolvedBuildDir,
        [string[]]$RelativePaths
    )

    $pathKeys = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach( $segment in ( $env:PATH -split ';' ) )
    {
        $key = Get-IpcQaNormalizedPathKey -PathSegment $segment
        if( -not [string]::IsNullOrWhiteSpace( $key ) )
        {
            [void]$pathKeys.Add( $key )
        }
    }

    [string[]]$missing = @()
    foreach( $rel in $RelativePaths )
    {
        $full = Join-Path $ResolvedBuildDir $rel
        $key = Get-IpcQaNormalizedPathKey -PathSegment $full
        if( [string]::IsNullOrWhiteSpace( $key ) -or -not $pathKeys.Contains( $key ) )
        {
            $missing += $full
        }
    }

    return $missing
}

$pathLenBefore = $env:PATH.Length
$pathLengthLog = New-Object System.Collections.Generic.List[string]
$pathLengthLog.Add( "iteration=0 phase=before_init path_len=$pathLenBefore" ) | Out-Null

$null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation
$pathLenAfterFirstInit = $env:PATH.Length
$pathLengthLog.Add( "iteration=1 phase=after_first_init path_len=$pathLenAfterFirstInit" ) | Out-Null

$pathGrowthDetected = $false
$maxLenAfterStabilization = $pathLenAfterFirstInit

for( $i = 2; $i -le $InitRepeatCount; $i++ )
{
    $null = Initialize-IpcQaEnvironment -BuildDir $BuildDir -RepoRoot $RepoRoot -ConfigRoot $cfgRoot -EnableAutomation
    $currentLen = $env:PATH.Length
    $pathLengthLog.Add( "iteration=$i phase=repeated_init path_len=$currentLen" ) | Out-Null

    if( $currentLen -gt $maxLenAfterStabilization )
    {
        $maxLenAfterStabilization = $currentLen
    }

    if( $currentLen -gt $pathLenAfterFirstInit )
    {
        $pathGrowthDetected = $true
    }
}

$pathLenAfterRepeatedInit = $env:PATH.Length
$pathDeltaAfterRepeatedInit = $pathLenAfterRepeatedInit - $pathLenAfterFirstInit

$missingPrefixes = Test-RequiredIpcQaPathPrefixesPresent -ResolvedBuildDir $BuildDir -RelativePaths $requiredRelativePaths
if( $null -eq $missingPrefixes )
{
    $missingPrefixes = @()
}
$requiredPrefixesPresent = if( $missingPrefixes.Count -eq 0 ) { "true" } else { "false" }
$missingRequiredPrefixes = if( $missingPrefixes.Count -eq 0 ) { "none" } else { ( $missingPrefixes -join ";" ) }

$kifaceResult = Test-IpcKifaceDllLoad -BuildDir $BuildDir -ArtifactsDir $runDir
$kifaceSmokeOk = if( $kifaceResult.AllOk ) { "true" } else { "false" }

$blocker = "N/A"
if( $pathGrowthDetected )
{
    $blocker = "path_grew_after_first_init:delta=$pathDeltaAfterRepeatedInit"
}
elseif( $requiredPrefixesPresent -ne "true" )
{
    $blocker = "missing_required_prefixes:$missingRequiredPrefixes"
}
elseif( $kifaceSmokeOk -ne "true" )
{
    $blocker = "kiface_smoke_failed"
}

$ipcPathIdempotenceGreen = if( $blocker -eq "N/A" ) { "true" } else { "false" }

$pathLengthLog | Set-Content -Path (Join-Path $runDir "ipc_path_length_trace.txt") -Encoding UTF8

@(
    "ipc_path_idempotence_green=$ipcPathIdempotenceGreen"
    "path_len_before=$pathLenBefore"
    "path_len_after_first_init=$pathLenAfterFirstInit"
    "path_len_after_repeated_init=$pathLenAfterRepeatedInit"
    "path_delta_after_repeated_init=$pathDeltaAfterRepeatedInit"
    "init_repeat_count=$InitRepeatCount"
    "required_prefixes_present=$requiredPrefixesPresent"
    "missing_required_prefixes=$missingRequiredPrefixes"
    "kiface_smoke_ok=$kifaceSmokeOk"
    "exact_blocker_if_any=$blocker"
    "target_run_dir=$runDir"
    ""
) | Set-Content -Path (Join-Path $runDir "qa_ipc_path_idempotence_summary.txt") -Encoding UTF8

Write-TargetRunDir -RunDir $runDir

if( $ipcPathIdempotenceGreen -ne "true" )
{
    throw "ipc_path_idempotence_green=false exact_blocker_if_any=$blocker"
}
