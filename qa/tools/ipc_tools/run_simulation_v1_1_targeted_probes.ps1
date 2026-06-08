param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $false)]
    [string]$ExistingRunDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

$artifactsRoot = Join-Path $BuildDir "ipc-v1-artifacts"
$runDir = Join-Path $artifactsRoot ((Get-Date -Format "yyyyMMdd-HHmmssfff") + "-simulation-v1-1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$contracts = @(
    [pscustomobject]@{ Label = "directive";            OutKey = "directive_ok";             File = "qa_simulation_directive_inspect_contract_summary.txt" },
    [pscustomobject]@{ Label = "netlist_preview";      OutKey = "netlist_preview_ok";       File = "qa_simulation_netlist_preview_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "model_library";        OutKey = "model_library_ok";         File = "qa_simulation_model_library_inspect_contract_summary.txt" },
    [pscustomobject]@{ Label = "preflight";            OutKey = "preflight_ok";             File = "qa_simulation_preflight_readiness_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "run_status";           OutKey = "run_status_ok";            File = "qa_simulation_run_status_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "run_minimal_summary";  OutKey = "run_minimal_summary_ok";   File = "qa_simulation_run_minimal_summary_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "run_vector_catalog";   OutKey = "run_vector_catalog_ok";    File = "qa_simulation_run_vector_catalog_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "vector_scalar_probe";  OutKey = "vector_scalar_probe_ok";   File = "qa_simulation_vector_scalar_probe_session_contract_summary.txt" },
    [pscustomobject]@{ Label = "xaxis_probe";          OutKey = "xaxis_probe_ok";           File = "qa_simulation_xaxis_probe_session_contract_summary.txt" }
)

function Read-ContractValue {
    param(
        [Parameter(Mandatory = $true)] [string]$SummaryPath,
        [Parameter(Mandatory = $true)] [string]$Key,
        [string]$DefaultValue = "absent"
    )

    if( !(Test-Path -LiteralPath $SummaryPath) )
    {
        return $DefaultValue
    }

    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if( [string]::IsNullOrWhiteSpace( $line ) )
    {
        return $DefaultValue
    }

    return ( $line -replace ('^' + [regex]::Escape( $Key ) + '='), '' ).Trim()
}

function Resolve-SourceRunDir {
    param(
        [string]$MaybeExistingRunDir
    )

    if( -not [string]::IsNullOrWhiteSpace( $MaybeExistingRunDir ) )
    {
        $resolved = (Resolve-Path -LiteralPath $MaybeExistingRunDir).Path
        return $resolved
    }

    return $null
}

function Resolve-LatestArtifactPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArtifactFileName
    )

    if( !(Test-Path -LiteralPath $artifactsRoot) )
    {
        return $null
    }

    $matches = Get-ChildItem -LiteralPath $artifactsRoot -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -eq $ArtifactFileName } |
        Sort-Object LastWriteTime -Descending

    if( $null -eq $matches -or $matches.Count -eq 0 )
    {
        return $null
    }

    return $matches[0].FullName
}

$sourceRunDir = Resolve-SourceRunDir -MaybeExistingRunDir $ExistingRunDir
$resolvedResults = @()
$firstBlocker = "N/A"

foreach( $def in $contracts )
{
    $sourcePath = $null
    if( $null -ne $sourceRunDir )
    {
        # Strict mode: caller provided a single source run dir.
        $sourcePath = Join-Path $sourceRunDir $def.File
    }
    else
    {
        # Multi-runs mode: resolve latest canonical artifact independently per tool.
        $sourcePath = Resolve-LatestArtifactPath -ArtifactFileName $def.File
    }

    $copiedPath = Join-Path $runDir $def.File

    $ok = "false"
    $blocker = "missing_artifact"

    if( -not [string]::IsNullOrWhiteSpace( $sourcePath ) -and (Test-Path -LiteralPath $sourcePath) )
    {
        Copy-Item -LiteralPath $sourcePath -Destination $copiedPath -Force
        $blocker = Read-ContractValue -SummaryPath $sourcePath -Key "exact_blocker_if_any" -DefaultValue "exact_blocker_missing"
        $ok = if( $blocker -eq "N/A" ) { "true" } else { "false" }
    }

    if( $firstBlocker -eq "N/A" -and $ok -ne "true" )
    {
        $firstBlocker = $def.Label + ":" + $blocker
    }

    $resolvedResults += [pscustomobject]@{
        Label = $def.Label
        OutKey = $def.OutKey
        File = $def.File
        Ok = $ok
        Blocker = $blocker
        SourcePath = $sourcePath
        CopiedPath = $copiedPath
    }
}

$familyGreen = $true
foreach( $r in $resolvedResults )
{
    if( $r.Ok -ne "true" )
    {
        $familyGreen = $false
        break
    }
}

$summaryLines = @()
$summaryLines += "simulation_v1_1_family_green=" + $(if($familyGreen){"true"}else{"false"})

foreach( $def in $contracts )
{
    $item = $resolvedResults | Where-Object { $_.Label -eq $def.Label } | Select-Object -First 1
    $summaryLines += ($def.OutKey + "=" + $item.Ok)
}

$summaryLines += "exact_blocker_if_any=$firstBlocker"
$summaryLines += "source_run_dir=" + $(if($null -eq $sourceRunDir){"multi_runs"}else{$sourceRunDir})

foreach( $def in $contracts )
{
    $item = $resolvedResults | Where-Object { $_.Label -eq $def.Label } | Select-Object -First 1
    $summaryLines += ($def.Label + "_artifact_path=" + $item.SourcePath)
}

$summaryLines += ""
$summaryLines | Set-Content -Path (Join-Path $runDir "qa_simulation_v1_1_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
