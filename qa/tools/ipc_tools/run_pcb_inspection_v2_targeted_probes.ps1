param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"


$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-inspection-v2-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$probeItemsScriptName    = "targeted_pcb_get_board_items_probe.ps1"
$probeByIdScriptName     = "targeted_pcb_get_board_item_by_id_probe.ps1"
$probeNetItemsScriptName = "targeted_pcb_get_net_items_probe.ps1"
$childSummaryItemsName   = "qa_pcb_get_board_items_session_contract_summary.txt"
$childSummaryByIdName    = "qa_pcb_get_board_item_by_id_session_contract_summary.txt"
$childSummaryNetItemsName = "qa_pcb_get_net_items_session_contract_summary.txt"

function Get-SummaryField {
    param(
        [string[]]$SummaryLines,
        [string]$Key
    )
    $line = $SummaryLines | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if( [string]::IsNullOrWhiteSpace( $line ) ) { return $null }
    $v = ( $line -replace ( "^" + [regex]::Escape( $Key ) + "=" ), "" ).Trim()
    return $v
}

function Test-PcbGetBoardItemsContractOk {
    param( [string[]]$SummaryLines )

    $requiredBools = @(
        "default_call_ok"
        "counts_ok"
        "items_summary_ok"
        "max_items_call_ok"
        "max_items_truncation_ok"
        "negative_invalid_item_type_ok"
        "negative_board_filename_mismatch_ok"
    )
    foreach( $k in $requiredBools )
    {
        $v = Get-SummaryField -SummaryLines $SummaryLines -Key $k
        if( $v -ne "true" ) { return $false }
    }
    $eb = Get-SummaryField -SummaryLines $SummaryLines -Key "exact_blocker_if_any"
    if( $eb -ne "N/A" ) { return $false }
    return $true
}

function Test-PcbGetBoardItemByIdContractOk {
    param( [string[]]$SummaryLines )

    $requiredBools = @(
        "inventory_call_ok"
        "item_selected_ok"
        "default_call_ok"
        "item_details_ok"
        "negative_invalid_item_id_ok"
        "negative_item_not_found_ok"
        "negative_board_filename_mismatch_ok"
    )
    foreach( $k in $requiredBools )
    {
        $v = Get-SummaryField -SummaryLines $SummaryLines -Key $k
        if( $v -ne "true" ) { return $false }
    }
    $eb = Get-SummaryField -SummaryLines $SummaryLines -Key "exact_blocker_if_any"
    if( $eb -ne "N/A" ) { return $false }
    return $true
}

function Test-PcbGetNetItemsContractOk {
    param( [string[]]$SummaryLines )

    $requiredBools = @(
        "net_selected_ok"
        "default_call_ok"
        "counts_ok"
        "items_summary_ok"
        "max_items_call_ok"
        "max_items_truncation_ok"
        "negative_invalid_net_selector_ok"
        "negative_net_not_found_ok"
        "negative_board_filename_mismatch_ok"
    )
    foreach( $k in $requiredBools )
    {
        $v = Get-SummaryField -SummaryLines $SummaryLines -Key $k
        if( $v -ne "true" ) { return $false }
    }
    $mm = Get-SummaryField -SummaryLines $SummaryLines -Key "negative_net_selector_mismatch_ok"
    if( $mm -ne "true" -and $mm -ne "skipped" ) { return $false }
    $eb = Get-SummaryField -SummaryLines $SummaryLines -Key "exact_blocker_if_any"
    if( $eb -ne "N/A" ) { return $false }
    return $true
}

function Invoke-TargetedProbe {
    # R93 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; contrats/green inchangés.
    param(
        [string]$ProbePath,
        [string]$ProbeScriptLabel,
        [string]$ChildSummaryName,
        [ValidateSet( 'board_items', 'board_item_by_id', 'net_items' )]
        [string]$ContractKind
    )

    $child = Invoke-IpcQaChildScript -ScriptPath $ProbePath -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName $ProbeScriptLabel `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $ChildSummaryName

    $targetRunDir = $child.TargetRunDir

    $contractOk = $false
    $exactBlocker = "N/A"

    if( $targetRunDir -eq "absent" )
    {
        $exactBlocker = "probe_target_run_dir_missing"
    }
    elseif( !(Test-Path -LiteralPath ( Join-Path $targetRunDir $ChildSummaryName )) )
    {
        $exactBlocker = "summary_missing"
    }
    else
    {
        $childSummaryPath = Join-Path $targetRunDir $ChildSummaryName
        Copy-Item -LiteralPath $childSummaryPath -Destination ( Join-Path $runDir $ChildSummaryName ) -Force
        $summaryLines = Get-Content -LiteralPath $childSummaryPath
        $ebLine = Get-SummaryField -SummaryLines $summaryLines -Key "exact_blocker_if_any"
        if( [string]::IsNullOrWhiteSpace( $ebLine ) )
        {
            $exactBlocker = "exact_blocker_field_missing"
        }
        else
        {
            $exactBlocker = $ebLine.Trim()
        }

        if( $ContractKind -eq 'board_items' )
        {
            $contractOk = Test-PcbGetBoardItemsContractOk -SummaryLines $summaryLines
        }
        elseif( $ContractKind -eq 'board_item_by_id' )
        {
            $contractOk = Test-PcbGetBoardItemByIdContractOk -SummaryLines $summaryLines
        }
        else
        {
            $contractOk = Test-PcbGetNetItemsContractOk -SummaryLines $summaryLines
        }
        if( -not $contractOk -and $exactBlocker -eq "N/A" )
        {
            $exactBlocker = "contract_checks_failed"
        }
    }

    if( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" )
    {
        $exactBlocker = "probe_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        ExitCode       = $child.ExitCode
        TargetRunDir   = $targetRunDir
        ContractOk     = $contractOk
        ExactBlocker   = $exactBlocker
    }
}

function Write-FamilySummaryEarlyExit {
    param(
        [string]$ItemsOk,
        [string]$ByIdOk,
        [string]$NetItemsOk,
        [string]$FamilyGreen,
        [string]$Blocker,
        [string]$ItemsRunDir,
        [string]$ByIdRunDir,
        [string]$NetItemsRunDir,
        [int]$ItemsExit,
        [int]$ByIdExit,
        [int]$NetItemsExit
    )
    @(
        "pcb_inspection_v2_family_green=$FamilyGreen"
        "pcb_get_board_items_ok=$ItemsOk"
        "pcb_get_board_item_by_id_ok=$ByIdOk"
        "pcb_get_net_items_ok=$NetItemsOk"
        "exact_blocker_if_any=$Blocker"
        "pcb_get_board_items_target_run_dir=$ItemsRunDir"
        "pcb_get_board_item_by_id_target_run_dir=$ByIdRunDir"
        "pcb_get_net_items_target_run_dir=$NetItemsRunDir"
        "pcb_get_board_items_exit_code=$ItemsExit"
        "pcb_get_board_item_by_id_exit_code=$ByIdExit"
        "pcb_get_net_items_exit_code=$NetItemsExit"
        ""
    ) | Set-Content -Path ( Join-Path $runDir "qa_pcb_inspection_v2_family_summary.txt" ) -Encoding UTF8
}

$probeItemsPath = Join-Path $ScriptDir $probeItemsScriptName
$probeByIdPath  = Join-Path $ScriptDir $probeByIdScriptName
$probeNetItemsPath = Join-Path $ScriptDir $probeNetItemsScriptName

if( !(Test-Path -LiteralPath $probeItemsPath) )
{
    Write-FamilySummaryEarlyExit -ItemsOk "false" -ByIdOk "false" -NetItemsOk "false" -FamilyGreen "false" `
        -Blocker "pcb_get_board_items:probe_script_missing" -ItemsRunDir "absent" -ByIdRunDir "absent" -NetItemsRunDir "absent" `
        -ItemsExit -1 -ByIdExit -1 -NetItemsExit -1
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}

if( !(Test-Path -LiteralPath $probeByIdPath) )
{
    Write-FamilySummaryEarlyExit -ItemsOk "false" -ByIdOk "false" -NetItemsOk "false" -FamilyGreen "false" `
        -Blocker "pcb_get_board_item_by_id:probe_script_missing" -ItemsRunDir "absent" -ByIdRunDir "absent" -NetItemsRunDir "absent" `
        -ItemsExit -1 -ByIdExit -1 -NetItemsExit -1
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}

if( !(Test-Path -LiteralPath $probeNetItemsPath) )
{
    Write-FamilySummaryEarlyExit -ItemsOk "false" -ByIdOk "false" -NetItemsOk "false" -FamilyGreen "false" `
        -Blocker "pcb_get_net_items:probe_script_missing" -ItemsRunDir "absent" -ByIdRunDir "absent" -NetItemsRunDir "absent" `
        -ItemsExit -1 -ByIdExit -1 -NetItemsExit -1
    Write-Output ("TARGET_RUN_DIR=" + $runDir)
    exit 1
}

$pcbItems = Invoke-TargetedProbe -ProbePath $probeItemsPath -ProbeScriptLabel "pcb_get_board_items" `
    -ChildSummaryName $childSummaryItemsName -ContractKind 'board_items'

$pcbById = Invoke-TargetedProbe -ProbePath $probeByIdPath -ProbeScriptLabel "pcb_get_board_item_by_id" `
    -ChildSummaryName $childSummaryByIdName -ContractKind 'board_item_by_id'

$pcbNetItems = Invoke-TargetedProbe -ProbePath $probeNetItemsPath -ProbeScriptLabel "pcb_get_net_items" `
    -ChildSummaryName $childSummaryNetItemsName -ContractKind 'net_items'

$pcbGetBoardItemsOk = ( $pcbItems.ContractOk -and $pcbItems.ExitCode -eq 0 )
$pcbGetBoardItemByIdOk = ( $pcbById.ContractOk -and $pcbById.ExitCode -eq 0 )
$pcbGetNetItemsOk = ( $pcbNetItems.ContractOk -and $pcbNetItems.ExitCode -eq 0 )
$familyGreen = ( $pcbGetBoardItemsOk -and $pcbGetBoardItemByIdOk -and $pcbGetNetItemsOk )

$familyExactBlocker = "N/A"
if( -not $familyGreen )
{
    if( -not $pcbGetBoardItemsOk )
    {
        if( $pcbItems.ExitCode -ne 0 )
        {
            $familyExactBlocker = "pcb_get_board_items:probe_exit_code_" + $pcbItems.ExitCode
        }
        elseif( $pcbItems.ExactBlocker -ne "N/A" )
        {
            $familyExactBlocker = "pcb_get_board_items:" + $pcbItems.ExactBlocker
        }
        else
        {
            $familyExactBlocker = "pcb_get_board_items:contract_or_unknown"
        }
    }
    elseif( -not $pcbGetBoardItemByIdOk )
    {
        if( $pcbById.ExitCode -ne 0 )
        {
            $familyExactBlocker = "pcb_get_board_item_by_id:probe_exit_code_" + $pcbById.ExitCode
        }
        elseif( $pcbById.ExactBlocker -ne "N/A" )
        {
            $familyExactBlocker = "pcb_get_board_item_by_id:" + $pcbById.ExactBlocker
        }
        else
        {
            $familyExactBlocker = "pcb_get_board_item_by_id:contract_or_unknown"
        }
    }
    else
    {
        if( $pcbNetItems.ExitCode -ne 0 )
        {
            $familyExactBlocker = "pcb_get_net_items:probe_exit_code_" + $pcbNetItems.ExitCode
        }
        elseif( $pcbNetItems.ExactBlocker -ne "N/A" )
        {
            $familyExactBlocker = "pcb_get_net_items:" + $pcbNetItems.ExactBlocker
        }
        else
        {
            $familyExactBlocker = "pcb_get_net_items:contract_or_unknown"
        }
    }
}

@(
    "pcb_inspection_v2_family_green=" + $( if( $familyGreen ) { "true" } else { "false" } )
    "pcb_get_board_items_ok=" + $( if( $pcbGetBoardItemsOk ) { "true" } else { "false" } )
    "pcb_get_board_item_by_id_ok=" + $( if( $pcbGetBoardItemByIdOk ) { "true" } else { "false" } )
    "pcb_get_net_items_ok=" + $( if( $pcbGetNetItemsOk ) { "true" } else { "false" } )
    "exact_blocker_if_any=$familyExactBlocker"
    "pcb_get_board_items_target_run_dir=" + $pcbItems.TargetRunDir
    "pcb_get_board_item_by_id_target_run_dir=" + $pcbById.TargetRunDir
    "pcb_get_net_items_target_run_dir=" + $pcbNetItems.TargetRunDir
    "pcb_get_board_items_exit_code=" + $pcbItems.ExitCode
    "pcb_get_board_item_by_id_exit_code=" + $pcbById.ExitCode
    "pcb_get_net_items_exit_code=" + $pcbNetItems.ExitCode
    ""
) | Set-Content -Path ( Join-Path $runDir "qa_pcb_inspection_v2_family_summary.txt" ) -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)

if( -not $familyGreen ) { exit 1 }
exit 0
