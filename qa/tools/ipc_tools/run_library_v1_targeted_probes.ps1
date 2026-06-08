param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-library-v1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

function Invoke-TargetedProbe {
    # R90 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/gating inchangés.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProbeScriptName,
        [Parameter(Mandatory = $true)]
        [string]$SummaryName,
        [Parameter(Mandatory = $true)]
        [string]$ResultKey
    )

    $probePath = Join-Path $ScriptDir $ProbeScriptName

    if( !(Test-Path -LiteralPath $probePath) )
    {
        return [pscustomobject]@{
            Name = $ResultKey
            ProbeScript = $ProbeScriptName
            ExitCode = -1
            TargetRunDir = "absent"
            SummaryPath = "absent"
            SummaryPresent = $false
            Ok = $false
            ExactBlocker = "probe_script_missing"
        }
    }

    $child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName $ProbeScriptName `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $SummaryName

    $targetRunDir = $child.TargetRunDir
    $summaryPath = if( $targetRunDir -eq "absent" ) { "absent" } else { $child.SummaryPath }
    $summaryPresent = $false
    $exactBlocker = "summary_missing"
    $ok = $false

    if( $targetRunDir -ne "absent" -and -not [string]::IsNullOrWhiteSpace( $child.SummaryPath ) )
    {
        $candidateSummary = $child.SummaryPath
        if( Test-Path -LiteralPath $candidateSummary )
        {
            $summaryPresent = $true
            $summaryContent = Get-Content -LiteralPath $candidateSummary
            $okLine = $summaryContent | Where-Object { $_ -like ($ResultKey + "=*") } | Select-Object -First 1
            if( -not [string]::IsNullOrWhiteSpace( $okLine ) )
            {
                $okValue = ($okLine -replace ('^' + [regex]::Escape( $ResultKey ) + '='), '').Trim()
                $ok = ($okValue -eq "true")
            }

            $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1
            if( -not [string]::IsNullOrWhiteSpace( $blockerLine ) )
            {
                $exactBlocker = ($blockerLine -replace '^exact_blocker_if_any=', '').Trim()
            }
            else
            {
                $exactBlocker = "exact_blocker_field_missing"
            }

            Copy-Item -LiteralPath $candidateSummary -Destination (Join-Path $runDir $SummaryName) -Force
        }
    }

    if( $child.ExitCode -ne 0 -and $exactBlocker -eq "summary_missing" )
    {
        $exactBlocker = "probe_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        Name = $ResultKey
        ProbeScript = $ProbeScriptName
        ExitCode = $child.ExitCode
        TargetRunDir = $targetRunDir
        SummaryPath = $summaryPath
        SummaryPresent = $summaryPresent
        Ok = $ok
        ExactBlocker = $exactBlocker
    }
}

function Get-SummaryField {
    param(
        [string]$SummaryPath,
        [string]$FieldName
    )
    if( $SummaryPath -eq "absent" -or !(Test-Path -LiteralPath $SummaryPath) )
    {
        return "absent"
    }
    $line = Get-Content -LiteralPath $SummaryPath | Where-Object { $_ -like ($FieldName + "=*") } | Select-Object -First 1
    if( [string]::IsNullOrWhiteSpace( $line ) )
    {
        return "absent"
    }
    return ( $line -replace ('^' + [regex]::Escape( $FieldName ) + '='), '' ).Trim()
}

$results = @()
$results += Invoke-TargetedProbe -ProbeScriptName "targeted_library_symbol_search_probe.ps1" -SummaryName "qa_library_symbol_search_session_contract_summary.txt" -ResultKey "search_ok"
$results += Invoke-TargetedProbe -ProbeScriptName "targeted_library_symbol_inspect_probe.ps1" -SummaryName "qa_library_symbol_inspect_session_contract_summary.txt" -ResultKey "inspect_ok"
$results += Invoke-TargetedProbe -ProbeScriptName "targeted_library_footprint_search_probe.ps1" -SummaryName "qa_library_footprint_search_session_contract_summary.txt" -ResultKey "search_ok"
$results += Invoke-TargetedProbe -ProbeScriptName "targeted_library_footprint_inspect_probe.ps1" -SummaryName "qa_library_footprint_inspect_session_contract_summary.txt" -ResultKey "inspect_ok"

$symbolSearch = $results[0]
$symbolInspect = $results[1]
$footprintSearch = $results[2]
$footprintInspect = $results[3]

$symbolSearchOk = if( $symbolSearch.Ok -and $symbolSearch.ExitCode -eq 0 ) { "true" } else { "false" }
$symbolInspectOk = if( $symbolInspect.Ok -and $symbolInspect.ExitCode -eq 0 ) { "true" } else { "false" }
$footprintSearchOk = if( $footprintSearch.Ok -and $footprintSearch.ExitCode -eq 0 ) { "true" } else { "false" }
$footprintInspectOk = if( $footprintInspect.Ok -and $footprintInspect.ExitCode -eq 0 ) { "true" } else { "false" }
$footprintSearchDiagnosticOk = $footprintSearchOk

$footprintSearchBoardFilenameNegativeGreen = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_negative_green"
$footprintSearchBoardFilenameMissingOk = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_missing_ok"
$footprintSearchBoardFilenameEmptyOk = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_empty_ok"
$footprintSearchBoardFilenameNonStringOk = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_non_string_ok"
$footprintSearchQueryPriorityOverInvalidPathOk = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_query_priority_over_invalid_path_ok"
$footprintSearchBoardFilenameInvalidPathOk = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_invalid_path_ok"
$footprintSearchBoardFilenameOrderNote = Get-SummaryField -SummaryPath $footprintSearch.SummaryPath -FieldName "footprint_search_board_filename_order_note"

$footprintInspectBoardFilenameNegativeGreen = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_negative_green"
$footprintInspectBoardFilenameMissingOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_missing_ok"
$footprintInspectBoardFilenameEmptyOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_empty_ok"
$footprintInspectBoardFilenameNonStringOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_non_string_ok"
$footprintInspectBoardFilenameWhitespaceOnlyOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_whitespace_only_ok"
$footprintInspectBoardFilenameMismatchStructuredOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_mismatch_structured_ok"
$footprintInspectBoardFilenameInvalidPathOk = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_invalid_path_ok"
$footprintInspectBoardFilenameOrderNote = Get-SummaryField -SummaryPath $footprintInspect.SummaryPath -FieldName "footprint_inspect_board_filename_order_note"

$footprintSearchBoardFilenameNegativeGating = "true"
$footprintInspectBoardFilenameNegativeGating = "true"

$familyGreen = ( $symbolSearchOk -eq "true" -and
                 $symbolInspectOk -eq "true" -and
                 $footprintSearchOk -eq "true" -and
                 $footprintSearchBoardFilenameNegativeGreen -eq "true" -and
                 $footprintInspectOk -eq "true" -and
                 $footprintInspectBoardFilenameNegativeGreen -eq "true" )

$firstBlocker = "N/A"
$gatingResults = @($symbolSearch, $symbolInspect, $footprintSearch, $footprintInspect)
foreach( $r in $gatingResults )
{
    if( $r.ExitCode -ne 0 )
    {
        $firstBlocker = $r.Name + ":probe_exit_code_" + $r.ExitCode
        break
    }

    if( $r.ExactBlocker -ne "N/A" )
    {
        $firstBlocker = $r.Name + ":" + $r.ExactBlocker
        break
    }
}

if( $firstBlocker -eq "N/A" -and $footprintSearchBoardFilenameNegativeGreen -ne "true" )
{
    $firstBlocker = "footprint_search_board_filename_negative_green:" + $footprintSearchBoardFilenameNegativeGreen
    $familyGreen = $false
}

if( $firstBlocker -eq "N/A" -and $footprintInspectBoardFilenameNegativeGreen -ne "true" )
{
    $firstBlocker = "footprint_inspect_board_filename_negative_green:" + $footprintInspectBoardFilenameNegativeGreen
    $familyGreen = $false
}

$footprintSearchBlocker = "N/A"
if( $footprintSearch.ExitCode -ne 0 )
{
    $footprintSearchBlocker = "search_ok:probe_exit_code_" + $footprintSearch.ExitCode
}
elseif( $footprintSearch.ExactBlocker -ne "N/A" )
{
    $footprintSearchBlocker = "search_ok:" + $footprintSearch.ExactBlocker
}
elseif( $footprintSearchOk -ne "true" )
{
    $footprintSearchBlocker = "search_ok:search_not_ok"
}

if( $firstBlocker -eq "N/A" -and $footprintSearchOk -ne "true" )
{
    $firstBlocker = if( $footprintSearchBlocker -ne "N/A" ) { $footprintSearchBlocker } else { "footprint_search:search_not_ok" }
    $familyGreen = $false
}

@(
    "library_v1_family_green=" + $(if($familyGreen){"true"}else{"false"})
    "symbol_search_ok=$symbolSearchOk"
    "symbol_inspect_ok=$symbolInspectOk"
    "footprint_search_ok=$footprintSearchOk"
    "footprint_search_diagnostic_ok=$footprintSearchDiagnosticOk"
    "footprint_search_gating=true"
    "footprint_inspect_ok=$footprintInspectOk"
    "footprint_search_board_filename_negative_green=$footprintSearchBoardFilenameNegativeGreen"
    "footprint_search_board_filename_missing_ok=$footprintSearchBoardFilenameMissingOk"
    "footprint_search_board_filename_empty_ok=$footprintSearchBoardFilenameEmptyOk"
    "footprint_search_board_filename_non_string_ok=$footprintSearchBoardFilenameNonStringOk"
    "footprint_search_query_priority_over_invalid_path_ok=$footprintSearchQueryPriorityOverInvalidPathOk"
    "footprint_search_board_filename_invalid_path_ok=$footprintSearchBoardFilenameInvalidPathOk"
    "footprint_search_board_filename_order_note=$footprintSearchBoardFilenameOrderNote"
    "footprint_search_board_filename_negative_gating=$footprintSearchBoardFilenameNegativeGating"
    "footprint_inspect_board_filename_negative_green=$footprintInspectBoardFilenameNegativeGreen"
    "footprint_inspect_board_filename_missing_ok=$footprintInspectBoardFilenameMissingOk"
    "footprint_inspect_board_filename_empty_ok=$footprintInspectBoardFilenameEmptyOk"
    "footprint_inspect_board_filename_non_string_ok=$footprintInspectBoardFilenameNonStringOk"
    "footprint_inspect_board_filename_whitespace_only_ok=$footprintInspectBoardFilenameWhitespaceOnlyOk"
    "footprint_inspect_board_filename_mismatch_structured_ok=$footprintInspectBoardFilenameMismatchStructuredOk"
    "footprint_inspect_board_filename_invalid_path_ok=$footprintInspectBoardFilenameInvalidPathOk"
    "footprint_inspect_board_filename_order_note=$footprintInspectBoardFilenameOrderNote"
    "footprint_inspect_board_filename_negative_gating=$footprintInspectBoardFilenameNegativeGating"
    "exact_blocker_footprint_search_if_any=$footprintSearchBlocker"
    "exact_blocker_if_any=$firstBlocker"
    "symbol_search_target_run_dir=" + $symbolSearch.TargetRunDir
    "symbol_inspect_target_run_dir=" + $symbolInspect.TargetRunDir
    "footprint_search_target_run_dir=" + $footprintSearch.TargetRunDir
    "footprint_inspect_target_run_dir=" + $footprintInspect.TargetRunDir
    "symbol_search_exit_code=" + $symbolSearch.ExitCode
    "symbol_inspect_exit_code=" + $symbolInspect.ExitCode
    "footprint_search_exit_code=" + $footprintSearch.ExitCode
    "footprint_inspect_exit_code=" + $footprintInspect.ExitCode
    ""
) | Set-Content -Path (Join-Path $runDir "qa_library_v1_family_summary.txt") -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)
