param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "ipc_qa_common.ps1")
$runDir = Join-Path $BuildDir ("ipc-v1-artifacts\" + (Get-Date -Format "yyyyMMdd-HHmmssfff") + "-pcb-geometry-v1-targeted")
New-Item -ItemType Directory -Force -Path $runDir | Out-Null

$measureProbeScript = "targeted_pcb_measure_distance_probe.ps1"
$itemGeomProbeScript = "targeted_pcb_item_geometry_probe.ps1"
$measureSummaryName = "qa_pcb_measure_distance_session_contract_summary.txt"
$itemGeomSummaryName = "qa_pcb_item_geometry_session_contract_summary.txt"

function Get-SummaryField {
    param(
        [string[]]$SummaryLines,
        [string]$Key
    )
    $line = $SummaryLines | Where-Object { $_ -like ($Key + "=*") } | Select-Object -First 1
    if ( [string]::IsNullOrWhiteSpace( $line ) ) { return $null }
    $v = ( $line -replace ( "^" + [regex]::Escape( $Key ) + "=" ), "" ).Trim()
    return $v
}

function Invoke-TargetedProbe {
    # R91 — wrapper local : délègue le spawn à Invoke-IpcQaChildScript ; green/summary inchangés.
    param(
        [Parameter(Mandatory = $true)]
        [string]$ProbeScriptName,
        [Parameter(Mandatory = $true)]
        [string]$SummaryName,
        [Parameter(Mandatory = $true)]
        [string]$GreenFieldKey
    )

    $probePath = Join-Path $ScriptDir $ProbeScriptName

    if ( !(Test-Path -LiteralPath $probePath) ) {
        return [pscustomobject]@{
            ProbeScript  = $ProbeScriptName
            ExitCode     = -1
            TargetRunDir = "absent"
            Ok           = $false
            ExactBlocker = "probe_script_missing"
        }
    }

    $child = Invoke-IpcQaChildScript -ScriptPath $probePath -BuildDir $BuildDir `
        -LogDir $runDir -LogBaseName $ProbeScriptName `
        -TargetRunDirSource StdoutAndStderr `
        -SummaryFileName $SummaryName

    $targetRunDir = $child.TargetRunDir

    $exactBlocker = "N/A"
    $ok = $false

    if ( $targetRunDir -eq "absent" ) {
        $exactBlocker = "probe_target_run_dir_missing"
    }
    else {
        $childSummaryPath = Join-Path $targetRunDir $SummaryName
        if ( !(Test-Path -LiteralPath $childSummaryPath) ) {
            $exactBlocker = "summary_missing"
        }
        else {
            Copy-Item -LiteralPath $childSummaryPath -Destination (Join-Path $runDir ($ProbeScriptName + "." + $SummaryName)) -Force
            $summaryContent = Get-Content -LiteralPath $childSummaryPath
            $blockerLine = $summaryContent | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1

            if ( -not [string]::IsNullOrWhiteSpace( $blockerLine ) ) {
                $exactBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
            }
            else {
                $exactBlocker = "exact_blocker_field_missing"
            }

            $greenLine = Get-SummaryField -SummaryLines $summaryContent -Key $GreenFieldKey
            $probeGreenOk = ($greenLine -eq "true")

            if ( $exactBlocker -eq "N/A" -and $child.ExitCode -eq 0 -and $probeGreenOk ) {
                $ok = $true
            }
            elseif ( $exactBlocker -eq "N/A" -and -not $probeGreenOk ) {
                $exactBlocker = ($GreenFieldKey + "_false")
            }
        }
    }

    if ( $child.ExitCode -ne 0 -and $exactBlocker -eq "N/A" ) {
        $exactBlocker = "probe_exit_code_" + $child.ExitCode
    }

    return [pscustomobject]@{
        ProbeScript  = $ProbeScriptName
        ExitCode     = $child.ExitCode
        TargetRunDir = $targetRunDir
        Ok           = $ok
        ExactBlocker = $exactBlocker
        GreenKey     = $GreenFieldKey
    }
}

$rMeasure = Invoke-TargetedProbe -ProbeScriptName $measureProbeScript -SummaryName $measureSummaryName -GreenFieldKey "pcb_measure_distance_green"
$rItemGeom = Invoke-TargetedProbe -ProbeScriptName $itemGeomProbeScript -SummaryName $itemGeomSummaryName -GreenFieldKey "pcb_item_geometry_green"

$measureDistanceOk = if ( $rMeasure.Ok ) { "true" } else { "false" }
$itemGeometryOk = if ( $rItemGeom.Ok ) { "true" } else { "false" }
$familyGreen = ( $rMeasure.Ok -and $rItemGeom.Ok )

$measureItemToItemFamily = "absent"
$measurePadAnchorFamily = "absent"
$negativePadAnchorNonPadFamily = "absent"
$negativeInvalidItemPositionFamily = "absent"
$measurePadShapeOriginFamily = "absent"
$measurePadAnchorToShapeOriginFamily = "absent"
$negativePadShapeOriginMissingLayerFamily = "absent"
$negativePadShapeOriginNonPadFamily = "absent"
$negativeInvalidLayerFamily = "absent"
$measurePadShapeOriginAliasFCuFamily = "absent"
$ipcRetryExtraFamily = "absent"
if ( $rMeasure.TargetRunDir -ne "absent" ) {
    $childSummaryPathAgain = Join-Path $rMeasure.TargetRunDir $measureSummaryName
    if ( Test-Path -LiteralPath $childSummaryPathAgain ) {
        $sc2 = Get-Content -LiteralPath $childSummaryPathAgain
        $mit = Get-SummaryField -SummaryLines $sc2 -Key "measure_item_to_item_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $mit ) ) { $measureItemToItemFamily = $mit }
        $mpa = Get-SummaryField -SummaryLines $sc2 -Key "measure_pad_anchor_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $mpa ) ) { $measurePadAnchorFamily = $mpa }
        $npn = Get-SummaryField -SummaryLines $sc2 -Key "negative_pad_anchor_non_pad_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $npn ) ) { $negativePadAnchorNonPadFamily = $npn }
        $niv = Get-SummaryField -SummaryLines $sc2 -Key "negative_invalid_item_position_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $niv ) ) { $negativeInvalidItemPositionFamily = $niv }
        $mpso = Get-SummaryField -SummaryLines $sc2 -Key "measure_pad_shape_origin_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $mpso ) ) { $measurePadShapeOriginFamily = $mpso }
        $mpato = Get-SummaryField -SummaryLines $sc2 -Key "measure_pad_anchor_to_shape_origin_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $mpato ) ) { $measurePadAnchorToShapeOriginFamily = $mpato }
        $npsoml = Get-SummaryField -SummaryLines $sc2 -Key "negative_pad_shape_origin_missing_layer_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $npsoml ) ) { $negativePadShapeOriginMissingLayerFamily = $npsoml }
        $npsonp = Get-SummaryField -SummaryLines $sc2 -Key "negative_pad_shape_origin_non_pad_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $npsonp ) ) { $negativePadShapeOriginNonPadFamily = $npsonp }
        $nil = Get-SummaryField -SummaryLines $sc2 -Key "negative_invalid_layer_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $nil ) ) { $negativeInvalidLayerFamily = $nil }
        $mpsaf = Get-SummaryField -SummaryLines $sc2 -Key "measure_pad_shape_origin_alias_f_cu_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $mpsaf ) ) { $measurePadShapeOriginAliasFCuFamily = $mpsaf }
        $ire = Get-SummaryField -SummaryLines $sc2 -Key "ipc_smoke_ipc_retry_extra_total"
        if ( -not [string]::IsNullOrWhiteSpace( $ire ) ) { $ipcRetryExtraFamily = $ire }
    }
}

$geometryBasicFamily = "absent"
$geometryPadAnchorFamily = "absent"
$geometryPadShapeOriginFamily = "absent"
$negativeInvalidItemIdFamily = "absent"
$negativeItemNotFoundFamily = "absent"
$negativeInvalidLayerItemGeomFamily = "absent"
$ipcRetryExtraItemGeom = "absent"
if ( $rItemGeom.TargetRunDir -ne "absent" ) {
    $igPath = Join-Path $rItemGeom.TargetRunDir $itemGeomSummaryName
    if ( Test-Path -LiteralPath $igPath ) {
        $ig = Get-Content -LiteralPath $igPath
        $gb = Get-SummaryField -SummaryLines $ig -Key "geometry_basic_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $gb ) ) { $geometryBasicFamily = $gb }
        $gpa = Get-SummaryField -SummaryLines $ig -Key "geometry_pad_anchor_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $gpa ) ) { $geometryPadAnchorFamily = $gpa }
        $gps = Get-SummaryField -SummaryLines $ig -Key "geometry_pad_shape_origin_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $gps ) ) { $geometryPadShapeOriginFamily = $gps }
        $nii = Get-SummaryField -SummaryLines $ig -Key "negative_invalid_item_id_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $nii ) ) { $negativeInvalidItemIdFamily = $nii }
        $ninf = Get-SummaryField -SummaryLines $ig -Key "negative_item_not_found_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $ninf ) ) { $negativeItemNotFoundFamily = $ninf }
        $nilig = Get-SummaryField -SummaryLines $ig -Key "negative_invalid_layer_ok"
        if ( -not [string]::IsNullOrWhiteSpace( $nilig ) ) { $negativeInvalidLayerItemGeomFamily = $nilig }
        $ire2 = Get-SummaryField -SummaryLines $ig -Key "ipc_smoke_ipc_retry_extra_total"
        if ( -not [string]::IsNullOrWhiteSpace( $ire2 ) ) { $ipcRetryExtraItemGeom = $ire2 }
    }
}

$familyExactBlocker = "N/A"
if ( -not $familyGreen ) {
    $parts = @()
    if ( -not $rMeasure.Ok ) {
        if ( $rMeasure.ExitCode -ne 0 -and $rMeasure.ExactBlocker -eq "N/A" ) {
            $parts += "pcb_measure_distance:probe_exit_code_" + $rMeasure.ExitCode
        }
        elseif ( $rMeasure.ExactBlocker -ne "N/A" ) {
            $parts += "pcb_measure_distance:" + $rMeasure.ExactBlocker
        }
        else {
            $parts += "pcb_measure_distance:contract_or_unknown"
        }
    }
    if ( -not $rItemGeom.Ok ) {
        if ( $rItemGeom.ExitCode -ne 0 -and $rItemGeom.ExactBlocker -eq "N/A" ) {
            $parts += "pcb_item_geometry:probe_exit_code_" + $rItemGeom.ExitCode
        }
        elseif ( $rItemGeom.ExactBlocker -ne "N/A" ) {
            $parts += "pcb_item_geometry:" + $rItemGeom.ExactBlocker
        }
        else {
            $parts += "pcb_item_geometry:contract_or_unknown"
        }
    }
    if ( $parts.Count -gt 0 ) {
        $familyExactBlocker = $parts -join ";"
    }
}

@(
    "pcb_geometry_v1_family_green=" + $( if ( $familyGreen ) { "true" } else { "false" } )
    "pcb_measure_distance_ok=$measureDistanceOk"
    "pcb_item_geometry_ok=$itemGeometryOk"
    "measure_item_to_item_ok=$measureItemToItemFamily"
    "measure_pad_anchor_ok=$measurePadAnchorFamily"
    "negative_pad_anchor_non_pad_ok=$negativePadAnchorNonPadFamily"
    "negative_invalid_item_position_ok=$negativeInvalidItemPositionFamily"
    "measure_pad_shape_origin_ok=$measurePadShapeOriginFamily"
    "measure_pad_anchor_to_shape_origin_ok=$measurePadAnchorToShapeOriginFamily"
    "negative_pad_shape_origin_missing_layer_ok=$negativePadShapeOriginMissingLayerFamily"
    "negative_pad_shape_origin_non_pad_ok=$negativePadShapeOriginNonPadFamily"
    "negative_invalid_layer_ok=$negativeInvalidLayerFamily"
    "measure_pad_shape_origin_alias_f_cu_ok=$measurePadShapeOriginAliasFCuFamily"
    "geometry_basic_ok=$geometryBasicFamily"
    "geometry_pad_anchor_ok=$geometryPadAnchorFamily"
    "geometry_pad_shape_origin_ok=$geometryPadShapeOriginFamily"
    "negative_invalid_item_id_ok=$negativeInvalidItemIdFamily"
    "negative_item_not_found_ok=$negativeItemNotFoundFamily"
    "negative_invalid_layer_item_geometry_ok=$negativeInvalidLayerItemGeomFamily"
    "ipc_smoke_ipc_retry_extra_total_measure=$ipcRetryExtraFamily"
    "ipc_smoke_ipc_retry_extra_total_item_geometry=$ipcRetryExtraItemGeom"
    "exact_blocker_if_any=$familyExactBlocker"
    "pcb_measure_distance_target_run_dir=" + $rMeasure.TargetRunDir
    "pcb_item_geometry_target_run_dir=" + $rItemGeom.TargetRunDir
    "target_run_dir=$runDir"
    "pcb_measure_distance_exit_code=" + $rMeasure.ExitCode
    "pcb_item_geometry_exit_code=" + $rItemGeom.ExitCode
    ""
) | Set-Content -Path ( Join-Path $runDir "qa_pcb_geometry_v1_family_summary.txt" ) -Encoding UTF8

Write-Output ("TARGET_RUN_DIR=" + $runDir)

if ( -not $familyGreen ) { exit 1 }
exit 0
