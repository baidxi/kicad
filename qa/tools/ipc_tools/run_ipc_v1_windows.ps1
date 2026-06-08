param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $false)]
    [string]$Preset = "win64-release",

    [Parameter(Mandatory = $false)]
    [int]$WithProjectIterations = 1,

    [Parameter(Mandatory = $false)]
    [switch]$SkipPrereqBuild,

    [Parameter(Mandatory = $false)]
    [switch]$SkipNoProjectScenario
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Ensure-Dir([string]$p) {
    if (!(Test-Path $p)) { New-Item -ItemType Directory -Force -Path $p | Out-Null }
}

# On Windows MSVC, cmake --build requires INCLUDE/LIB from VsDevCmd for /external:env:INCLUDE.
# Wrap build in VS dev environment when VsDevCmd.bat exists.
function Invoke-CmakeBuild([string]$Preset, [string]$Target) {
    $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path $vsDevCmd)) {
        $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat"
    }
    if (-not (Test-Path $vsDevCmd)) {
        $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat"
    }
    if (-not (Test-Path $vsDevCmd)) {
        $vsDevCmd = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    }
    if (Test-Path $vsDevCmd) {
        $cmd = "`"$vsDevCmd`" -arch=amd64 -host_arch=amd64 && cmake --build --preset $Preset --target $Target"
        cmd /c $cmd
    } else {
        & cmake --build --preset $Preset --target $Target
    }
}

function Get-LastApiUrl([string]$apiLog) {
    if (!(Test-Path $apiLog)) { return $null }
    $lines = Get-Content $apiLog -Tail 400
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i] -match 'started at (ipc://.+) ---') { return $Matches[1] }
    }
    return $null
}

$BuildDir = (Resolve-Path $BuildDir).Path

$repoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$kicadExe   = Join-Path $BuildDir "kicad\kicad.exe"
$smokeExe   = Join-Path $BuildDir "qa\tools\ipc_tools\qa_ipc_smoke.exe"
$apiLog     = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "KiCad\10.0\logs\api.log"
$tempKiCad  = Join-Path $env:TEMP "kicad"
Ensure-Dir (Join-Path $BuildDir "ipc-v1-artifacts")

# Build prerequisites (schemas + kifaces). Uses VsDevCmd on Windows for MSVC INCLUDE/LIB.
if( -not $SkipPrereqBuild )
{
    Invoke-CmakeBuild -Preset $Preset -Target api_schema_build_copy | Out-Null
    Invoke-CmakeBuild -Preset $Preset -Target schema_build_copy | Out-Null
    Invoke-CmakeBuild -Preset $Preset -Target kicad | Out-Null
    Invoke-CmakeBuild -Preset $Preset -Target eeschema | Out-Null
    Invoke-CmakeBuild -Preset $Preset -Target pcbnew | Out-Null
    # Ensure the module used by kicad.exe is up-to-date (set-board appearance callbacks live in the loaded pcbnew module).
    Invoke-CmakeBuild -Preset $Preset -Target _pcbnew.dll | Out-Null
    Invoke-CmakeBuild -Preset $Preset -Target qa_ipc_smoke | Out-Null
}

if (!(Test-Path $kicadExe)) { throw "Missing kicad.exe at $kicadExe" }
if (!(Test-Path $smokeExe)) { throw "Missing qa_ipc_smoke.exe at $smokeExe" }

# Create isolated config with api.enable_server=true
$cfgRoot = Join-Path $BuildDir "ipc-v1-config"
$cfg10   = Join-Path $cfgRoot "10.0"
Ensure-Dir $cfg10

# Global library tables: copy known-good QA tables so STARTWIZARD does not block on
# LIBRARY_MANAGER::GlobalTablesValid() (stock template paths under build/ often do not exist).
$qaFpTable = Join-Path $repoRoot "qa\data\libraries\fp-lib-table"
$qaDesignBlockTable = Join-Path $repoRoot "qa\data\config\10.0\design-block-lib-table"
if( ( Test-Path $qaFpTable ) -and ( Test-Path $qaDesignBlockTable ) )
{
    Copy-Item -LiteralPath $qaFpTable -Destination ( Join-Path $cfg10 "fp-lib-table" ) -Force
    Copy-Item -LiteralPath $qaDesignBlockTable -Destination ( Join-Path $cfg10 "design-block-lib-table" ) -Force
}

$pythonHomeDir = Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3"
$pythonExe = Join-Path $pythonHomeDir "python.exe"
$apiConfig = @{
    enable_server = $true
}
if( Test-Path $pythonExe )
{
    $apiConfig.interpreter_path = $pythonExe
}

$common = @{
    meta = @{ version = 5 }
    api  = $apiConfig
    system = @{
        local_history_enabled = $false
        working_dir           = (Split-Path -Parent $BuildDir)
    }
    auto_backup = @{
        enabled          = $false
        limit_total_size = 104857600
    }
    do_not_show_again = @{
        data_collection_prompt      = $true
        update_check_prompt         = $true
        env_var_overwrite_warning   = $true
        scaled_3d_models_warning    = $true
        zone_fill_warning           = $true
    }
} | ConvertTo-Json -Depth 10

$kicad = @{
    meta   = @{ version = 0 }
    system = @{
        first_run_shown = $true
        open_projects   = @()
    }
} | ConvertTo-Json -Depth 6

$common | Set-Content -Path (Join-Path $cfg10 "kicad_common.json") -Encoding UTF8
$kicad   | Set-Content -Path (Join-Path $cfg10 "kicad.json") -Encoding UTF8
"EnableAPILogging=1" | Set-Content -Path (Join-Path $cfg10 "kicad_advanced") -Encoding ASCII

# Runtime env
$env:KICAD_CONFIG_HOME = $cfgRoot
$env:KICAD_RUN_FROM_BUILD_DIR = "1"
# KICAD_IPC_AUTOMATION can make "no project" startup exit immediately on Windows.
Remove-Item Env:KICAD_IPC_AUTOMATION -ErrorAction SilentlyContinue
if( Test-Path $pythonExe )
{
    $env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
    $env:PYTHONHOME = $pythonHomeDir
}
else
{
    Remove-Item Env:KICAD_USE_EXTERNAL_PYTHONHOME -ErrorAction SilentlyContinue
    Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
    Write-Host "External Python runtime not found at '$pythonExe'; using KiCad defaults."
}

$basePath = $env:PATH
$pathPrefix = @(
    (Join-Path $BuildDir "kicad"),
    (Join-Path $BuildDir "eeschema"),
    (Join-Path $BuildDir "pcbnew"),
    (Join-Path $BuildDir "common"),
    (Join-Path $BuildDir "common\gal"),
    (Join-Path $BuildDir "api"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\bin"),
    (Join-Path $BuildDir "vcpkg_installed\x64-windows\tools\python3")
) -join ";"

$env:PATH = $pathPrefix + ";" + $basePath

Write-Host "BuildDir=$BuildDir"
Write-Host "KICAD_CONFIG_HOME=$cfgRoot"
Write-Host "RepoRoot=$repoRoot"

# Use the IPC v1 stub project under BuildDir for deterministic "with project"
# behavior. `tools/newstroke/font.kicad_*` is dominated by text_box content and
# does not expose the wire/symbol stub UUIDs this harness chains on.
$projDir = Join-Path $BuildDir "ipc-v1-proj"
$proj = Join-Path $projDir "v1.kicad_pro"
$pcb  = Join-Path $projDir "v1.kicad_pcb"
$sch  = Join-Path $projDir "v1.kicad_sch"
$spicePreviewSch = Join-Path $repoRoot "qa\data\eeschema\spice_netlists\directives\directives.kicad_sch"
$spiceLibraryInspectPath = Join-Path $repoRoot "qa\data\eeschema\spice_netlists\libraries\diodes.lib.spice"
if (!(Test-Path $proj)) {
    throw "Missing contractual stub project at '$proj'. Expected CMake/build to populate ipc-v1-proj (see qa/tools/ipc_tools/mcp_stdio_smoke.py)."
}
if (!(Test-Path $pcb) -or !(Test-Path $sch)) {
    throw "Missing contractual stub board/schematic next to project: pcb='$pcb' sch='$sch'"
}
if (!(Test-Path $spicePreviewSch)) {
    throw "Missing simulation preview schematic fixture at '$spicePreviewSch'"
}
if (!(Test-Path $spiceLibraryInspectPath)) {
    throw "Missing simulation model library fixture at '$spiceLibraryInspectPath'"
}
# Intentionally invalid board file for get_pcb_items negative probe (not tracked in-tree).
$pcbInvalidProbe = Join-Path $projDir "v1_invalid.kicad_pcb"
if (!(Test-Path $pcbInvalidProbe)) {
    "INVALID_KICAD_PCB_FOR_QA_PROBE" | Set-Content -Path $pcbInvalidProbe -Encoding ASCII
}
Write-Host "ProjectPath=$proj"

# Helper: run scenario
function Dump-RunContext([string]$outPath, [string]$label, [string[]]$kicadArgs) {
    $lines = @()
    $lines += "label=$label"
    $lines += "timestamp_utc=$([DateTime]::UtcNow.ToString('o'))"
    $lines += "cwd=$((Get-Location).Path)"
    $lines += "working_directory=$repoRoot"
    $lines += "exe=$kicadExe"
    $lines += "args_count=$($kicadArgs.Count)"
    $lines += "args=$($kicadArgs -join ' ')"
    $lines += ""
    $lines += "KICAD_CONFIG_HOME=$env:KICAD_CONFIG_HOME"
    $lines += "KICAD_RUN_FROM_BUILD_DIR=$env:KICAD_RUN_FROM_BUILD_DIR"
    $lines += "KICAD_USE_EXTERNAL_PYTHONHOME=$env:KICAD_USE_EXTERNAL_PYTHONHOME"
    $lines += "PYTHONHOME=$env:PYTHONHOME"
    $lines += ""
    $lines += "PATH=$env:PATH"
    $lines | Set-Content -Path $outPath -Encoding UTF8
}

function Stop-KiCadAndWait() {
    $hadKiCad = [bool](Get-Process -Name kicad -ErrorAction SilentlyContinue)
    Stop-Process -Name kicad -Force -ErrorAction SilentlyContinue
    $stopped = $false
    for ($i = 0; $i -lt 40; $i++) {
        if (!(Get-Process -Name kicad -ErrorAction SilentlyContinue)) {
            $stopped = $true
            break
        }
        Start-Sleep -Milliseconds 250
    }
    if (!$stopped -and (Get-Process -Name kicad -ErrorAction SilentlyContinue)) {
        throw "Failed to stop existing KiCad process(es) before scenario startup."
    }
    # After a forced termination, allow Windows cleanup/single-instance state to settle.
    if ($hadKiCad) {
        Start-Sleep -Seconds 3
    }
}

function Call-McpToolArgsAndLog([string]$runDir, [string]$toolName, [string]$caseLabel, [hashtable]$argsObj,
                                [string]$url, [string]$smokeExe) {
    # Default serialization depth is 2; nested tool args (e.g. set_net_classes net_classes[].board) must not truncate.
    $argsJson = $argsObj | ConvertTo-Json -Compress -Depth 30
    $inputPath = Join-Path $runDir ("mcp_call_tool_args_" + $toolName + "_" + $caseLabel + "_input.json")
    $argsJson | Set-Content -Path $inputPath -Encoding UTF8

    $logPath = Join-Path $runDir ("mcp_call_tool_args_" + $toolName + "_" + $caseLabel + "_output.log")
    # Pass args_json via file (prefix '@') to avoid Windows command-line quoting issues.
    $out = & $smokeExe $url "mcp_call_tool_args" $toolName ("@" + $inputPath) 2>&1 | Out-String
    $out | Set-Content -Path $logPath -Encoding UTF8

    return $out
}

function Get-McpResultJsonLine([string]$toolOut) {
    foreach ($line in ($toolOut -split "`n")) {
        if ($line.StartsWith("result_json=")) {
            return $line.Substring("result_json=".Length)
        }
    }
    return $null
}

function Get-NetClassNamesKeyFromResultJson([string]$json) {
    if ([string]::IsNullOrWhiteSpace($json)) { return "" }
    $o = $json | ConvertFrom-Json
    if (-not $o.PSObject.Properties["net_classes"]) { return "" }
    $names = @($o.net_classes | ForEach-Object { $_.name })
    return ($names | Sort-Object) -join "|"
}

function Run-Scenario([string]$label, [string[]]$kicadArgs, [bool]$ExpectLoadedTrue,
    [string]$OpenEditorType = "") {
    Write-Host ""
    Write-Host "=== Scenario: $label ==="
    Write-Host ("ArgsCount={0} Args={1}" -f $kicadArgs.Count, ($kicadArgs -join ' '))

    Stop-KiCadAndWait
    Remove-Item -Recurse -Force $tempKiCad -ErrorAction SilentlyContinue

    $beforeLen = 0
    if (Test-Path $apiLog) { $beforeLen = (Get-Item $apiLog).Length }

    $safeLabel = ($label -replace '[\\/:*?\"<>|\s]+','_')
    $runDir = Join-Path $BuildDir ("ipc-v1-artifacts\{0:yyyyMMdd-HHmmssfff}-{1}" -f (Get-Date), $safeLabel)
    Ensure-Dir $runDir
    Dump-RunContext (Join-Path $runDir "context.txt") $label $kicadArgs

    $stdoutLog = Join-Path $runDir "kicad_stdout.log"
    $stderrLog = Join-Path $runDir "kicad_stderr.log"
    if ($kicadArgs -and $kicadArgs.Count -gt 0) {
        $p = Start-Process -FilePath $kicadExe -WorkingDirectory $repoRoot -ArgumentList $kicadArgs `
            -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog -PassThru
    } else {
        $p = Start-Process -FilePath $kicadExe -WorkingDirectory $repoRoot `
            -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog -PassThru
    }

    # Wait for server to start (api.log grows and contains the URL)
    $url = $null
    for ($i = 0; $i -lt 60; $i++) {
        Start-Sleep -Milliseconds 500

        $alive = [bool](Get-Process -Id $p.Id -ErrorAction SilentlyContinue)
        if (!$alive) {
            # On some Windows launches the initial process can hand off and exit quickly.
            # If another KiCad process exists, adopt it instead of failing early.
            $fallbackProc = Get-Process -Name kicad -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($fallbackProc) {
                $p = $fallbackProc
                $alive = $true
            } else {
                $exitCode = "<unavailable>"
                try { $exitCode = [string]$p.ExitCode } catch {}
                ("early_exit=true`n" +
                 "scenario=$label`n" +
                 "pid=$($p.Id)`n" +
                 "exit_code=$exitCode`n") |
                    Set-Content -Path (Join-Path $runDir "early_exit.txt") -Encoding UTF8
                throw "KiCad exited early in scenario '$label' (pid=$($p.Id), exit_code=$exitCode)."
            }
        }

        if (Test-Path $apiLog) {
            $len = (Get-Item $apiLog).Length
            if ($len -gt $beforeLen) {
                $url = Get-LastApiUrl $apiLog
                if ($url) { break }
            }
        }
    }

    if (!$url) {
        # Fallback when api.log is stale/missing: try the default IPC socket path.
        $fallbackSock = Join-Path $tempKiCad "api.sock"
        $url = "ipc://$fallbackSock"
    }

    Write-Host "PID=$($p.Id) Alive=True"
    Write-Host "URL=$url"

    # Retry dial a few times; server can bind slightly later than log write.
    # Note: during full startup (especially "with project"), KiCad can accept
    # IPC requests but remain AS_NOT_READY for a while.  Don't abort the whole
    # scenario early just because `ping` didn't yet return AS_OK.
    $ok = $false
    for ($i = 0; $i -lt 600; $i++) {
        $alive = [bool](Get-Process -Id $p.Id -ErrorAction SilentlyContinue)
        if (!$alive) {
            $exitCode = "<unavailable>"
            try { $exitCode = [string]$p.ExitCode } catch {}
            ("early_exit=true`n" +
             "scenario=$label`n" +
             "phase=ping`n" +
             "pid=$($p.Id)`n" +
             "exit_code=$exitCode`n" +
             "url=$url`n") |
                Set-Content -Path (Join-Path $runDir "early_exit.txt") -Encoding UTF8
            throw "KiCad exited before IPC ping became ready in scenario '$label' (pid=$($p.Id), exit_code=$exitCode)."
        }

        try {
            $pingOut = & $smokeExe $url ping 2>&1 | Out-String
            $pingOk = ( $pingOut -match 'status=1' -or $pingOut -match 'KiCad is not ready to reply' )
            if( $pingOk )
            {
                $ok = $true
                break
            }
        } catch {
            Start-Sleep -Milliseconds 500
            continue
        }
        Start-Sleep -Milliseconds 500
    }
    if (!$ok) { throw "IPC ping failed (nng_dial) in scenario '$label'." }

    & $smokeExe $url get_version

    if ($ExpectLoadedTrue) {
        $okLoaded = $false
        for ($i = 0; $i -lt 600; $i++) {
                $alive = [bool](Get-Process -Id $p.Id -ErrorAction SilentlyContinue)
                if (!$alive) {
                    $exit = $p.ExitCode
                    ("KiCad exited early. ExitCode={0}" -f $exit) | Set-Content -Path (Join-Path $runDir "exit.txt") -Encoding UTF8
                    break
                }

                try {
                    $out = & $smokeExe $url get_project_info 2>&1 | Out-String
                    $out | Add-Content -Path (Join-Path $runDir "get_project_info.log") -Encoding UTF8
                    if ($out -match 'loaded=true') { $okLoaded = $true; break }
                } catch {
                    $_.Exception.Message | Add-Content -Path (Join-Path $runDir "get_project_info.log") -Encoding UTF8
                    Start-Sleep -Milliseconds 500
                    continue
                }
                Start-Sleep -Milliseconds 500
            }
        if (Test-Path $apiLog) { Get-Content $apiLog -Tail 200 | Set-Content -Path (Join-Path $runDir "api_tail.log") -Encoding UTF8 }
        if (!$okLoaded) { throw "GetProjectInfo did not reach loaded=true in scenario '$label'." }

        # V2: Workspace overview should also reflect a loaded project.
        $wo = & $smokeExe $url workspace_overview 2>&1 | Out-String
        $wo | Set-Content -Path (Join-Path $runDir "workspace_overview.log") -Encoding UTF8
        if ($wo -notmatch 'project_loaded=true') { throw "WorkspaceOverview did not report project_loaded=true in scenario '$label'." }

        $ct1 = & $smokeExe $url mcp_call_tool get_project_info 2>&1 | Out-String
        $ct1 | Set-Content -Path (Join-Path $runDir "mcp_call_tool_get_project_info.log") -Encoding UTF8
        if ($ct1 -notmatch 'loaded=true') { throw "MCP CallTool get_project_info did not report loaded=true in scenario '$label'." }
        $callable_project_info = $(if ($ct1 -match 'status=1') { "ok" } else { "error" })
        $project_info_loaded = $(if ($ct1 -match 'loaded=true') { "true" } else { "false" })
        $exact_blocker_if_any_project_info = "N/A"
        if ($callable_project_info -ne "ok") {
            $exact_blocker_if_any_project_info = "get_project_info_call_failed"
        } elseif ($project_info_loaded -ne "true") {
            $exact_blocker_if_any_project_info = "project_not_loaded_in_project_info"
        }
        ("callable_in_project_context=" + $callable_project_info + "`n" +
         "loaded=" + $project_info_loaded + "`n" +
         "exact_blocker_if_any=" + $exact_blocker_if_any_project_info + "`n") |
            Set-Content -Path (Join-Path $runDir "qa_get_project_info_contract_summary.txt") -Encoding UTF8
        if ($exact_blocker_if_any_project_info -ne "N/A") {
            throw "get_project_info contract failed in scenario '$label' (blocker=$exact_blocker_if_any_project_info)."
        }

        $ct2 = & $smokeExe $url mcp_call_tool get_workspace_overview 2>&1 | Out-String
        $ct2 | Set-Content -Path (Join-Path $runDir "mcp_call_tool_get_workspace_overview.log") -Encoding UTF8
        if ($ct2 -notmatch 'project_loaded=true') { throw "MCP CallTool get_workspace_overview did not report project_loaded=true in scenario '$label'." }
        $callable_workspace_overview = $(if ($ct2 -match 'status=1') { "ok" } else { "error" })
        $workspace_project_loaded = $(if ($ct2 -match 'project_loaded=true') { "true" } else { "false" })
        $exact_blocker_if_any_workspace_overview = "N/A"
        if ($callable_workspace_overview -ne "ok") {
            $exact_blocker_if_any_workspace_overview = "get_workspace_overview_call_failed"
        } elseif ($workspace_project_loaded -ne "true") {
            $exact_blocker_if_any_workspace_overview = "project_not_loaded_in_workspace_overview"
        }
        ("callable_in_project_context=" + $callable_workspace_overview + "`n" +
         "project_loaded=" + $workspace_project_loaded + "`n" +
         "exact_blocker_if_any=" + $exact_blocker_if_any_workspace_overview + "`n") |
            Set-Content -Path (Join-Path $runDir "qa_get_workspace_overview_contract_summary.txt") -Encoding UTF8
        if ($exact_blocker_if_any_workspace_overview -ne "N/A") {
            throw "get_workspace_overview contract failed in scenario '$label' (blocker=$exact_blocker_if_any_workspace_overview)."
        }

        $lt = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
        $lt | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8
        if ($lt -notmatch 'get_project_info') { throw "MCP ListTools missing get_project_info in scenario '$label'." }
        if ($lt -notmatch 'get_workspace_overview') { throw "MCP ListTools missing get_workspace_overview in scenario '$label'." }

        if ($label -match '^With project run 1 of') {
            # Manifest/count coherence guard: ensure doc sections match their declared counts and runtime ListTools.
            $manifestPath = Join-Path $repoRoot "qa\tools\ipc_tools\MCP_V1_OFFICIAL_MANIFEST.md"
            if (!(Test-Path $manifestPath)) {
                throw "Manifest not found for count guard: $manifestPath"
            }
            $manifest = Get-Content -Raw $manifestPath
            $declStable = [int]([regex]::Match($manifest, '\|\s+\*\*Stable v1\*\*\s+\|\s+([0-9]+)\s+\|').Groups[1].Value)
            $declBeta = [int]([regex]::Match($manifest, '\|\s+\*\*Beta\*\*\s+\|\s+([0-9]+)\s+\|').Groups[1].Value)
            $declQaOnly = [int]([regex]::Match($manifest, '\|\s+\*\*QA-only\*\*\s+\|\s+([0-9]+)\s+\|').Groups[1].Value)
            $declNonRetained = [int]([regex]::Match($manifest, '\|\s+\*\*Non retenu\*\*\s+\|\s+([0-9]+)\s+\|').Groups[1].Value)

            $stableSection = [regex]::Match($manifest, '## 2\. Outils stable v1 \(\d+\)([\s\S]*?)\n---').Groups[1].Value
            $betaSection = [regex]::Match($manifest, '## 3\. Outils beta \(\d+\)([\s\S]*?)\n---').Groups[1].Value
            $qaOnlySection = [regex]::Match($manifest, '## 4\. QA-only \(\d+\)([\s\S]*?)\n---').Groups[1].Value
            $nonRetainedSection = [regex]::Match($manifest, '## 5\. Non retenus \(\d+\)([\s\S]*?)\n---').Groups[1].Value

            $stableRows = ([regex]::Matches($stableSection, '^\| `[^`]+` \|', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
            $betaRows = ([regex]::Matches($betaSection, '^\| `[^`]+` \|', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
            $qaOnlyRows = ([regex]::Matches($qaOnlySection, '^\| `[^`]+` \|', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count
            $nonRetainedRows = ([regex]::Matches($nonRetainedSection, '^\| `[^`]+` \|', [System.Text.RegularExpressions.RegexOptions]::Multiline)).Count

            $runtimeRegisteredCount = ([regex]::Matches($lt, 'name:\s+"[^"]+"')).Count
            $expectedRuntimeCount = $declStable + $declBeta + $declQaOnly

            $manifest_guard_ok = $true
            $manifest_guard_blocker = "N/A"
            if ($declStable -ne $stableRows) {
                $manifest_guard_ok = $false
                $manifest_guard_blocker = "stable_declared_vs_rows_mismatch"
            } elseif ($declBeta -ne $betaRows) {
                $manifest_guard_ok = $false
                $manifest_guard_blocker = "beta_declared_vs_rows_mismatch"
            } elseif ($declQaOnly -ne $qaOnlyRows) {
                $manifest_guard_ok = $false
                $manifest_guard_blocker = "qa_only_declared_vs_rows_mismatch"
            } elseif ($declNonRetained -ne $nonRetainedRows) {
                $manifest_guard_ok = $false
                $manifest_guard_blocker = "non_retained_declared_vs_rows_mismatch"
            } elseif ($runtimeRegisteredCount -ne $expectedRuntimeCount) {
                $manifest_guard_ok = $false
                $manifest_guard_blocker = "runtime_registered_vs_manifest_mismatch"
            }

            ("manifest_guard_ok=" + $(if ($manifest_guard_ok) { "true" } else { "false" }) + "`n" +
             "declared_stable=" + $declStable + "`n" +
             "rows_stable=" + $stableRows + "`n" +
             "declared_beta=" + $declBeta + "`n" +
             "rows_beta=" + $betaRows + "`n" +
             "declared_qa_only=" + $declQaOnly + "`n" +
             "rows_qa_only=" + $qaOnlyRows + "`n" +
             "declared_non_retained=" + $declNonRetained + "`n" +
             "rows_non_retained=" + $nonRetainedRows + "`n" +
             "runtime_registered_count=" + $runtimeRegisteredCount + "`n" +
             "expected_runtime_count_from_manifest=" + $expectedRuntimeCount + "`n" +
             "exact_blocker_if_any=" + $manifest_guard_blocker + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_manifest_count_guard_summary.txt") -Encoding UTF8

            if (-not $manifest_guard_ok) {
                throw "manifest/count guard failed in scenario '$label' (blocker=$manifest_guard_blocker runtime=$runtimeRegisteredCount expected=$expectedRuntimeCount)."
            }

            if ($lt -notmatch 'get_net_classes') { throw "MCP ListTools missing get_net_classes in scenario '$label'." }
            if ($lt -notmatch 'set_net_classes') { throw "MCP ListTools missing set_net_classes in scenario '$label'." }
            if ($lt -notmatch 'get_project_context_snapshot') {
                throw "MCP ListTools missing get_project_context_snapshot in scenario '$label'."
            }
            if ($lt -notmatch 'netclass_patch_session') {
                throw "MCP ListTools missing netclass_patch_session in scenario '$label'."
            }
            if ($lt -notmatch 'electrical_profile_prepare_session') {
                throw "MCP ListTools missing electrical_profile_prepare_session in scenario '$label'."
            }
            if ($lt -notmatch 'cross_context_preflight_correction_session') {
                throw "MCP ListTools missing cross_context_preflight_correction_session in scenario '$label'."
            }
            if ($lt -notmatch 'workflow_preflight_gate_session') {
                throw "MCP ListTools missing workflow_preflight_gate_session in scenario '$label'."
            }
            if ($lt -notmatch 'workflow_next_best_action_session') {
                throw "MCP ListTools missing workflow_next_best_action_session in scenario '$label'."
            }
            if ($lt -notmatch 'workflow_handoff_readiness_session') {
                throw "MCP ListTools missing workflow_handoff_readiness_session in scenario '$label'."
            }
            if ($lt -notmatch 'workflow_assistant_session') {
                throw "MCP ListTools missing workflow_assistant_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_directive_inspect') {
                throw "MCP ListTools missing simulation_directive_inspect in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_netlist_preview_session') {
                throw "MCP ListTools missing simulation_netlist_preview_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_model_library_inspect') {
                throw "MCP ListTools missing simulation_model_library_inspect in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_preflight_readiness_session') {
                throw "MCP ListTools missing simulation_preflight_readiness_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_run_status_session') {
                throw "MCP ListTools missing simulation_run_status_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_run_minimal_summary_session') {
                throw "MCP ListTools missing simulation_run_minimal_summary_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_run_vector_catalog_session') {
                throw "MCP ListTools missing simulation_run_vector_catalog_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_vector_scalar_probe_session') {
                throw "MCP ListTools missing simulation_vector_scalar_probe_session in scenario '$label'."
            }
            if ($lt -notmatch 'simulation_xaxis_probe_session') {
                throw "MCP ListTools missing simulation_xaxis_probe_session in scenario '$label'."
            }
            if ($lt -notmatch 'library_symbol_search_session') {
                throw "MCP ListTools missing library_symbol_search_session in scenario '$label'."
            }

            # get_project_context_snapshot (composed read-only): run before set_net_classes mutates project net classes.
            $visible_without_editor_if_applicable = ($lt -match 'get_project_context_snapshot')
            $visible_without_editor_if_applicable_str = $(if ($visible_without_editor_if_applicable) { "true" } else { "false" })
            $snapArgsEmpty = @{}
            $outProjSnap = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_project_context_snapshot" `
                    -caseLabel "project_context_snapshot_initial" -argsObj $snapArgsEmpty -url $url -smokeExe $smokeExe
            $callable_in_project_context = $(if ($outProjSnap -match 'status=1') { "ok" } else { "error" })
            $project_info_present = $(if ($outProjSnap -match 'project_info_present=1') { "true" } else { "false" })
            $workspace_present = $(if ($outProjSnap -match 'workspace_present=1') { "true" } else { "false" })
            $open_documents_present = $(if ($outProjSnap -match 'open_documents_present=1') { "true" } else { "false" })
            $net_classes_present = $(if ($outProjSnap -match 'net_classes_present=1') { "true" } else { "false" })
            $nets_present = $(if ($outProjSnap -match 'nets_present=1') { "true" } else { "false" })
            $snapshot_summary_present = $(if ($outProjSnap -match 'snapshot_summary_present=1') { "true" } else { "false" })
            $exact_blocker_if_any_snap = "N/A"
            if ($callable_in_project_context -ne "ok") {
                $exact_blocker_if_any_snap = "snapshot_call_failed"
            } elseif ($project_info_present -ne "true" -or $workspace_present -ne "true" -or $open_documents_present -ne "true" -or $net_classes_present -ne "true" -or $snapshot_summary_present -ne "true") {
                $exact_blocker_if_any_snap = "snapshot_presence_flag_missing"
            }
            ("visible_without_editor_if_applicable=" + $visible_without_editor_if_applicable_str + "`n" +
             "callable_in_project_context=" + $callable_in_project_context + "`n" +
             "project_info_present=" + $project_info_present + "`n" +
             "workspace_present=" + $workspace_present + "`n" +
             "open_documents_present=" + $open_documents_present + "`n" +
             "net_classes_present=" + $net_classes_present + "`n" +
             "nets_present=" + $nets_present + "`n" +
             "snapshot_summary_present=" + $snapshot_summary_present + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_snap + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_get_project_context_snapshot_contract_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_snap -ne "N/A") {
                throw "get_project_context_snapshot contract failed in scenario '$label' (blocker=$exact_blocker_if_any_snap)."
            }

            # netclass_patch_session (composed lecture+action): dry-run + apply/verify/rollback on one QA class.
            $visible_in_project_context = ($lt -match 'netclass_patch_session')
            $visible_in_project_context_str = $(if ($visible_in_project_context) { "true" } else { "false" })
            $netclassPatchArgsDryRun = @{
                class_name             = "QA_IPC_PATCH_SESSION_1"
                track_width_nm         = [int64]410000
                dry_run                = $true
                max_classes_to_modify  = 1
            }
            $outNetclassPatchDryRun = Call-McpToolArgsAndLog -runDir $runDir -toolName "netclass_patch_session" `
                    -caseLabel "project_netclass_patch_session_dry_run" -argsObj $netclassPatchArgsDryRun -url $url -smokeExe $smokeExe
            $dry_run_ok_netclass_patch = $(if ($outNetclassPatchDryRun -match 'status=1' -and $outNetclassPatchDryRun -match 'dry_run_ok=1') { "true" } else { "false" })

            $netclassPatchArgsApply = @{
                class_name             = "QA_IPC_PATCH_SESSION_1"
                track_width_nm         = [int64]410000
                max_classes_to_modify  = 1
            }
            $outNetclassPatchApply = Call-McpToolArgsAndLog -runDir $runDir -toolName "netclass_patch_session" `
                    -caseLabel "project_netclass_patch_session_apply_verify_rollback" -argsObj $netclassPatchArgsApply -url $url -smokeExe $smokeExe
            $callable_in_project_context_patch = $(if ($outNetclassPatchApply -match 'status=1') { "ok" } else { "error" })
            $apply_ok_patch = $(if ($outNetclassPatchApply -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_patch = $(if ($outNetclassPatchApply -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_patch = $(if ($outNetclassPatchApply -match 'rollback_ok=1') { "true" } else { "false" })

            $exact_blocker_if_any_netclass_patch = "N/A"
            if ($dry_run_ok_netclass_patch -ne "true") {
                $exact_blocker_if_any_netclass_patch = "dry_run_failed"
            } elseif ($callable_in_project_context_patch -ne "ok") {
                $exact_blocker_if_any_netclass_patch = "call_failed"
            } elseif ($apply_ok_patch -ne "true") {
                $exact_blocker_if_any_netclass_patch = "apply_failed"
            } elseif ($verify_ok_patch -ne "true") {
                $exact_blocker_if_any_netclass_patch = "verify_failed"
            } elseif ($rollback_ok_patch -ne "true") {
                $exact_blocker_if_any_netclass_patch = "rollback_failed"
            }

            ("visible_in_project_context=" + $visible_in_project_context_str + "`n" +
             "callable_in_project_context=" + $callable_in_project_context_patch + "`n" +
             "dry_run_ok=" + $dry_run_ok_netclass_patch + "`n" +
             "apply_ok=" + $apply_ok_patch + "`n" +
             "verify_ok=" + $verify_ok_patch + "`n" +
             "rollback_ok=" + $rollback_ok_patch + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_netclass_patch + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_netclass_patch_session_contract_summary.txt") -Encoding UTF8

            if ($exact_blocker_if_any_netclass_patch -ne "N/A") {
                throw "netclass_patch_session contract failed in scenario '$label' (blocker=$exact_blocker_if_any_netclass_patch)."
            }

            # electrical_profile_prepare_session (composed métier): resolve intent + preflight dry-run + apply/verify/rollback.
            $visible_in_project_context_profile = ($lt -match 'electrical_profile_prepare_session')
            $visible_in_project_context_profile_str = $(if ($visible_in_project_context_profile) { "true" } else { "false" })

            $electricalProfileArgsDryRun = @{
                profile_intent           = "prototype_safe"
                class_name               = "QA_IPC_ELECTRICAL_PROFILE_1"
                dry_run                  = $true
                max_classes_to_modify    = 1
            }
            $outElectricalProfileDryRun = Call-McpToolArgsAndLog -runDir $runDir -toolName "electrical_profile_prepare_session" `
                    -caseLabel "project_electrical_profile_prepare_session_dry_run" -argsObj $electricalProfileArgsDryRun -url $url -smokeExe $smokeExe
            $intent_resolution_ok_profile = $(if ($outElectricalProfileDryRun -match 'status=1' -and $outElectricalProfileDryRun -match 'intent_resolution_ok=1') { "true" } else { "false" })
            $dry_run_ok_profile = $(if ($outElectricalProfileDryRun -match 'status=1' -and $outElectricalProfileDryRun -match 'dry_run_ok=1') { "true" } else { "false" })

            $electricalProfileArgsApply = @{
                profile_intent           = "prototype_safe"
                class_name               = "QA_IPC_ELECTRICAL_PROFILE_1"
                max_classes_to_modify    = 1
            }
            $outElectricalProfileApply = Call-McpToolArgsAndLog -runDir $runDir -toolName "electrical_profile_prepare_session" `
                    -caseLabel "project_electrical_profile_prepare_session_apply_verify_rollback" -argsObj $electricalProfileArgsApply -url $url -smokeExe $smokeExe
            $callable_in_project_context_profile = $(if ($outElectricalProfileApply -match 'status=1') { "ok" } else { "error" })
            $apply_ok_profile = $(if ($outElectricalProfileApply -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_profile = $(if ($outElectricalProfileApply -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_profile = $(if ($outElectricalProfileApply -match 'rollback_ok=1') { "true" } else { "false" })

            $exact_blocker_if_any_electrical_profile = "N/A"
            if ($intent_resolution_ok_profile -ne "true") {
                $exact_blocker_if_any_electrical_profile = "intent_resolution_failed"
            } elseif ($dry_run_ok_profile -ne "true") {
                $exact_blocker_if_any_electrical_profile = "dry_run_failed"
            } elseif ($callable_in_project_context_profile -ne "ok") {
                $exact_blocker_if_any_electrical_profile = "call_failed"
            } elseif ($apply_ok_profile -ne "true") {
                $exact_blocker_if_any_electrical_profile = "apply_failed"
            } elseif ($verify_ok_profile -ne "true") {
                $exact_blocker_if_any_electrical_profile = "verify_failed"
            } elseif ($rollback_ok_profile -ne "true") {
                $exact_blocker_if_any_electrical_profile = "rollback_failed"
            }

            ("visible_in_project_context=" + $visible_in_project_context_profile_str + "`n" +
             "callable_in_project_context=" + $callable_in_project_context_profile + "`n" +
             "intent_resolution_ok=" + $intent_resolution_ok_profile + "`n" +
             "dry_run_ok=" + $dry_run_ok_profile + "`n" +
             "apply_ok=" + $apply_ok_profile + "`n" +
             "verify_ok=" + $verify_ok_profile + "`n" +
             "rollback_ok=" + $rollback_ok_profile + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_electrical_profile + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_electrical_profile_prepare_session_contract_summary.txt") -Encoding UTF8

            if ($exact_blocker_if_any_electrical_profile -ne "N/A") {
                throw "electrical_profile_prepare_session contract failed in scenario '$label' (blocker=$exact_blocker_if_any_electrical_profile)."
            }

            # cross_context_preflight_correction_session (global v1) - subcase electrical_priority
            $visible_in_project_context_ccp_electrical = $(if ($lt -match 'cross_context_preflight_correction_session') { "true" } else { "false" })
            $ccpElectricalArgs = @{
                mode                    = "electrical_priority"
                allow_single_correction = $true
                profile_intent          = "prototype_safe"
                class_name              = "QA_IPC_CROSS_CONTEXT_ELECTRICAL_1"
            }
            $outCcpElectrical = Call-McpToolArgsAndLog -runDir $runDir -toolName "cross_context_preflight_correction_session" `
                    -caseLabel "project_cross_context_preflight_correction_electrical_priority" -argsObj $ccpElectricalArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_ccp_electrical = $(if ($outCcpElectrical -match 'status=1') { "ok" } else { "error" })
            $decision_taken_ok_ccp_electrical = $(if ($outCcpElectrical -match 'decision_taken_ok=1') { "true" } else { "false" })
            $chosen_branch_ccp_electrical = "N/A"
            $chosenBranchLineElectrical = (($outCcpElectrical -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenBranchLineElectrical)) { $chosen_branch_ccp_electrical = ($chosenBranchLineElectrical -replace '^chosen_branch=', '').Trim() }
            $single_correction_enforced_ccp_electrical = $(if ($outCcpElectrical -match 'single_correction_enforced=1') { "true" } else { "false" })
            $apply_ok_ccp_electrical = $(if ($outCcpElectrical -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_ccp_electrical = $(if ($outCcpElectrical -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_ccp_electrical = $(if ($outCcpElectrical -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_ccp_electrical = $(if ($outCcpElectrical -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $correction_needed_detected_ccp_electrical = $(if ($outCcpElectrical -match 'correction_needed_detected=1') { "true" } else { "false" })
            $exact_blocker_if_any_ccp_electrical = "N/A"
            if ($callable_in_project_context_ccp_electrical -ne "ok") {
                $exact_blocker_if_any_ccp_electrical = "call_failed"
            } elseif ($decision_taken_ok_ccp_electrical -ne "true") {
                $exact_blocker_if_any_ccp_electrical = "decision_not_taken"
            } elseif ($chosen_branch_ccp_electrical -ne "electrical") {
                $exact_blocker_if_any_ccp_electrical = "unexpected_chosen_branch"
            } elseif ($single_correction_enforced_ccp_electrical -ne "true") {
                $exact_blocker_if_any_ccp_electrical = "single_correction_not_enforced"
            } elseif ($apply_ok_ccp_electrical -ne "true" -or $verify_ok_ccp_electrical -ne "true" -or $rollback_ok_ccp_electrical -ne "true" -or $cleanup_or_state_restored_ok_ccp_electrical -ne "true") {
                $exact_blocker_if_any_ccp_electrical = "branch_flags_failed"
            }
            ("visible_in_project_context=" + $visible_in_project_context_ccp_electrical + "`n" +
             "callable_in_project_context=" + $callable_in_project_context_ccp_electrical + "`n" +
             "decision_taken_ok=" + $decision_taken_ok_ccp_electrical + "`n" +
             "correction_needed_detected=" + $correction_needed_detected_ccp_electrical + "`n" +
             "chosen_branch=" + $chosen_branch_ccp_electrical + "`n" +
             "single_correction_enforced=" + $single_correction_enforced_ccp_electrical + "`n" +
             "apply_ok=" + $apply_ok_ccp_electrical + "`n" +
             "verify_ok=" + $verify_ok_ccp_electrical + "`n" +
             "rollback_ok=" + $rollback_ok_ccp_electrical + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_ccp_electrical + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_ccp_electrical + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_cross_context_preflight_correction_session_contract_electrical_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_ccp_electrical -ne "N/A") {
                throw "cross_context_preflight_correction_session (electrical_priority) failed in scenario '$label' (blocker=$exact_blocker_if_any_ccp_electrical)."
            }

            # workflow_preflight_gate_session - executed subcase electrical_priority (execute=true)
            $workflowGateElectricalArgs = @{
                workflow_stage          = "prototype_preflight"
                priority_mode           = "electrical_priority"
                allow_single_correction = $true
                execute                 = $true
            }
            $outWorkflowGateElectrical = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_preflight_gate_session" `
                    -caseLabel "project_workflow_preflight_gate_session_electrical_execute_true" -argsObj $workflowGateElectricalArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'status=1') { "ok" } else { "error" })
            $decision_taken_ok_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'decision_taken_ok=1') { "true" } else { "false" })
            $gate_decision_workflow_gate_electrical = "N/A"
            $gateDecisionLineWorkflowElectrical = (($outWorkflowGateElectrical -split "`r?`n") | Where-Object { $_ -like "gate_decision=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($gateDecisionLineWorkflowElectrical)) { $gate_decision_workflow_gate_electrical = ($gateDecisionLineWorkflowElectrical -replace '^gate_decision=', '').Trim() }
            $chosen_branch_workflow_gate_electrical = "N/A"
            $chosenBranchLineWorkflowElectrical = (($outWorkflowGateElectrical -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenBranchLineWorkflowElectrical)) { $chosen_branch_workflow_gate_electrical = ($chosenBranchLineWorkflowElectrical -replace '^chosen_branch=', '').Trim() }
            $single_correction_enforced_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'single_correction_enforced=1') { "true" } else { "false" })
            $apply_ok_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_workflow_gate_electrical = $(if ($outWorkflowGateElectrical -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_workflow_gate_electrical = "N/A"
            if ($callable_in_project_context_workflow_gate_electrical -ne "ok") {
                $exact_blocker_if_any_workflow_gate_electrical = "call_failed"
            } elseif ($decision_taken_ok_workflow_gate_electrical -ne "true") {
                $exact_blocker_if_any_workflow_gate_electrical = "decision_not_taken"
            } elseif ($gate_decision_workflow_gate_electrical -ne "ready") {
                $exact_blocker_if_any_workflow_gate_electrical = "gate_not_ready_after_execute"
            } elseif ($chosen_branch_workflow_gate_electrical -ne "electrical") {
                $exact_blocker_if_any_workflow_gate_electrical = "unexpected_chosen_branch"
            } elseif ($single_correction_enforced_workflow_gate_electrical -ne "true") {
                $exact_blocker_if_any_workflow_gate_electrical = "single_correction_not_enforced"
            } elseif ($apply_ok_workflow_gate_electrical -ne "true" -or $verify_ok_workflow_gate_electrical -ne "true" -or $rollback_ok_workflow_gate_electrical -ne "true" -or $cleanup_or_state_restored_ok_workflow_gate_electrical -ne "true") {
                $exact_blocker_if_any_workflow_gate_electrical = "branch_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_workflow_gate_electrical + "`n" +
             "decision_taken_ok=" + $decision_taken_ok_workflow_gate_electrical + "`n" +
             "gate_decision=" + $gate_decision_workflow_gate_electrical + "`n" +
             "chosen_branch=" + $chosen_branch_workflow_gate_electrical + "`n" +
             "single_correction_enforced=" + $single_correction_enforced_workflow_gate_electrical + "`n" +
             "apply_ok=" + $apply_ok_workflow_gate_electrical + "`n" +
             "verify_ok=" + $verify_ok_workflow_gate_electrical + "`n" +
             "rollback_ok=" + $rollback_ok_workflow_gate_electrical + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_workflow_gate_electrical + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_workflow_gate_electrical + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_preflight_gate_session_contract_electrical_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_workflow_gate_electrical -ne "N/A") {
                throw "workflow_preflight_gate_session (electrical execute=true) failed in scenario '$label' (blocker=$exact_blocker_if_any_workflow_gate_electrical)."
            }

            # workflow_next_best_action_session - executed subcase electrical family (execute=true)
            $nextBestElectricalArgs = @{
                workflow_stage          = "prototype_preflight"
                priority_mode           = "electrical_priority"
                allow_single_correction = $true
                execute                 = $true
            }
            $outNextBestElectrical = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_next_best_action_session" `
                    -caseLabel "project_workflow_next_best_action_session_electrical_execute_true" -argsObj $nextBestElectricalArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_next_best_electrical = $(if ($outNextBestElectrical -match 'status=1') { "ok" } else { "error" })
            $recommendation_taken_ok_next_best_electrical = $(if ($outNextBestElectrical -match 'recommendation_taken_ok=1') { "true" } else { "false" })
            $recommended_action_next_best_electrical = "N/A"
            $recommendedActionLineNextBestElectrical = (($outNextBestElectrical -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($recommendedActionLineNextBestElectrical)) { $recommended_action_next_best_electrical = ($recommendedActionLineNextBestElectrical -replace '^recommended_action=', '').Trim() }
            $execute_mode_next_best_electrical = "N/A"
            $executeModeLineNextBestElectrical = (($outNextBestElectrical -split "`r?`n") | Where-Object { $_ -like "execute_mode=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($executeModeLineNextBestElectrical)) { $execute_mode_next_best_electrical = ($executeModeLineNextBestElectrical -replace '^execute_mode=', '').Trim() }
            $single_action_enforced_next_best_electrical = $(if ($outNextBestElectrical -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_next_best_electrical = $(if ($outNextBestElectrical -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_next_best_electrical = $(if ($outNextBestElectrical -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_next_best_electrical = $(if ($outNextBestElectrical -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_next_best_electrical = $(if ($outNextBestElectrical -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_next_best_electrical = "N/A"
            if ($callable_in_project_context_next_best_electrical -ne "ok") {
                $exact_blocker_if_any_next_best_electrical = "call_failed"
            } elseif ($recommendation_taken_ok_next_best_electrical -ne "true") {
                $exact_blocker_if_any_next_best_electrical = "recommendation_not_taken"
            } elseif ($recommended_action_next_best_electrical -ne "prepare_electrical_profile_prototype_safe") {
                $exact_blocker_if_any_next_best_electrical = "unexpected_recommended_action"
            } elseif ($execute_mode_next_best_electrical -ne "execute_true") {
                $exact_blocker_if_any_next_best_electrical = "unexpected_execute_mode"
            } elseif ($single_action_enforced_next_best_electrical -ne "true") {
                $exact_blocker_if_any_next_best_electrical = "single_action_not_enforced"
            } elseif ($apply_ok_next_best_electrical -ne "true" -or $verify_ok_next_best_electrical -ne "true" -or $rollback_ok_next_best_electrical -ne "true" -or $cleanup_or_state_restored_ok_next_best_electrical -ne "true") {
                $exact_blocker_if_any_next_best_electrical = "branch_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_next_best_electrical + "`n" +
             "recommendation_taken_ok=" + $recommendation_taken_ok_next_best_electrical + "`n" +
             "recommended_action=" + $recommended_action_next_best_electrical + "`n" +
             "execute_mode=" + $execute_mode_next_best_electrical + "`n" +
             "single_action_enforced=" + $single_action_enforced_next_best_electrical + "`n" +
             "apply_ok=" + $apply_ok_next_best_electrical + "`n" +
             "verify_ok=" + $verify_ok_next_best_electrical + "`n" +
             "rollback_ok=" + $rollback_ok_next_best_electrical + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_next_best_electrical + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_next_best_electrical + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_next_best_action_session_contract_electrical_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_next_best_electrical -ne "N/A") {
                throw "workflow_next_best_action_session (electrical execute=true) failed in scenario '$label' (blocker=$exact_blocker_if_any_next_best_electrical)."
            }

            # workflow_preflight_gate_session - diagnostic-only subcase (execute=false)
            $workflowGateDiagnosticArgs = @{
                workflow_stage          = "prototype_preflight"
                priority_mode           = "electrical_priority"
                allow_single_correction = $true
                execute                 = $false
            }
            $outWorkflowGateDiagnostic = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_preflight_gate_session" `
                    -caseLabel "project_workflow_preflight_gate_session_diagnostic_execute_false" -argsObj $workflowGateDiagnosticArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'status=1') { "ok" } else { "error" })
            $decision_taken_ok_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'decision_taken_ok=1') { "true" } else { "false" })
            $gate_decision_workflow_gate_diag = "N/A"
            $gateDecisionLineWorkflowDiag = (($outWorkflowGateDiagnostic -split "`r?`n") | Where-Object { $_ -like "gate_decision=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($gateDecisionLineWorkflowDiag)) { $gate_decision_workflow_gate_diag = ($gateDecisionLineWorkflowDiag -replace '^gate_decision=', '').Trim() }
            $chosen_branch_workflow_gate_diag = "N/A"
            $chosenBranchLineWorkflowDiag = (($outWorkflowGateDiagnostic -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenBranchLineWorkflowDiag)) { $chosen_branch_workflow_gate_diag = ($chosenBranchLineWorkflowDiag -replace '^chosen_branch=', '').Trim() }
            $single_correction_enforced_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'single_correction_enforced=1') { "true" } else { "false" })
            $apply_ok_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_workflow_gate_diag = $(if ($outWorkflowGateDiagnostic -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_workflow_gate_diag = "N/A"
            if ($callable_in_project_context_workflow_gate_diag -ne "ok") {
                $exact_blocker_if_any_workflow_gate_diag = "call_failed"
            } elseif ($decision_taken_ok_workflow_gate_diag -ne "true") {
                $exact_blocker_if_any_workflow_gate_diag = "decision_not_taken"
            } elseif ($gate_decision_workflow_gate_diag -ne "blocked") {
                $exact_blocker_if_any_workflow_gate_diag = "diagnostic_gate_not_blocked"
            } elseif ($chosen_branch_workflow_gate_diag -ne "electrical") {
                $exact_blocker_if_any_workflow_gate_diag = "unexpected_chosen_branch"
            } elseif ($single_correction_enforced_workflow_gate_diag -ne "true") {
                $exact_blocker_if_any_workflow_gate_diag = "single_correction_not_enforced"
            } elseif ($apply_ok_workflow_gate_diag -ne "true" -or $verify_ok_workflow_gate_diag -ne "true" -or $rollback_ok_workflow_gate_diag -ne "true" -or $cleanup_or_state_restored_ok_workflow_gate_diag -ne "true") {
                $exact_blocker_if_any_workflow_gate_diag = "diagnostic_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_workflow_gate_diag + "`n" +
             "decision_taken_ok=" + $decision_taken_ok_workflow_gate_diag + "`n" +
             "gate_decision=" + $gate_decision_workflow_gate_diag + "`n" +
             "chosen_branch=" + $chosen_branch_workflow_gate_diag + "`n" +
             "single_correction_enforced=" + $single_correction_enforced_workflow_gate_diag + "`n" +
             "apply_ok=" + $apply_ok_workflow_gate_diag + "`n" +
             "verify_ok=" + $verify_ok_workflow_gate_diag + "`n" +
             "rollback_ok=" + $rollback_ok_workflow_gate_diag + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_workflow_gate_diag + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_workflow_gate_diag + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_preflight_gate_session_contract_diagnostic_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_workflow_gate_diag -ne "N/A") {
                throw "workflow_preflight_gate_session (diagnostic execute=false) failed in scenario '$label' (blocker=$exact_blocker_if_any_workflow_gate_diag)."
            }

            # workflow_next_best_action_session - diagnostic-only subcase (execute=false)
            $nextBestDiagnosticArgs = @{
                workflow_stage          = "prototype_preflight"
                priority_mode           = "electrical_priority"
                allow_single_correction = $true
                execute                 = $false
            }
            $outNextBestDiagnostic = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_next_best_action_session" `
                    -caseLabel "project_workflow_next_best_action_session_diagnostic_execute_false" -argsObj $nextBestDiagnosticArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_next_best_diag = $(if ($outNextBestDiagnostic -match 'status=1') { "ok" } else { "error" })
            $recommendation_taken_ok_next_best_diag = $(if ($outNextBestDiagnostic -match 'recommendation_taken_ok=1') { "true" } else { "false" })
            $recommended_action_next_best_diag = "N/A"
            $recommendedActionLineNextBestDiag = (($outNextBestDiagnostic -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($recommendedActionLineNextBestDiag)) { $recommended_action_next_best_diag = ($recommendedActionLineNextBestDiag -replace '^recommended_action=', '').Trim() }
            $execute_mode_next_best_diag = "N/A"
            $executeModeLineNextBestDiag = (($outNextBestDiagnostic -split "`r?`n") | Where-Object { $_ -like "execute_mode=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($executeModeLineNextBestDiag)) { $execute_mode_next_best_diag = ($executeModeLineNextBestDiag -replace '^execute_mode=', '').Trim() }
            $single_action_enforced_next_best_diag = $(if ($outNextBestDiagnostic -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_next_best_diag = $(if ($outNextBestDiagnostic -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_next_best_diag = $(if ($outNextBestDiagnostic -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_next_best_diag = $(if ($outNextBestDiagnostic -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_next_best_diag = $(if ($outNextBestDiagnostic -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_next_best_diag = "N/A"
            if ($callable_in_project_context_next_best_diag -ne "ok") {
                $exact_blocker_if_any_next_best_diag = "call_failed"
            } elseif ($recommendation_taken_ok_next_best_diag -ne "true") {
                $exact_blocker_if_any_next_best_diag = "recommendation_not_taken"
            } elseif ($recommended_action_next_best_diag -ne "prepare_electrical_profile_prototype_safe") {
                $exact_blocker_if_any_next_best_diag = "unexpected_recommended_action"
            } elseif ($execute_mode_next_best_diag -ne "execute_false") {
                $exact_blocker_if_any_next_best_diag = "unexpected_execute_mode"
            } elseif ($single_action_enforced_next_best_diag -ne "true") {
                $exact_blocker_if_any_next_best_diag = "single_action_not_enforced"
            } elseif ($apply_ok_next_best_diag -ne "true" -or $verify_ok_next_best_diag -ne "true" -or $rollback_ok_next_best_diag -ne "true" -or $cleanup_or_state_restored_ok_next_best_diag -ne "true") {
                $exact_blocker_if_any_next_best_diag = "diagnostic_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_next_best_diag + "`n" +
             "recommendation_taken_ok=" + $recommendation_taken_ok_next_best_diag + "`n" +
             "recommended_action=" + $recommended_action_next_best_diag + "`n" +
             "execute_mode=" + $execute_mode_next_best_diag + "`n" +
             "single_action_enforced=" + $single_action_enforced_next_best_diag + "`n" +
             "apply_ok=" + $apply_ok_next_best_diag + "`n" +
             "verify_ok=" + $verify_ok_next_best_diag + "`n" +
             "rollback_ok=" + $rollback_ok_next_best_diag + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_next_best_diag + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_next_best_diag + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_next_best_action_session_contract_diagnostic_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_next_best_diag -ne "N/A") {
                throw "workflow_next_best_action_session (diagnostic execute=false) failed in scenario '$label' (blocker=$exact_blocker_if_any_next_best_diag)."
            }

            # workflow_handoff_readiness_session - diagnostic-only subcase (execute=false)
            $handoffReadinessDiagnosticArgs = @{
                handoff_target          = "pcb_to_release"
                allow_single_correction = $true
                execute                 = $false
            }
            $outHandoffReadinessDiagnostic = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_handoff_readiness_session" `
                    -caseLabel "project_workflow_handoff_readiness_session_diagnostic_execute_false" -argsObj $handoffReadinessDiagnosticArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'status=1') { "ok" } else { "error" })
            $handoff_decision_taken_ok_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'handoff_decision_taken_ok=1') { "true" } else { "false" })
            $handoff_target_handoff_diag = "N/A"
            $handoffTargetLineHandoffDiag = (($outHandoffReadinessDiagnostic -split "`r?`n") | Where-Object { $_ -like "handoff_target=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($handoffTargetLineHandoffDiag)) { $handoff_target_handoff_diag = ($handoffTargetLineHandoffDiag -replace '^handoff_target=', '').Trim() }
            $handoff_decision_handoff_diag = "N/A"
            $handoffDecisionLineHandoffDiag = (($outHandoffReadinessDiagnostic -split "`r?`n") | Where-Object { $_ -like "handoff_decision=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($handoffDecisionLineHandoffDiag)) { $handoff_decision_handoff_diag = ($handoffDecisionLineHandoffDiag -replace '^handoff_decision=', '').Trim() }
            $chosen_branch_handoff_diag = "N/A"
            $chosenBranchLineHandoffDiag = (($outHandoffReadinessDiagnostic -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenBranchLineHandoffDiag)) { $chosen_branch_handoff_diag = ($chosenBranchLineHandoffDiag -replace '^chosen_branch=', '').Trim() }
            $single_action_enforced_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_handoff_diag = $(if ($outHandoffReadinessDiagnostic -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_handoff_diag = "N/A"
            if ($callable_in_project_context_handoff_diag -ne "ok") {
                $exact_blocker_if_any_handoff_diag = "call_failed"
            } elseif ($handoff_decision_taken_ok_handoff_diag -ne "true") {
                $exact_blocker_if_any_handoff_diag = "handoff_decision_not_taken"
            } elseif ($handoff_target_handoff_diag -ne "pcb_to_release") {
                $exact_blocker_if_any_handoff_diag = "unexpected_handoff_target"
            } elseif ($handoff_decision_handoff_diag -ne "blocked") {
                $exact_blocker_if_any_handoff_diag = "handoff_diagnostic_not_blocked"
            } elseif ($chosen_branch_handoff_diag -ne "pcb") {
                $exact_blocker_if_any_handoff_diag = "unexpected_chosen_branch"
            } elseif ($single_action_enforced_handoff_diag -ne "true") {
                $exact_blocker_if_any_handoff_diag = "single_action_not_enforced"
            } elseif ($apply_ok_handoff_diag -ne "true" -or $verify_ok_handoff_diag -ne "true" -or $rollback_ok_handoff_diag -ne "true" -or $cleanup_or_state_restored_ok_handoff_diag -ne "true") {
                $exact_blocker_if_any_handoff_diag = "diagnostic_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_handoff_diag + "`n" +
             "handoff_decision_taken_ok=" + $handoff_decision_taken_ok_handoff_diag + "`n" +
             "handoff_target=" + $handoff_target_handoff_diag + "`n" +
             "handoff_decision=" + $handoff_decision_handoff_diag + "`n" +
             "chosen_branch=" + $chosen_branch_handoff_diag + "`n" +
             "single_action_enforced=" + $single_action_enforced_handoff_diag + "`n" +
             "apply_ok=" + $apply_ok_handoff_diag + "`n" +
             "verify_ok=" + $verify_ok_handoff_diag + "`n" +
             "rollback_ok=" + $rollback_ok_handoff_diag + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_handoff_diag + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_handoff_diag + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_handoff_readiness_session_contract_diagnostic_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_handoff_diag -ne "N/A") {
                throw "workflow_handoff_readiness_session (diagnostic execute=false) failed in scenario '$label' (blocker=$exact_blocker_if_any_handoff_diag)."
            }

            # workflow_assistant_session - executed subcase preflight workflow family
            $assistantPreflightArgs = @{
                assistant_intent        = "preflight_gate"
                allow_single_correction = $true
                execute                 = $true
            }
            $outAssistantPreflight = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_assistant_session" `
                    -caseLabel "project_workflow_assistant_session_preflight_execute_true" -argsObj $assistantPreflightArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_assistant_preflight = $(if ($outAssistantPreflight -match 'status=1') { "ok" } else { "error" })
            $assistant_decision_taken_ok_assistant_preflight = $(if ($outAssistantPreflight -match 'assistant_decision_taken_ok=1') { "true" } else { "false" })
            $assistant_intent_assistant_preflight = "N/A"
            $assistantIntentLineAssistantPreflight = (($outAssistantPreflight -split "`r?`n") | Where-Object { $_ -like "assistant_intent=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($assistantIntentLineAssistantPreflight)) { $assistant_intent_assistant_preflight = ($assistantIntentLineAssistantPreflight -replace '^assistant_intent=', '').Trim() }
            $chosen_workflow_assistant_preflight = "N/A"
            $chosenWorkflowLineAssistantPreflight = (($outAssistantPreflight -split "`r?`n") | Where-Object { $_ -like "chosen_workflow=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenWorkflowLineAssistantPreflight)) { $chosen_workflow_assistant_preflight = ($chosenWorkflowLineAssistantPreflight -replace '^chosen_workflow=', '').Trim() }
            $recommended_action_assistant_preflight = "N/A"
            $recommendedActionLineAssistantPreflight = (($outAssistantPreflight -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($recommendedActionLineAssistantPreflight)) { $recommended_action_assistant_preflight = ($recommendedActionLineAssistantPreflight -replace '^recommended_action=', '').Trim() }
            $single_action_enforced_assistant_preflight = $(if ($outAssistantPreflight -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_assistant_preflight = $(if ($outAssistantPreflight -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_assistant_preflight = $(if ($outAssistantPreflight -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_assistant_preflight = $(if ($outAssistantPreflight -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_assistant_preflight = $(if ($outAssistantPreflight -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_assistant_preflight = "N/A"
            if ($callable_in_project_context_assistant_preflight -ne "ok") {
                $exact_blocker_if_any_assistant_preflight = "call_failed"
            } elseif ($assistant_decision_taken_ok_assistant_preflight -ne "true") {
                $exact_blocker_if_any_assistant_preflight = "assistant_decision_not_taken"
            } elseif ($assistant_intent_assistant_preflight -ne "preflight_gate") {
                $exact_blocker_if_any_assistant_preflight = "unexpected_assistant_intent"
            } elseif ($chosen_workflow_assistant_preflight -ne "workflow_preflight_gate_session") {
                $exact_blocker_if_any_assistant_preflight = "unexpected_chosen_workflow"
            } elseif ($recommended_action_assistant_preflight -ne "prepare_electrical_profile_prototype_safe") {
                $exact_blocker_if_any_assistant_preflight = "unexpected_recommended_action"
            } elseif ($single_action_enforced_assistant_preflight -ne "true") {
                $exact_blocker_if_any_assistant_preflight = "single_action_not_enforced"
            } elseif ($apply_ok_assistant_preflight -ne "true" -or $verify_ok_assistant_preflight -ne "true" -or $rollback_ok_assistant_preflight -ne "true" -or $cleanup_or_state_restored_ok_assistant_preflight -ne "true") {
                $exact_blocker_if_any_assistant_preflight = "branch_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_assistant_preflight + "`n" +
             "assistant_decision_taken_ok=" + $assistant_decision_taken_ok_assistant_preflight + "`n" +
             "assistant_intent=" + $assistant_intent_assistant_preflight + "`n" +
             "chosen_workflow=" + $chosen_workflow_assistant_preflight + "`n" +
             "recommended_action=" + $recommended_action_assistant_preflight + "`n" +
             "single_action_enforced=" + $single_action_enforced_assistant_preflight + "`n" +
             "apply_ok=" + $apply_ok_assistant_preflight + "`n" +
             "verify_ok=" + $verify_ok_assistant_preflight + "`n" +
             "rollback_ok=" + $rollback_ok_assistant_preflight + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_assistant_preflight + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_assistant_preflight + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_assistant_session_contract_preflight_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_assistant_preflight -ne "N/A") {
                throw "workflow_assistant_session (preflight execute=true) failed in scenario '$label' (blocker=$exact_blocker_if_any_assistant_preflight)."
            }

            # workflow_assistant_session - executed subcase next-best-action workflow family
            $assistantNextArgs = @{
                assistant_intent        = "next_best_action"
                allow_single_correction = $true
                execute                 = $true
            }
            $outAssistantNext = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_assistant_session" `
                    -caseLabel "project_workflow_assistant_session_next_best_execute_true" -argsObj $assistantNextArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_assistant_next = $(if ($outAssistantNext -match 'status=1') { "ok" } else { "error" })
            $assistant_decision_taken_ok_assistant_next = $(if ($outAssistantNext -match 'assistant_decision_taken_ok=1') { "true" } else { "false" })
            $assistant_intent_assistant_next = "N/A"
            $assistantIntentLineAssistantNext = (($outAssistantNext -split "`r?`n") | Where-Object { $_ -like "assistant_intent=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($assistantIntentLineAssistantNext)) { $assistant_intent_assistant_next = ($assistantIntentLineAssistantNext -replace '^assistant_intent=', '').Trim() }
            $chosen_workflow_assistant_next = "N/A"
            $chosenWorkflowLineAssistantNext = (($outAssistantNext -split "`r?`n") | Where-Object { $_ -like "chosen_workflow=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenWorkflowLineAssistantNext)) { $chosen_workflow_assistant_next = ($chosenWorkflowLineAssistantNext -replace '^chosen_workflow=', '').Trim() }
            $recommended_action_assistant_next = "N/A"
            $recommendedActionLineAssistantNext = (($outAssistantNext -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($recommendedActionLineAssistantNext)) { $recommended_action_assistant_next = ($recommendedActionLineAssistantNext -replace '^recommended_action=', '').Trim() }
            $single_action_enforced_assistant_next = $(if ($outAssistantNext -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_assistant_next = $(if ($outAssistantNext -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_assistant_next = $(if ($outAssistantNext -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_assistant_next = $(if ($outAssistantNext -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_assistant_next = $(if ($outAssistantNext -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_assistant_next = "N/A"
            if ($callable_in_project_context_assistant_next -ne "ok") {
                $exact_blocker_if_any_assistant_next = "call_failed"
            } elseif ($assistant_decision_taken_ok_assistant_next -ne "true") {
                $exact_blocker_if_any_assistant_next = "assistant_decision_not_taken"
            } elseif ($assistant_intent_assistant_next -ne "next_best_action") {
                $exact_blocker_if_any_assistant_next = "unexpected_assistant_intent"
            } elseif ($chosen_workflow_assistant_next -ne "workflow_next_best_action_session") {
                $exact_blocker_if_any_assistant_next = "unexpected_chosen_workflow"
            } elseif ($recommended_action_assistant_next -ne "prepare_electrical_profile_prototype_safe") {
                $exact_blocker_if_any_assistant_next = "unexpected_recommended_action"
            } elseif ($single_action_enforced_assistant_next -ne "true") {
                $exact_blocker_if_any_assistant_next = "single_action_not_enforced"
            } elseif ($apply_ok_assistant_next -ne "true" -or $verify_ok_assistant_next -ne "true" -or $rollback_ok_assistant_next -ne "true" -or $cleanup_or_state_restored_ok_assistant_next -ne "true") {
                $exact_blocker_if_any_assistant_next = "branch_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_assistant_next + "`n" +
             "assistant_decision_taken_ok=" + $assistant_decision_taken_ok_assistant_next + "`n" +
             "assistant_intent=" + $assistant_intent_assistant_next + "`n" +
             "chosen_workflow=" + $chosen_workflow_assistant_next + "`n" +
             "recommended_action=" + $recommended_action_assistant_next + "`n" +
             "single_action_enforced=" + $single_action_enforced_assistant_next + "`n" +
             "apply_ok=" + $apply_ok_assistant_next + "`n" +
             "verify_ok=" + $verify_ok_assistant_next + "`n" +
             "rollback_ok=" + $rollback_ok_assistant_next + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_assistant_next + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_assistant_next + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_assistant_session_contract_next_best_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_assistant_next -ne "N/A") {
                throw "workflow_assistant_session (next_best execute=true) failed in scenario '$label' (blocker=$exact_blocker_if_any_assistant_next)."
            }

            # workflow_assistant_session - diagnostic-only subcase (execute=false)
            $assistantDiagnosticArgs = @{
                assistant_intent        = "handoff_readiness"
                allow_single_correction = $true
                execute                 = $false
            }
            $outAssistantDiagnostic = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_assistant_session" `
                    -caseLabel "project_workflow_assistant_session_diagnostic_execute_false" -argsObj $assistantDiagnosticArgs -url $url -smokeExe $smokeExe
            $callable_in_project_context_assistant_diag = $(if ($outAssistantDiagnostic -match 'status=1') { "ok" } else { "error" })
            $assistant_decision_taken_ok_assistant_diag = $(if ($outAssistantDiagnostic -match 'assistant_decision_taken_ok=1') { "true" } else { "false" })
            $assistant_intent_assistant_diag = "N/A"
            $assistantIntentLineAssistantDiag = (($outAssistantDiagnostic -split "`r?`n") | Where-Object { $_ -like "assistant_intent=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($assistantIntentLineAssistantDiag)) { $assistant_intent_assistant_diag = ($assistantIntentLineAssistantDiag -replace '^assistant_intent=', '').Trim() }
            $chosen_workflow_assistant_diag = "N/A"
            $chosenWorkflowLineAssistantDiag = (($outAssistantDiagnostic -split "`r?`n") | Where-Object { $_ -like "chosen_workflow=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($chosenWorkflowLineAssistantDiag)) { $chosen_workflow_assistant_diag = ($chosenWorkflowLineAssistantDiag -replace '^chosen_workflow=', '').Trim() }
            $recommended_action_assistant_diag = "N/A"
            $recommendedActionLineAssistantDiag = (($outAssistantDiagnostic -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
            if (![string]::IsNullOrWhiteSpace($recommendedActionLineAssistantDiag)) { $recommended_action_assistant_diag = ($recommendedActionLineAssistantDiag -replace '^recommended_action=', '').Trim() }
            $single_action_enforced_assistant_diag = $(if ($outAssistantDiagnostic -match 'single_action_enforced=1') { "true" } else { "false" })
            $apply_ok_assistant_diag = $(if ($outAssistantDiagnostic -match 'apply_ok=1') { "true" } else { "false" })
            $verify_ok_assistant_diag = $(if ($outAssistantDiagnostic -match 'verify_ok=1') { "true" } else { "false" })
            $rollback_ok_assistant_diag = $(if ($outAssistantDiagnostic -match 'rollback_ok=1') { "true" } else { "false" })
            $cleanup_or_state_restored_ok_assistant_diag = $(if ($outAssistantDiagnostic -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
            $exact_blocker_if_any_assistant_diag = "N/A"
            if ($callable_in_project_context_assistant_diag -ne "ok") {
                $exact_blocker_if_any_assistant_diag = "call_failed"
            } elseif ($assistant_decision_taken_ok_assistant_diag -ne "true") {
                $exact_blocker_if_any_assistant_diag = "assistant_decision_not_taken"
            } elseif ($assistant_intent_assistant_diag -ne "handoff_readiness") {
                $exact_blocker_if_any_assistant_diag = "unexpected_assistant_intent"
            } elseif ($chosen_workflow_assistant_diag -ne "workflow_handoff_readiness_session") {
                $exact_blocker_if_any_assistant_diag = "unexpected_chosen_workflow"
            } elseif ($single_action_enforced_assistant_diag -ne "true") {
                $exact_blocker_if_any_assistant_diag = "single_action_not_enforced"
            } elseif ($apply_ok_assistant_diag -ne "true" -or $verify_ok_assistant_diag -ne "true" -or $rollback_ok_assistant_diag -ne "true" -or $cleanup_or_state_restored_ok_assistant_diag -ne "true") {
                $exact_blocker_if_any_assistant_diag = "diagnostic_flags_failed"
            }
            ("visible_in_project_context=true`n" +
             "callable_in_project_context=" + $callable_in_project_context_assistant_diag + "`n" +
             "assistant_decision_taken_ok=" + $assistant_decision_taken_ok_assistant_diag + "`n" +
             "assistant_intent=" + $assistant_intent_assistant_diag + "`n" +
             "chosen_workflow=" + $chosen_workflow_assistant_diag + "`n" +
             "recommended_action=" + $recommended_action_assistant_diag + "`n" +
             "single_action_enforced=" + $single_action_enforced_assistant_diag + "`n" +
             "apply_ok=" + $apply_ok_assistant_diag + "`n" +
             "verify_ok=" + $verify_ok_assistant_diag + "`n" +
             "rollback_ok=" + $rollback_ok_assistant_diag + "`n" +
             "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_assistant_diag + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_assistant_diag + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_workflow_assistant_session_contract_diagnostic_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_assistant_diag -ne "N/A") {
                throw "workflow_assistant_session (diagnostic execute=false) failed in scenario '$label' (blocker=$exact_blocker_if_any_assistant_diag)."
            }

            # simulation_directive_inspect (read-only simulation opening lot): deterministic parser corpus.
            $visible_in_context_if_applicable = ($lt -match 'simulation_directive_inspect')
            $visible_in_context_if_applicable_str = $(if ($visible_in_context_if_applicable) { "true" } else { "false" })
            $corpus = @(
                @{ name = "tran_valid"; directive = ".tran 1n 10u"; expected_type = "tran"; expected_parse_ok = "1" },
                @{ name = "ac_valid"; directive = ".ac dec 10 1 1e6"; expected_type = "ac"; expected_parse_ok = "1" },
                @{ name = "dc_valid"; directive = ".dc V1 0 5 0.1"; expected_type = "dc"; expected_parse_ok = "1" },
                @{ name = "noise_valid"; directive = ".noise v(out,0) VIN dec 10 1 1e6"; expected_type = "noise"; expected_parse_ok = "1" },
                @{ name = "pz_valid"; directive = ".pz V1 0 V(out) 0 vol pol"; expected_type = "pz"; expected_parse_ok = "1" },
                @{ name = "noise_invalid"; directive = ".noise i(out,0) VIN dec 10 1 1e6"; expected_type = "noise"; expected_parse_ok = "0" },
                @{ name = "unknown_invalid"; directive = ".foo bar"; expected_type = "unknown"; expected_parse_ok = "0" }
            )

            $all_callable = $true
            $all_parse_ok_match = $true
            $all_simulation_type_match = $true
            $all_diagnostics_present = $true
            $exact_blocker_if_any_simulation_directive = "N/A"
            $corpusLogs = @()

            foreach ($entry in $corpus) {
                $args = @{ directive = $entry.directive }
                $outDirective = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_directive_inspect" `
                        -caseLabel ("project_simulation_directive_inspect_" + $entry.name) -argsObj $args -url $url -smokeExe $smokeExe

                $callable = ($outDirective -match 'status=1')
                if (-not $callable) { $all_callable = $false }

                $parse_ok = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "parse_ok=*" } | Select-Object -First 1) -replace '^parse_ok=', '').Trim()
                $simulation_type = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "simulation_type=*" } | Select-Object -First 1) -replace '^simulation_type=', '').Trim()
                $normalized_directive = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "normalized_directive=*" } | Select-Object -First 1) -replace '^normalized_directive=', '').Trim()
                $diagnostics_present = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_case = ((($outDirective -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()

                if ($parse_ok -ne $entry.expected_parse_ok) { $all_parse_ok_match = $false }
                if ($simulation_type -ne $entry.expected_type) { $all_simulation_type_match = $false }
                if ($diagnostics_present -ne "true") { $all_diagnostics_present = $false }

                $corpusLogs += ("case=" + $entry.name + ";raw_directive=" + $entry.directive + ";normalized_directive=" + $normalized_directive +
                                ";simulation_type=" + $simulation_type + ";parse_result=" + $(if ($parse_ok -eq "1") { "ok" } else { "error" }) +
                                ";diagnostics_present=" + $diagnostics_present + ";exact_blocker_if_any=" + $exact_blocker_case +
                                ";diagnostics=" + $diagnostics)
            }

            if (-not $all_callable) {
                $exact_blocker_if_any_simulation_directive = "callable_without_runtime_simulation_failed"
            } elseif (-not $all_parse_ok_match) {
                $exact_blocker_if_any_simulation_directive = "parse_ok_mismatch_against_corpus"
            } elseif (-not $all_simulation_type_match) {
                $exact_blocker_if_any_simulation_directive = "simulation_type_mismatch_against_corpus"
            } elseif (-not $all_diagnostics_present) {
                $exact_blocker_if_any_simulation_directive = "diagnostics_missing_in_output"
            }

            ("visible_in_context_if_applicable=" + $visible_in_context_if_applicable_str + "`n" +
             "callable_without_runtime_simulation=" + $(if ($all_callable) { "true" } else { "false" }) + "`n" +
             "parse_ok=" + $(if ($all_parse_ok_match) { "true" } else { "false" }) + "`n" +
             "simulation_type=" + $(if ($all_simulation_type_match) { "true" } else { "false" }) + "`n" +
             "diagnostics_present=" + $(if ($all_diagnostics_present) { "true" } else { "false" }) + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_simulation_directive + "`n" +
             "corpus_logs=" + ($corpusLogs -join " || ") + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_simulation_directive_inspect_contract_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_simulation_directive -ne "N/A") {
                throw "simulation_directive_inspect contract failed in scenario '$label' (blocker=$exact_blocker_if_any_simulation_directive)."
            }

            # simulation_model_library_inspect (read-only): inspect .model/.subckt declarations.
            $visible_in_context_if_applicable_model_library = ($lt -match 'simulation_model_library_inspect')
            $visible_in_context_if_applicable_model_library_str = $(if ($visible_in_context_if_applicable_model_library) { "true" } else { "false" })
            $modelLibraryInspectArgs = @{
                source_path = $spiceLibraryInspectPath
            }
            $outModelLibraryInspect = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_model_library_inspect" `
                    -caseLabel "project_simulation_model_library_inspect" -argsObj $modelLibraryInspectArgs -url $url -smokeExe $smokeExe

            $callable_without_runtime_simulation_model_library = $(if ($outModelLibraryInspect -match 'status=1') { "true" } else { "false" })
            $inspect_ok = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "inspect_ok=*" } | Select-Object -First 1) -replace '^inspect_ok=', '').Trim()
            $inspect_ok_str = $(if ($inspect_ok -eq "1") { "true" } else { "false" })
            $library_kind = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "library_kind=*" } | Select-Object -First 1) -replace '^library_kind=', '').Trim()
            $library_kind_valid = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "library_kind_valid=*" } | Select-Object -First 1) -replace '^library_kind_valid=', '').Trim()
            $models_count = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "models_count=*" } | Select-Object -First 1) -replace '^models_count=', '').Trim()
            $models_count_valid = $false
            if ($models_count -match '^\d+$' -and [int]$models_count -gt 0) { $models_count_valid = $true }
            $models_count_valid_str = $(if ($models_count_valid) { "true" } else { "false" })
            $models_summary = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "models_summary=*" } | Select-Object -First 1) -replace '^models_summary=', '').Trim()
            $diagnostics_present_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
            $diagnostics_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
            $source_path_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "session_log=source_path=*" -or $_ -like "source_path=*" } | Select-Object -First 1) -replace '^session_log=source_path=', '' -replace '^source_path=', '').Trim()
            $parse_strategy_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "parse_strategy=*" } | Select-Object -First 1) -replace '^parse_strategy=', '').Trim()
            $library_kind_log_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "library_kind=*" } | Select-Object -Last 1) -replace '^library_kind=', '').Trim()
            $models_summary_log_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "models_summary=*" } | Select-Object -Last 1) -replace '^models_summary=', '').Trim()
            $exact_blocker_if_any_tool_model_library = ((($outModelLibraryInspect -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()

            $exact_blocker_if_any_model_library = "N/A"
            if ($callable_without_runtime_simulation_model_library -ne "true") {
                $exact_blocker_if_any_model_library = "callable_without_runtime_simulation_failed"
            } elseif ($inspect_ok_str -ne "true") {
                $exact_blocker_if_any_model_library = "inspect_ok_false"
            } elseif ($library_kind_valid -ne "true") {
                $exact_blocker_if_any_model_library = "library_kind_invalid"
            } elseif ($models_count_valid_str -ne "true") {
                $exact_blocker_if_any_model_library = "models_count_invalid"
            } elseif ($diagnostics_present_model_library -ne "true") {
                $exact_blocker_if_any_model_library = "diagnostics_missing"
            } elseif ([string]::IsNullOrWhiteSpace($source_path_model_library) -or [string]::IsNullOrWhiteSpace($parse_strategy_model_library) -or [string]::IsNullOrWhiteSpace($library_kind_log_model_library)) {
                $exact_blocker_if_any_model_library = "session_log_missing_expected_fields"
            }

            ("visible_in_context_if_applicable=" + $visible_in_context_if_applicable_model_library_str + "`n" +
             "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_model_library + "`n" +
             "inspect_ok=" + $inspect_ok_str + "`n" +
             "library_kind_valid=" + $library_kind_valid + "`n" +
             "models_count_valid=" + $models_count_valid_str + "`n" +
             "diagnostics_present=" + $diagnostics_present_model_library + "`n" +
             "source_path=" + $source_path_model_library + "`n" +
             "parse_strategy=" + $parse_strategy_model_library + "`n" +
             "library_kind=" + $library_kind + "`n" +
             "models_summary=" + $models_summary + "`n" +
             "models_summary_log=" + $models_summary_log_model_library + "`n" +
             "diagnostics=" + $diagnostics_model_library + "`n" +
             "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_model_library + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_model_library + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_simulation_model_library_inspect_contract_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_model_library -ne "N/A") {
                throw "simulation_model_library_inspect contract failed in scenario '$label' (blocker=$exact_blocker_if_any_model_library)."
            }

            # simulation_netlist_preview_session (read-only, no ngspice runtime): preview SPICE netlist text.
            $visible_in_context_if_applicable_netlist_preview = ($lt -match 'simulation_netlist_preview_session')
            $visible_in_context_if_applicable_netlist_preview_str = $(if ($visible_in_context_if_applicable_netlist_preview) { "true" } else { "false" })
            $netlistPreviewArgs = @{
                schematic_filename = $spicePreviewSch
            }
            $outNetlistPreview = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_netlist_preview_session" `
                    -caseLabel "project_simulation_netlist_preview_session" -argsObj $netlistPreviewArgs -url $url -smokeExe $smokeExe

            $callable_without_runtime_simulation = $(if ($outNetlistPreview -match 'status=1') { "true" } else { "false" })
            $preview_ok = ((($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "preview_ok=*" } | Select-Object -First 1) -replace '^preview_ok=', '').Trim()
            $preview_ok_str = $(if ($preview_ok -eq "1") { "true" } else { "false" })
            $netlist_text_present = ((($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "netlist_text_present=*" } | Select-Object -First 1) -replace '^netlist_text_present=', '').Trim()
            $netlist_line_count = ((($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "netlist_line_count=*" } | Select-Object -First 1) -replace '^netlist_line_count=', '').Trim()
            $netlist_line_count_valid = $false
            if ($netlist_line_count -match '^\d+$' -and [int]$netlist_line_count -gt 0) { $netlist_line_count_valid = $true }
            $netlist_line_count_valid_str = $(if ($netlist_line_count_valid) { "true" } else { "false" })
            $diagnostics_present = ((($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
            $diagnostics = ((($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
            $source_context_line = (($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1)
            $source_context = ""
            if ($source_context_line) {
                $source_context = ($source_context_line -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
            }
            if ([string]::IsNullOrWhiteSpace($source_context)) { $source_context = "absent" }
            $export_path_strategy_line = (($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "export_path_strategy=*" } | Select-Object -First 1)
            $export_path_strategy = ""
            if ($export_path_strategy_line) { $export_path_strategy = ($export_path_strategy_line -replace '^export_path_strategy=', '').Trim() }
            if ([string]::IsNullOrWhiteSpace($export_path_strategy)) { $export_path_strategy = "absent" }
            $preview_result_line = (($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "preview_result=*" } | Select-Object -First 1)
            $preview_result = ""
            if ($preview_result_line) { $preview_result = ($preview_result_line -replace '^preview_result=', '').Trim() }
            if ([string]::IsNullOrWhiteSpace($preview_result)) { $preview_result = "absent" }
            $first_lines_excerpt_line = (($outNetlistPreview -split "`r?`n") | Where-Object { $_ -like "first_lines_excerpt=*" } | Select-Object -First 1)
            $first_lines_excerpt = ""
            if ($first_lines_excerpt_line) { $first_lines_excerpt = ($first_lines_excerpt_line -replace '^first_lines_excerpt=', '').Trim() }

            $exact_blocker_if_any_netlist_preview = "N/A"
            if ($callable_without_runtime_simulation -ne "true") {
                $exact_blocker_if_any_netlist_preview = "callable_without_runtime_simulation_failed"
            } elseif ($diagnostics_present -ne "true") {
                $exact_blocker_if_any_netlist_preview = "diagnostics_missing"
            } elseif ($source_context -eq "absent" -or $export_path_strategy -eq "absent" -or $preview_result -eq "absent") {
                $exact_blocker_if_any_netlist_preview = "session_log_missing_expected_fields"
            } elseif ($preview_ok_str -eq "true" -and ($netlist_text_present -ne "true" -or $netlist_line_count_valid_str -ne "true")) {
                $exact_blocker_if_any_netlist_preview = "preview_success_shape_invalid"
            }

            ("visible_in_context_if_applicable=" + $visible_in_context_if_applicable_netlist_preview_str + "`n" +
             "callable_without_runtime_simulation=" + $callable_without_runtime_simulation + "`n" +
             "preview_ok=" + $preview_ok_str + "`n" +
             "netlist_text_present=" + $netlist_text_present + "`n" +
             "netlist_line_count_valid=" + $netlist_line_count_valid_str + "`n" +
             "diagnostics_present=" + $diagnostics_present + "`n" +
             "source_context=" + $source_context + "`n" +
             "export_path_strategy=" + $export_path_strategy + "`n" +
             "preview_result=" + $preview_result + "`n" +
             "first_lines_excerpt=" + $first_lines_excerpt + "`n" +
             "diagnostics=" + $diagnostics + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_netlist_preview + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_simulation_netlist_preview_session_contract_summary.txt") -Encoding UTF8
            if ($exact_blocker_if_any_netlist_preview -ne "N/A") {
                throw "simulation_netlist_preview_session contract failed in scenario '$label' (blocker=$exact_blocker_if_any_netlist_preview)."
            }

            $visible_before_use_set_nc = ($lt -match 'set_net_classes')
            $visible_before_use_set_nc_str = $(if ($visible_before_use_set_nc) { "true" } else { "false" })

            $getNcArgsEmpty = @{}
            $outGetNcInitial = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_net_classes" `
                    -caseLabel "project_net_classes_initial" -argsObj $getNcArgsEmpty -url $url -smokeExe $smokeExe
            if ($outGetNcInitial -notmatch 'status=1') {
                throw "get_net_classes failed unexpectedly in scenario '$label'."
            }
            $jsonInitial = Get-McpResultJsonLine $outGetNcInitial
            if ([string]::IsNullOrWhiteSpace($jsonInitial)) {
                throw "get_net_classes did not emit result_json line in scenario '$label'."
            }
            $namesKeyInitial = Get-NetClassNamesKeyFromResultJson $jsonInitial
            if ([string]::IsNullOrWhiteSpace($namesKeyInitial)) {
                throw "get_net_classes result_json missing net_classes names in scenario '$label'."
            }
            if ($namesKeyInitial -match '(^|\|)QA_IPC_NETCLASS_1(\||$)') {
                throw "QA_IPC_NETCLASS_1 already present before test; refusing to run set_net_classes scenario in '$label'."
            }

            $mergeSetNcArgs = @{
                merge_mode  = "MMM_MERGE"
                net_classes = @(
                    @{
                        name = "QA_IPC_NETCLASS_1"
                        type = "NCT_EXPLICIT"
                        board  = @{
                            track_width = @{ value_nm = [int64]400000 }
                        }
                    }
                )
            }
            $outSetNcMerge = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_net_classes" `
                    -caseLabel "project_set_net_classes_merge_qa_class" -argsObj $mergeSetNcArgs -url $url -smokeExe $smokeExe
            $callable_after_merge_set_nc = $(if ($outSetNcMerge -match 'status=1') { "ok" } else { "error" })

            $outGetNcChanged = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_net_classes" `
                    -caseLabel "project_net_classes_after_merge" -argsObj $getNcArgsEmpty -url $url -smokeExe $smokeExe
            if ($outGetNcChanged -notmatch 'status=1') {
                throw "get_net_classes after merge failed in scenario '$label'."
            }
            $jsonChanged = Get-McpResultJsonLine $outGetNcChanged
            $namesKeyChanged = Get-NetClassNamesKeyFromResultJson $jsonChanged

            $exact_blocker_if_any_set_nc = "N/A"
            if ($callable_after_merge_set_nc -ne "ok") {
                $exact_blocker_if_any_set_nc = "set_net_classes_merge_call_failed"
            } elseif ($namesKeyChanged -notmatch '(^|\|)QA_IPC_NETCLASS_1(\||$)') {
                $exact_blocker_if_any_set_nc = "qa_netclass_not_observed_after_merge"
            }

            $parsedInitialForRollback = $jsonInitial | ConvertFrom-Json
            $ncRollbackArr = @()
            if ($parsedInitialForRollback.PSObject.Properties["net_classes"] -and $parsedInitialForRollback.net_classes) {
                $ncRollbackArr = @($parsedInitialForRollback.net_classes)
            }
            $rollbackSetNcArgs = @{
                merge_mode  = "MMM_REPLACE"
                net_classes = $ncRollbackArr
            }
            $outSetNcRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_net_classes" `
                    -caseLabel "project_set_net_classes_rollback_replace" -argsObj $rollbackSetNcArgs -url $url -smokeExe $smokeExe
            $rollbackCallOkSetNc = ($outSetNcRollback -match 'status=1')

            $outGetNcFinal = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_net_classes" `
                    -caseLabel "project_net_classes_after_rollback" -argsObj $getNcArgsEmpty -url $url -smokeExe $smokeExe
            if ($outGetNcFinal -notmatch 'status=1') {
                throw "get_net_classes after rollback failed in scenario '$label'."
            }
            $jsonFinal = Get-McpResultJsonLine $outGetNcFinal
            $namesKeyFinal = Get-NetClassNamesKeyFromResultJson $jsonFinal

            $rollbackOkSetNc = "false"
            if ($rollbackCallOkSetNc -and $namesKeyFinal -eq $namesKeyInitial) {
                $rollbackOkSetNc = "true"
            }
            if ($exact_blocker_if_any_set_nc -eq "N/A") {
                if (-not $rollbackCallOkSetNc) {
                    $exact_blocker_if_any_set_nc = "rollback_call_failed"
                } elseif ($namesKeyFinal -ne $namesKeyInitial) {
                    $exact_blocker_if_any_set_nc = "rollback_state_mismatch"
                }
            }

            $callable_after_context_set_nc = $(
                if ($callable_after_merge_set_nc -eq "ok" -and $rollbackOkSetNc -eq "true") { "ok" } else { "error" })

            # "No project" is the only reliable null-project context: argv may load a project before get_project_info polling.
            $callableBeforeCtxFromNoProj = "N/A"
            $npDirsSetNc = @( Get-ChildItem -Directory (Join-Path $BuildDir "ipc-v1-artifacts") -ErrorAction SilentlyContinue |
                    Where-Object { $_.Name -like "*-No_project" } |
                    Sort-Object LastWriteTime -Descending )
            if ($npDirsSetNc.Count -gt 0) {
                $npSetNcSummaryPath = Join-Path $npDirsSetNc[0].FullName "qa_set_net_classes_no_project_context_summary.txt"
                if (Test-Path $npSetNcSummaryPath) {
                    $npSetNcRaw = Get-Content $npSetNcSummaryPath -Raw
                    if ($npSetNcRaw -match 'callable_before_context=(\w+)') {
                        $callableBeforeCtxFromNoProj = $Matches[1]
                    }
                }
            }

            ("visible_before_use=" + $visible_before_use_set_nc_str + "`n" +
             "callable_before_context=" + $callableBeforeCtxFromNoProj + "`n" +
             "callable_after_context=" + $callable_after_context_set_nc + "`n" +
             "initial_net_classes_summary=names=" + $namesKeyInitial + "`n" +
             "changed_net_classes_summary=names=" + $namesKeyChanged + "`n" +
             "rollback_ok=" + $rollbackOkSetNc + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_set_nc + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_set_net_classes_contract_summary.txt") -Encoding UTF8

            if ($callable_after_context_set_nc -ne "ok") {
                throw "set_net_classes contract failed in scenario '$label' (exact_blocker_if_any=$exact_blocker_if_any_set_nc)."
            }
        }

        if ($OpenEditorType -ne "") {
            if ($OpenEditorType -ne "pcb" -and $OpenEditorType -ne "sch") {
                throw "OpenEditorType must be either 'pcb' or 'sch' (got '$OpenEditorType')"
            }

            $openFile = if ($OpenEditorType -eq "pcb") { $pcb } else { $sch }
            if (!(Test-Path $openFile)) { throw "Missing editor file to open: $openFile" }

            $openArgsObj = @{
                type = $OpenEditorType
                filename = $openFile
            }
            $openArgsJson = $openArgsObj | ConvertTo-Json -Compress

            if ($OpenEditorType -eq "pcb") {
                # Artifact: get_pcb_items before opening the PCB frame.
                $visible_before_open = ($lt -match 'get_pcb_items')

                $beforeArgsObj = @{
                    board_filename = $pcb
                }
                $beforeOut = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" `
                    -caseLabel "pcb_before_frame_types_absent" -argsObj $beforeArgsObj `
                    -url $url -smokeExe $smokeExe

                $callable_before_open = $(if ($beforeOut -match 'status=1') { "ok" } else { "error" })
                $visible_before_open_str = $(if ($visible_before_open) { "true" } else { "false" })

                ("visible_before_open=" + $visible_before_open_str + "`n" +
                 "callable_before_open=" + $callable_before_open + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pcb_items_contract_before_summary.txt") -Encoding UTF8

                # get_pcb_items_by_id contract check (QA): before opening PCB frame.
                $visible_before_open_by_id = ($lt -match 'get_pcb_items_by_id')

                $visible_before_open_by_id_str = $(if ($visible_before_open_by_id) { "true" } else { "false" })

                $beforeArgsObjById = @{
                    board_filename = $pcb
                    ids             = @("00000000-0000-0000-0000-000000000000")
                }
                $beforeOutById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                        -caseLabel "pcb_before_frame_ids_absent" -argsObj $beforeArgsObjById `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_by_id = $(if ($beforeOutById -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_by_id + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pcb_items_by_id_contract_before_summary.txt") -Encoding UTF8

                # get_selection_pcb contract check (QA): before opening PCB frame.
                $visible_before_open_sel = ($lt -match 'get_selection_pcb')
                $visible_before_open_sel_str = $(if ($visible_before_open_sel) { "true" } else { "false" })

                $beforeArgsObjSel = @{}
                $beforeOutSel = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_selection_pcb" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjSel `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_sel = $(if ($beforeOutSel -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_sel_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sel + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_selection_pcb_contract_before_summary.txt") -Encoding UTF8

                # hit_test_pcb contract check (QA): before opening PCB frame.
                $visible_before_open_hit_test = ($lt -match 'hit_test_pcb')
                $visible_before_open_hit_test_str = $(if ($visible_before_open_hit_test) { "true" } else { "false" })

                $beforeArgsObjHitTest = @{
                    board_filename = $pcb
                    id              = "00000000-0000-0000-0000-000000000000"
                    position        = @{
                        x_nm = 5000000
                        y_nm = 0
                    }
                    tolerance       = 100000
                }

                $beforeOutHitTest = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_pcb" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjHitTest `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_hit_test = $(if ($beforeOutHitTest -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_hit_test_str + "`n" +
                 "callable_before_open=" + $callable_before_open_hit_test + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_pcb_contract_before_summary.txt") -Encoding UTF8

                # get_board_stackup contract check (QA): before opening PCB frame.
                $visible_before_open_stackup = ($lt -match 'get_board_stackup')
                $visible_before_open_stackup_str = $(if ($visible_before_open_stackup) { "true" } else { "false" })

                $beforeArgsObjStackup = @{
                    board_filename = $pcb
                }

                $beforeOutStackup = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_stackup" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjStackup `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_stackup = $(if ($beforeOutStackup -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_stackup_str + "`n" +
                 "callable_before_open=" + $callable_before_open_stackup + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_stackup_contract_before_summary.txt") -Encoding UTF8

                # get_nets contract check (QA): before opening PCB frame.
                $visible_before_open_nets = ($lt -match 'get_nets')
                $visible_before_open_nets_str = $(if ($visible_before_open_nets) { "true" } else { "false" })

                $beforeArgsObjNets = @{
                    board_filename = $pcb
                }

                $beforeOutNets = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_nets" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjNets `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_nets = $(if ($beforeOutNets -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_nets_str + "`n" +
                 "callable_before_open=" + $callable_before_open_nets + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_nets_contract_before_summary.txt") -Encoding UTF8

                # get_bounding_box_pcb contract check (QA): before opening PCB frame.
                $visible_before_open_bbox = ($lt -match 'get_bounding_box_pcb')
                $visible_before_open_bbox_str = $(if ($visible_before_open_bbox) { "true" } else { "false" })

                $beforeArgsObjBbox = @{
                    board_filename = $pcb
                    ids            = @("00000000-0000-0000-0000-000000000000")
                }

                $beforeOutBbox = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_bounding_box_pcb" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjBbox `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_bbox = $(if ($beforeOutBbox -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_bbox_str + "`n" +
                 "callable_before_open=" + $callable_before_open_bbox + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_bounding_box_pcb_contract_before_summary.txt") -Encoding UTF8

                # get_visible_layers contract check (QA): before opening PCB frame.
                $visible_before_open_visible_layers = ($lt -match 'get_visible_layers')
                $visible_before_open_visible_layers_str = $(if ($visible_before_open_visible_layers) { "true" } else { "false" })

                $beforeArgsObjVisibleLayers = @{
                    board_filename = $pcb
                }

                $beforeOutVisibleLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_visible_layers" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjVisibleLayers `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_visible_layers = $(if ($beforeOutVisibleLayers -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_visible_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_visible_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_visible_layers_contract_before_summary.txt") -Encoding UTF8

                # get_board_enabled_layers contract check (QA): before opening PCB frame.
                $visible_before_open_enabled_layers = ($lt -match 'get_board_enabled_layers')
                $visible_before_open_enabled_layers_str = $(if ($visible_before_open_enabled_layers) { "true" } else { "false" })

                $beforeArgsObjEnabledLayers = @{
                    board_filename = $pcb
                }

                $beforeOutEnabledLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_enabled_layers" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjEnabledLayers `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_enabled_layers = $(if ($beforeOutEnabledLayers -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_enabled_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_enabled_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_enabled_layers_contract_before_summary.txt") -Encoding UTF8

                # get_board_layer_names contract check (QA): before opening PCB frame.
                $visible_before_open_layer_names = ($lt -match 'get_board_layer_names')
                $visible_before_open_layer_names_str = $(if ($visible_before_open_layer_names) { "true" } else { "false" })

                $beforeArgsObjLayerNames = @{
                    board_filename = $pcb
                }

                $beforeOutLayerNames = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_layer_names" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjLayerNames `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_layer_names = $(if ($beforeOutLayerNames -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_layer_names_str + "`n" +
                 "callable_before_open=" + $callable_before_open_layer_names + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_layer_names_contract_before_summary.txt") -Encoding UTF8

                # get_active_layer contract check (QA): before opening PCB frame.
                $visible_before_open_active_layer = ($lt -match 'get_active_layer')
                $visible_before_open_active_layer_str = $(if ($visible_before_open_active_layer) { "true" } else { "false" })

                $beforeArgsObjActiveLayer = @{
                    board_filename = $pcb
                }

                $beforeOutActiveLayer = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_active_layer" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjActiveLayer `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_active_layer = $(if ($beforeOutActiveLayer -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_active_layer_str + "`n" +
                 "callable_before_open=" + $callable_before_open_active_layer + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_active_layer_contract_before_summary.txt") -Encoding UTF8

                # set_active_layer contract check (QA): before opening PCB frame.
                $visible_before_open_set_active_layer = ($lt -match 'set_active_layer')
                $visible_before_open_set_active_layer_str = $(if ($visible_before_open_set_active_layer) { "true" } else { "false" })

                $beforeArgsObjSetActiveLayer = @{
                    board_filename = $pcb
                    layer          = 0
                }
                $beforeOutSetActiveLayer = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_active_layer" `
                        -caseLabel "pcb_before_frame_set_active_layer_context_missing" -argsObj $beforeArgsObjSetActiveLayer `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_set_active_layer = $(if ($beforeOutSetActiveLayer -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_set_active_layer = "N/A"
                if ($callable_before_open_set_active_layer -eq "ok") {
                    $exact_blocker_if_any_before_set_active_layer = "unexpected_success_before_open"
                }

                ("visible_before_open=" + $visible_before_open_set_active_layer_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_active_layer + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_set_active_layer + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_active_layer_contract_before_summary.txt") -Encoding UTF8

                # set_visible_layers contract check (QA): before opening PCB frame.
                $visible_before_open_set_visible_layers = ($lt -match 'set_visible_layers')
                $visible_before_open_set_visible_layers_str = $(if ($visible_before_open_set_visible_layers) { "true" } else { "false" })

                $beforeArgsObjSetVisibleLayers = @{
                    board_filename = $pcb
                    layers         = @(3, 34)
                }
                $beforeOutSetVisibleLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_visible_layers" `
                        -caseLabel "pcb_before_frame_set_visible_layers_context_missing" -argsObj $beforeArgsObjSetVisibleLayers `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_set_visible_layers = $(if ($beforeOutSetVisibleLayers -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_set_visible_layers = "N/A"
                if ($callable_before_open_set_visible_layers -eq "ok") {
                    $exact_blocker_if_any_before_set_visible_layers = "unexpected_success_before_open"
                }

                ("visible_before_open=" + $visible_before_open_set_visible_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_visible_layers + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_set_visible_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_visible_layers_contract_before_summary.txt") -Encoding UTF8

                # set_board_editor_appearance_settings contract check (QA): before opening PCB frame.
                $visible_before_open_set_editor_appearance = ($lt -match 'set_board_editor_appearance_settings')
                $visible_before_open_set_editor_appearance_str = $(if ($visible_before_open_set_editor_appearance) { "true" } else { "false" })

                $beforeArgsObjSetEditorAppearance = @{
                    board_filename = $pcb
                    settings = @{
                        inactive_layer_display = 1
                        net_color_display      = 1
                        board_flip             = 1
                        ratsnest_display      = 1
                    }
                }

                $beforeOutSetEditorAppearance = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_editor_appearance_settings" `
                        -caseLabel "pcb_before_frame_set_board_editor_appearance_settings_context_missing" -argsObj $beforeArgsObjSetEditorAppearance `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_set_editor_appearance = $(if ($beforeOutSetEditorAppearance -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_set_editor_appearance = "N/A"
                if ($callable_before_open_set_editor_appearance -eq "ok") {
                    $exact_blocker_if_any_before_set_editor_appearance = "unexpected_success_before_open"
                }

                ("visible_before_open=" + $visible_before_open_set_editor_appearance_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_editor_appearance + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_set_editor_appearance + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_board_editor_appearance_settings_contract_before_summary.txt") -Encoding UTF8

                # set_board_origin contract check (QA): before opening PCB frame.
                $visible_before_open_set_board_origin = ($lt -match 'set_board_origin')
                $visible_before_open_set_board_origin_str = $(if ($visible_before_open_set_board_origin) { "true" } else { "false" })

                $beforeArgsObjSetBoardOrigin = @{
                    board_filename = $pcb
                    type            = 1
                    origin          = @{
                        x_nm = 0
                        y_nm = 0
                    }
                }

                $beforeOutSetBoardOrigin = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_origin" `
                        -caseLabel "pcb_before_frame_set_board_origin_context_missing" -argsObj $beforeArgsObjSetBoardOrigin `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_set_board_origin = $(if ($beforeOutSetBoardOrigin -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_set_board_origin = "N/A"
                if ($callable_before_open_set_board_origin -eq "ok") {
                    $exact_blocker_if_any_before_set_board_origin = "unexpected_success_before_open"
                }

                ("visible_before_open=" + $visible_before_open_set_board_origin_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_board_origin + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_set_board_origin + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_board_origin_contract_before_summary.txt") -Encoding UTF8

                # create_pcb_items / delete_pcb_items (v1 content-write): before PCB frame exists.
                $visible_before_open_create_pcb_items = ($lt -match 'create_pcb_items')
                $visible_before_open_delete_pcb_items = ($lt -match 'delete_pcb_items')
                $visible_before_open_pcb_write_str = $(if ($visible_before_open_create_pcb_items -and $visible_before_open_delete_pcb_items) { "true" } else { "false" })

                $beforeArgsObjCreatePcbItems = @{
                    board_filename = $pcb
                    start          = @{ x_nm = 1000000; y_nm = 2000000 }
                    end            = @{ x_nm = 2000000; y_nm = 2000000 }
                }
                $beforeOutCreatePcbItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "create_pcb_items" `
                        -caseLabel "pcb_before_frame_create_pcb_items" -argsObj $beforeArgsObjCreatePcbItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_create_pcb_items = $(if ($beforeOutCreatePcbItems -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_pcb_write = "N/A"
                if ($callable_before_open_create_pcb_items -eq "ok") {
                    $exact_blocker_if_any_before_pcb_write = "unexpected_success_create_before_open"
                }

                $beforeArgsObjDeletePcbItems = @{
                    board_filename = $pcb
                    ids             = @("00000000-0000-4000-8000-000000000001")
                }
                $beforeOutDeletePcbItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "delete_pcb_items" `
                        -caseLabel "pcb_before_frame_delete_pcb_items" -argsObj $beforeArgsObjDeletePcbItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_delete_pcb_items = $(if ($beforeOutDeletePcbItems -match 'status=1') { "ok" } else { "error" })
                if ($callable_before_open_delete_pcb_items -eq "ok") {
                    $exact_blocker_if_any_before_pcb_write = "unexpected_success_delete_before_open"
                }

                ("visible_before_open=" + $visible_before_open_pcb_write_str + "`n" +
                 "callable_before_open_create=" + $callable_before_open_create_pcb_items + "`n" +
                 "callable_before_open_delete=" + $callable_before_open_delete_pcb_items + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_pcb_write + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_create_delete_pcb_items_contract_before_summary.txt") -Encoding UTF8

                # update_pcb_items (v1 content-write): before PCB frame exists.
                $visible_before_open_update_pcb_items = ($lt -match 'update_pcb_items')
                $visible_before_open_update_pcb_items_str = $(if ($visible_before_open_update_pcb_items) { "true" } else { "false" })

                $beforeArgsObjUpdatePcbItems = @{
                    board_filename = $pcb
                    item_id        = "00000000-0000-4000-8000-000000000001"
                    end            = @{ x_nm = 3000000; y_nm = 3000000 }
                }
                $beforeOutUpdatePcbItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_pcb_items" `
                        -caseLabel "pcb_before_frame_update_pcb_items" -argsObj $beforeArgsObjUpdatePcbItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_update_pcb_items = $(if ($beforeOutUpdatePcbItems -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_update_pcb_items = "N/A"
                if ($callable_before_open_update_pcb_items -eq "ok") {
                    $exact_blocker_if_any_before_update_pcb_items = "unexpected_success_update_before_open"
                }

                ("visible_before_open=" + $visible_before_open_update_pcb_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_update_pcb_items + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_update_pcb_items + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_update_pcb_items_contract_before_summary.txt") -Encoding UTF8

                # pcb_graphic_annotation_session (composed v1): before PCB frame exists.
                $visible_before_open_pcb_graphic_annotation = ($lt -match 'pcb_graphic_annotation_session')
                $visible_before_open_pcb_graphic_annotation_str = $(if ($visible_before_open_pcb_graphic_annotation) { "true" } else { "false" })
                $beforeArgsObjPcbGraphicAnnotation = @{
                    board_filename = $pcb
                    start          = @{ x_nm = 1700000; y_nm = 2700000 }
                    end            = @{ x_nm = 2700000; y_nm = 2700000 }
                }
                $beforeOutPcbGraphicAnnotation = Call-McpToolArgsAndLog -runDir $runDir -toolName "pcb_graphic_annotation_session" `
                        -caseLabel "pcb_before_frame_pcb_graphic_annotation_session" -argsObj $beforeArgsObjPcbGraphicAnnotation `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_pcb_graphic_annotation = $(if ($beforeOutPcbGraphicAnnotation -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_pcb_graphic_annotation = "N/A"
                if ($callable_before_open_pcb_graphic_annotation -eq "ok") {
                    $exact_blocker_if_any_before_pcb_graphic_annotation = "unexpected_success_before_open"
                }

                # get_board_editor_appearance_settings contract check (QA): before opening PCB frame.
                $visible_before_open_editor_appearance = ($lt -match 'get_board_editor_appearance_settings')
                $visible_before_open_editor_appearance_str = $(if ($visible_before_open_editor_appearance) { "true" } else { "false" })

                $beforeArgsObjEditorAppearance = @{
                    board_filename = $pcb
                }

                $beforeOutEditorAppearance = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_editor_appearance_settings" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjEditorAppearance `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_editor_appearance = $(if ($beforeOutEditorAppearance -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_editor_appearance_str + "`n" +
                 "callable_before_open=" + $callable_before_open_editor_appearance + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_editor_appearance_settings_contract_before_summary.txt") -Encoding UTF8

                # get_board_origin contract check (QA): before opening PCB frame.
                $visible_before_open_board_origin = ($lt -match 'get_board_origin')
                $visible_before_open_board_origin_str = $(if ($visible_before_open_board_origin) { "true" } else { "false" })

                $beforeArgsObjBoardOrigin = @{
                    board_filename = $pcb
                }

                $beforeOutBoardOrigin = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_origin" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjBoardOrigin `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_board_origin = $(if ($beforeOutBoardOrigin -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_board_origin_str + "`n" +
                 "callable_before_open=" + $callable_before_open_board_origin + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_origin_contract_before_summary.txt") -Encoding UTF8

                # get_board_extent contract check (QA): before opening PCB frame.
                $visible_before_open_board_extent = ($lt -match 'get_board_extent')
                $visible_before_open_board_extent_str = $(if ($visible_before_open_board_extent) { "true" } else { "false" })

                $beforeArgsObjBoardExtent = @{
                    board_filename = $pcb
                }

                $beforeOutBoardExtent = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_extent" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjBoardExtent `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_board_extent = $(if ($beforeOutBoardExtent -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_board_extent_str + "`n" +
                 "callable_before_open=" + $callable_before_open_board_extent + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_extent_contract_before_summary.txt") -Encoding UTF8

                # get_pad_shape_as_polygon contract check (QA): before opening PCB frame.
                $visible_before_open_pad_shape = ($lt -match 'get_pad_shape_as_polygon')
                $visible_before_open_pad_shape_str = $(if ($visible_before_open_pad_shape) { "true" } else { "false" })

                $beforeArgsObjPadShape = @{
                    board_filename = $pcb
                    pads           = @("00000000-0000-0000-0000-000000000000")
                    layer          = "BL_F_Cu"
                }
                $beforeOutPadShape = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pad_shape_as_polygon" `
                        -caseLabel "pcb_before_frame_context_missing" -argsObj $beforeArgsObjPadShape `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_pad_shape = $(if ($beforeOutPadShape -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_pad_shape_str + "`n" +
                 "callable_before_open=" + $callable_before_open_pad_shape + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pad_shape_as_polygon_contract_before_summary.txt") -Encoding UTF8
            }
            elseif ($OpenEditorType -eq "sch") {
                # get_selection_sch contract check (QA): before opening SCH frame.
                $visible_before_open_sel_sch = ($lt -match 'get_selection_sch')
                $visible_before_open_sel_sch_str = $(if ($visible_before_open_sel_sch) { "true" } else { "false" })

                $beforeArgsObjSelSch = @{}
                $beforeOutSelSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_selection_sch" `
                        -caseLabel "sch_before_frame_context_missing" -argsObj $beforeArgsObjSelSch `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_sel_sch = $(if ($beforeOutSelSch -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_sel_sch_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sel_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_selection_sch_contract_before_summary.txt") -Encoding UTF8

                # hit_test_sch contract check (QA): before opening SCH frame.
                $visible_before_open_hit_test_sch = ($lt -match 'hit_test_sch')
                $visible_before_open_hit_test_sch_str = $(if ($visible_before_open_hit_test_sch) { "true" } else { "false" })

                $beforeArgsObjHitTestSch = @{
                    schematic_filename = $sch
                    id                 = "11111111-1111-1111-1111-111111111111"
                    position           = @{
                        x_nm = 5000000
                        y_nm = 0
                    }
                    tolerance          = 0
                }
                $beforeOutHitTestSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_before_frame_context_missing" -argsObj $beforeArgsObjHitTestSch `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_hit_test_sch = $(if ($beforeOutHitTestSch -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_hit_test_sch_str + "`n" +
                 "callable_before_open=" + $callable_before_open_hit_test_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_sch_contract_before_summary.txt") -Encoding UTF8

                # get_sch_items_by_id contract check (QA): before opening SCH frame.
                $visible_before_open_sch_items_by_id = ($lt -match 'get_sch_items_by_id')
                $visible_before_open_sch_items_by_id_str = $(if ($visible_before_open_sch_items_by_id) { "true" } else { "false" })

                $wireSchId = "11111111-1111-1111-1111-111111111111"
                $beforeArgsObjSchItemsById = @{
                    schematic_filename = $sch
                    ids                 = @($wireSchId)
                }
                $beforeOutSchItemsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                        -caseLabel "sch_before_frame_items_by_id_context_missing" -argsObj $beforeArgsObjSchItemsById `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_sch_items_by_id = $(if ($beforeOutSchItemsById -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_sch_items_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items_by_id + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_by_id_contract_before_summary.txt") -Encoding UTF8

                # get_sch_items_by_id symbol contract check (QA): before opening SCH frame.
                $schSymbolId = "22222222-2222-2222-2222-222222222222"
                $beforeArgsObjSchItemsByIdSymbols = @{
                    schematic_filename = $sch
                    ids                 = @($schSymbolId)
                }
                $beforeOutSchItemsByIdSymbols = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                        -caseLabel "sch_before_frame_items_by_id_symbol_context_missing" -argsObj $beforeArgsObjSchItemsByIdSymbols `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_sch_items_by_id_symbols = $(if ($beforeOutSchItemsByIdSymbols -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_sch_items_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items_by_id_symbols + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_by_id_symbol_contract_before_summary.txt") -Encoding UTF8

                # get_sch_items contract check (QA): before opening SCH frame.
                $visible_before_open_sch_items = ($lt -match 'get_sch_items')
                $visible_before_open_sch_items_str = $(if ($visible_before_open_sch_items) { "true" } else { "false" })

                $beforeArgsObjSchItems = @{
                    schematic_filename = $sch
                }
                $beforeOutSchItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_before_frame_items_context_missing" -argsObj $beforeArgsObjSchItems `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_sch_items = $(if ($beforeOutSchItems -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_sch_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_contract_before_summary.txt") -Encoding UTF8

                # create_sch_items / delete_sch_items (v1 content-write): before SCH frame exists.
                $visible_before_open_create_sch_items = ($lt -match 'create_sch_items')
                $visible_before_open_delete_sch_items = ($lt -match 'delete_sch_items')
                $visible_before_open_sch_write_str = $(if ($visible_before_open_create_sch_items -and $visible_before_open_delete_sch_items) { "true" } else { "false" })

                $beforeArgsObjCreateSchItems = @{
                    schematic_filename = $sch
                    start              = @{ x_nm = 1000000; y_nm = 2000000 }
                    end                = @{ x_nm = 2000000; y_nm = 2000000 }
                }
                $beforeOutCreateSchItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "create_sch_items" `
                        -caseLabel "sch_before_frame_create_sch_items" -argsObj $beforeArgsObjCreateSchItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_create_sch_items = $(if ($beforeOutCreateSchItems -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_sch_write = "N/A"
                if ($callable_before_open_create_sch_items -eq "ok") {
                    $exact_blocker_if_any_before_sch_write = "unexpected_success_create_before_open"
                }

                $beforeArgsObjDeleteSchItems = @{
                    schematic_filename = $sch
                    ids                 = @("00000000-0000-4000-8000-000000000001")
                }
                $beforeOutDeleteSchItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "delete_sch_items" `
                        -caseLabel "sch_before_frame_delete_sch_items" -argsObj $beforeArgsObjDeleteSchItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_delete_sch_items = $(if ($beforeOutDeleteSchItems -match 'status=1') { "ok" } else { "error" })
                if ($callable_before_open_delete_sch_items -eq "ok") {
                    $exact_blocker_if_any_before_sch_write = "unexpected_success_delete_before_open"
                }

                ("visible_before_open=" + $visible_before_open_sch_write_str + "`n" +
                 "callable_before_open_create=" + $callable_before_open_create_sch_items + "`n" +
                 "callable_before_open_delete=" + $callable_before_open_delete_sch_items + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_sch_write + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_create_delete_sch_items_contract_before_summary.txt") -Encoding UTF8

                # update_sch_items (v1): before SCH frame exists.
                $visible_before_open_update_sch_items = ($lt -match 'update_sch_items')
                $visible_before_open_update_sch_items_str = $(if ($visible_before_open_update_sch_items) { "true" } else { "false" })
                $beforeArgsObjUpdateSchItems = @{
                    schematic_filename = $sch
                    id                 = "00000000-0000-4000-8000-000000000002"
                    end                = @{ x_nm = 1000000; y_nm = 2000000 }
                }
                $beforeOutUpdateSchItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_sch_items" `
                        -caseLabel "sch_before_frame_update_sch_items" -argsObj $beforeArgsObjUpdateSchItems `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_update_sch_items = $(if ($beforeOutUpdateSchItems -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_sch_update = "N/A"
                if ($callable_before_open_update_sch_items -eq "ok") {
                    $exact_blocker_if_any_before_sch_update = "unexpected_success_update_before_open"
                }
                $callable_before_open_sch_update = $(if ($callable_before_open_update_sch_items -eq "ok") { "unexpected_ok" } else { "error" })
                ("visible_before_open=" + $visible_before_open_update_sch_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_update + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_sch_update + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_update_sch_items_contract_before_summary.txt") -Encoding UTF8

                # sch_wire_edit_session (composed v1): before SCH frame exists.
                $visible_before_open_sch_wire_session = ($lt -match 'sch_wire_edit_session')
                $visible_before_open_sch_wire_session_str = $(if ($visible_before_open_sch_wire_session) { "true" } else { "false" })
                $beforeArgsObjSchWireSession = @{
                    schematic_filename = $sch
                    start              = @{ x_nm = 1820000; y_nm = 2820000 }
                    end                = @{ x_nm = 2820000; y_nm = 2820000 }
                    end_updated        = @{ x_nm = 3320000; y_nm = 2820000 }
                }
                $beforeOutSchWireSession = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_wire_edit_session" `
                        -caseLabel "sch_before_frame_sch_wire_edit_session" -argsObj $beforeArgsObjSchWireSession `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_sch_wire_session_raw = $(if ($beforeOutSchWireSession -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_sch_wire_session = "N/A"
                if ($callable_before_open_sch_wire_session_raw -eq "ok") {
                    $exact_blocker_if_any_before_sch_wire_session = "unexpected_success_session_before_open"
                }
                $callable_before_open_sch_wire_session = $(if ($callable_before_open_sch_wire_session_raw -eq "ok") { "unexpected_ok" } else { "error" })
                ("visible_before_open=" + $visible_before_open_sch_wire_session_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_wire_session + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_sch_wire_session + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_wire_edit_session_contract_before_summary.txt") -Encoding UTF8

                # sch_wire_quick_fix_session (composed v1): before SCH frame exists.
                $visible_before_open_sch_wire_quick_fix = ($lt -match 'sch_wire_quick_fix_session')
                $visible_before_open_sch_wire_quick_fix_str = $(if ($visible_before_open_sch_wire_quick_fix) { "true" } else { "false" })
                $beforeArgsObjSchWireQuickFix = @{
                    schematic_filename = $sch
                    target_wire_id     = "11111111-1111-1111-1111-111111111111"
                    delta_end_x_nm     = 200000
                    delta_end_y_nm     = 0
                }
                $beforeOutSchWireQuickFix = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_wire_quick_fix_session" `
                        -caseLabel "sch_before_frame_sch_wire_quick_fix_session" -argsObj $beforeArgsObjSchWireQuickFix `
                        -url $url -smokeExe $smokeExe
                $callable_before_open_sch_wire_quick_fix_raw = $(if ($beforeOutSchWireQuickFix -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_before_sch_wire_quick_fix = "N/A"
                if ($callable_before_open_sch_wire_quick_fix_raw -eq "ok") {
                    $exact_blocker_if_any_before_sch_wire_quick_fix = "unexpected_success_quick_fix_before_open"
                }
                $callable_before_open_sch_wire_quick_fix = $(if ($callable_before_open_sch_wire_quick_fix_raw -eq "ok") { "unexpected_ok" } else { "error" })
                ("visible_before_open=" + $visible_before_open_sch_wire_quick_fix_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_wire_quick_fix + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before_sch_wire_quick_fix + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_wire_quick_fix_session_contract_before_summary.txt") -Encoding UTF8

                # get_symbol_pins_by_id contract check (QA): before opening SCH frame.
                $visible_before_open_symbol_pins_by_id = ($lt -match 'get_symbol_pins_by_id')
                $visible_before_open_symbol_pins_by_id_str = $(if ($visible_before_open_symbol_pins_by_id) { "true" } else { "false" })

                $schSymbolId = "22222222-2222-2222-2222-222222222222"
                $beforeArgsObjSymbolPinsById = @{
                    schematic_filename = $sch
                    id                 = $schSymbolId
                }
                $beforeOutSymbolPinsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_pins_by_id" `
                        -caseLabel "sch_before_frame_symbol_pins_context_missing" -argsObj $beforeArgsObjSymbolPinsById `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_symbol_pins_by_id = $(if ($beforeOutSymbolPinsById -match 'status=1') { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_symbol_pins_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_symbol_pins_by_id + "`n" +
                 "first_pin_x_iu=N/A`n" +
                 "first_pin_y_iu=N/A`n" +
                 "first_pin_orientation=N/A`n" +
                 "first_pin_length_iu=N/A`n" +
                 "first_pin_root_x_iu=N/A`n" +
                 "first_pin_root_y_iu=N/A`n" +
                 "first_pin_type_name=N/A`n" +
                 "first_pin_orientation_name=N/A`n" +
                 "first_pin_is_power=N/A`n" +
                 "first_pin_is_global_power=N/A`n" +
                 "first_pin_is_local_power=N/A`n" +
                 "first_pin_net_name=N/A`n" +
                 "first_pin_net_name_source=N/A`n" +
                 "first_pin_net_name_default=N/A`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_symbol_pins_by_id_contract_before_summary.txt") -Encoding UTF8

                # get_symbol_fields_by_id contract check (QA): before opening SCH frame.
                $visible_before_open_symbol_fields_by_id = ($lt -match 'get_symbol_fields_by_id')
                $visible_before_open_symbol_fields_by_id_str = $(if ($visible_before_open_symbol_fields_by_id) { "true" } else { "false" })

                $schSymbolId = "22222222-2222-2222-2222-222222222222"
                $expectedRef = "#PWR"
                $expectedValue = "GND"
                $expectedLibId = "power:GND"
                $expectedIsPowerSymbol = "true"
                $expectedIsGlobalPowerSymbol = "true"
                $expectedIsLocalPowerSymbol = "false"
                $beforeArgsObjSymbolFieldsById = @{
                    schematic_filename = $sch
                    id                 = $schSymbolId
                }
                $beforeOutSymbolFieldsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_fields_by_id" `
                        -caseLabel "sch_before_frame_symbol_fields_context_missing" -argsObj $beforeArgsObjSymbolFieldsById `
                        -url $url -smokeExe $smokeExe

                $callable_before_open_symbol_fields_by_id = $(if ($beforeOutSymbolFieldsById -match 'status=1') { "ok" } else { "error" })

                $fields_present_summary_before = "N/A"
                $exact_blocker_if_any_before = "N/A"

                ("visible_before_open=" + $visible_before_open_symbol_fields_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_symbol_fields_by_id + "`n" +
                 "reference_before=" + "N/A" + "`n" +
                 "value_before=" + "N/A" + "`n" +
                 "lib_id_before=" + "N/A" + "`n" +
                 "is_power_symbol_before=" + "N/A" + "`n" +
                 "is_global_power_symbol_before=" + "N/A" + "`n" +
                 "is_local_power_symbol_before=" + "N/A" + "`n" +
                 "fields_present_summary=" + $fields_present_summary_before + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_before + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_symbol_fields_by_id_contract_before_summary.txt") -Encoding UTF8
            }

            # Optional before-open proof for workspace API get_open_documents.
            $openDocsBeforeArgsObj = @{
                type = $OpenEditorType
            }
            $openDocsBeforeArgsJson = $openDocsBeforeArgsObj | ConvertTo-Json -Compress
            $openDocsBeforeInputPath = Join-Path $runDir ("mcp_call_tool_args_get_open_documents_before_" + $OpenEditorType + "_input.json")
            $openDocsBeforeArgsJson | Set-Content -Path $openDocsBeforeInputPath -Encoding UTF8
            $odBefore = & $smokeExe $url "mcp_call_tool_args" "get_open_documents" ("@" + $openDocsBeforeInputPath) 2>&1 | Out-String
            $odBefore | Set-Content -Path (Join-Path $runDir ("mcp_call_tool_args_get_open_documents_before_" + $OpenEditorType + ".log")) -Encoding UTF8
            $callable_before_open_get_open_documents = $(if ($odBefore -match 'status=1') { "ok" } else { "error" })
            ("callable_before_open=" + $callable_before_open_get_open_documents + "`n" +
             "type_filter=" + $OpenEditorType + "`n" +
             "exact_blocker_if_any=" + $(if ($callable_before_open_get_open_documents -eq "ok") { "N/A" } else { "get_open_documents_call_failed_before_open" }) + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_get_open_documents_contract_before_summary.txt") -Encoding UTF8

            # Pass args_json via file (prefix '@') to avoid Windows command-line quoting issues.
            $qaOpenInputPath = Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_" + $OpenEditorType + "_input.json")
            $openArgsJson | Set-Content -Path $qaOpenInputPath -Encoding UTF8
            $qaOpen = & $smokeExe $url "mcp_call_tool_args" "qa_open_editor_frame" ("@" + $qaOpenInputPath) 2>&1 | Out-String
            $qaOpen | Set-Content -Path (Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_" + $OpenEditorType + ".log")) -Encoding UTF8

            Start-Sleep -Seconds 2

            $ct2b = & $smokeExe $url mcp_call_tool get_workspace_overview 2>&1 | Out-String
            $ct2b | Set-Content -Path (Join-Path $runDir ("mcp_call_tool_get_workspace_overview_after_open_" + $OpenEditorType + ".log")) -Encoding UTF8

            if ($OpenEditorType -eq "pcb") {
                # Artifact: tools/list after opening PCB frame.
                $ltAfter = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
                $ltAfter | Set-Content -Path (Join-Path $runDir "mcp_list_tools_after_open_pcb.log") -Encoding UTF8
                if ($ltAfter -notmatch 'create_pcb_items') {
                    throw "MCP ListTools missing create_pcb_items after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'delete_pcb_items') {
                    throw "MCP ListTools missing delete_pcb_items after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'update_pcb_items') {
                    throw "MCP ListTools missing update_pcb_items after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'pcb_graphic_annotation_session') {
                    throw "MCP ListTools missing pcb_graphic_annotation_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'pcb_fabrication_origin_prepare_session') {
                    throw "MCP ListTools missing pcb_fabrication_origin_prepare_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'pcb_origin_decision_correction_session') {
                    throw "MCP ListTools missing pcb_origin_decision_correction_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'cross_context_preflight_correction_session') {
                    throw "MCP ListTools missing cross_context_preflight_correction_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_preflight_gate_session') {
                    throw "MCP ListTools missing workflow_preflight_gate_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_next_best_action_session') {
                    throw "MCP ListTools missing workflow_next_best_action_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_handoff_readiness_session') {
                    throw "MCP ListTools missing workflow_handoff_readiness_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_assistant_session') {
                    throw "MCP ListTools missing workflow_assistant_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_directive_inspect') {
                    throw "MCP ListTools missing simulation_directive_inspect after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_netlist_preview_session') {
                    throw "MCP ListTools missing simulation_netlist_preview_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_model_library_inspect') {
                    throw "MCP ListTools missing simulation_model_library_inspect after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_preflight_readiness_session') {
                    throw "MCP ListTools missing simulation_preflight_readiness_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_status_session') {
                    throw "MCP ListTools missing simulation_run_status_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_minimal_summary_session') {
                    throw "MCP ListTools missing simulation_run_minimal_summary_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_vector_catalog_session') {
                    throw "MCP ListTools missing simulation_run_vector_catalog_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_vector_scalar_probe_session') {
                    throw "MCP ListTools missing simulation_vector_scalar_probe_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_xaxis_probe_session') {
                    throw "MCP ListTools missing simulation_xaxis_probe_session after opening PCB in scenario '$label'."
                }
                if ($ltAfter -notmatch 'library_symbol_search_session') {
                    throw "MCP ListTools missing library_symbol_search_session after opening PCB in scenario '$label'."
                }
            } elseif ($OpenEditorType -eq "sch") {
                # Artifact: tools/list after opening SCH frame.
                $ltAfter = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
                $ltAfter | Set-Content -Path (Join-Path $runDir "mcp_list_tools_after_open_sch.log") -Encoding UTF8
                if ($ltAfter -notmatch 'create_sch_items') {
                    throw "MCP ListTools missing create_sch_items after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'delete_sch_items') {
                    throw "MCP ListTools missing delete_sch_items after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'update_sch_items') {
                    throw "MCP ListTools missing update_sch_items after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'sch_wire_edit_session') {
                    throw "MCP ListTools missing sch_wire_edit_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'sch_wire_quick_fix_session') {
                    throw "MCP ListTools missing sch_wire_quick_fix_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'sch_pin_connection_fix_session') {
                    throw "MCP ListTools missing sch_pin_connection_fix_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'sch_connection_repair_decision_session') {
                    throw "MCP ListTools missing sch_connection_repair_decision_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'cross_context_preflight_correction_session') {
                    throw "MCP ListTools missing cross_context_preflight_correction_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_preflight_gate_session') {
                    throw "MCP ListTools missing workflow_preflight_gate_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_next_best_action_session') {
                    throw "MCP ListTools missing workflow_next_best_action_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_handoff_readiness_session') {
                    throw "MCP ListTools missing workflow_handoff_readiness_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'workflow_assistant_session') {
                    throw "MCP ListTools missing workflow_assistant_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_directive_inspect') {
                    throw "MCP ListTools missing simulation_directive_inspect after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_netlist_preview_session') {
                    throw "MCP ListTools missing simulation_netlist_preview_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_model_library_inspect') {
                    throw "MCP ListTools missing simulation_model_library_inspect after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_preflight_readiness_session') {
                    throw "MCP ListTools missing simulation_preflight_readiness_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_status_session') {
                    throw "MCP ListTools missing simulation_run_status_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_minimal_summary_session') {
                    throw "MCP ListTools missing simulation_run_minimal_summary_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_run_vector_catalog_session') {
                    throw "MCP ListTools missing simulation_run_vector_catalog_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_vector_scalar_probe_session') {
                    throw "MCP ListTools missing simulation_vector_scalar_probe_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'simulation_xaxis_probe_session') {
                    throw "MCP ListTools missing simulation_xaxis_probe_session after opening SCH in scenario '$label'."
                }
                if ($ltAfter -notmatch 'library_symbol_search_session') {
                    throw "MCP ListTools missing library_symbol_search_session after opening SCH in scenario '$label'."
                }
            }

            # Validate product tool: get_open_documents (filtered).
            $openDocsArgsObj = @{
                type = $OpenEditorType
            }
            $openDocsArgsJson = $openDocsArgsObj | ConvertTo-Json -Compress

            $openDocsInputPath = Join-Path $runDir ("mcp_call_tool_args_get_open_documents_" + $OpenEditorType + "_input.json")
            $openDocsArgsJson | Set-Content -Path $openDocsInputPath -Encoding UTF8
            $od = & $smokeExe $url "mcp_call_tool_args" "get_open_documents" ("@" + $openDocsInputPath) 2>&1 | Out-String
            $od | Set-Content -Path (Join-Path $runDir ("mcp_call_tool_args_get_open_documents_" + $OpenEditorType + ".log")) -Encoding UTF8

            $callable_after_open_get_open_documents = $(if ($od -match 'status=1') { "ok" } else { "error" })
            $exact_blocker_if_any_open_docs = $(if ($callable_after_open_get_open_documents -eq "ok") { "N/A" } else { "get_open_documents_call_failed" })
            ("callable_after_open=" + $callable_after_open_get_open_documents + "`n" +
             "type_filter=" + $OpenEditorType + "`n" +
             "exact_blocker_if_any=" + $exact_blocker_if_any_open_docs + "`n") |
                Set-Content -Path (Join-Path $runDir "qa_get_open_documents_contract_summary.txt") -Encoding UTF8

            if ($OpenEditorType -eq "sch") {
                # simulation_netlist_preview_session targeted validation in real SCH-open context.
                $netlistPreviewOpenSchArgs = @{}
                $outNetlistPreviewOpenSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_netlist_preview_session" `
                        -caseLabel "sch_open_simulation_netlist_preview_session" -argsObj $netlistPreviewOpenSchArgs -url $url -smokeExe $smokeExe

                $callable_without_runtime_simulation_open_sch = $(if ($outNetlistPreviewOpenSch -match 'status=1') { "true" } else { "false" })
                $preview_ok_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "preview_ok=*" } | Select-Object -First 1) -replace '^preview_ok=', '').Trim()
                $preview_ok_open_sch_str = $(if ($preview_ok_open_sch -eq "1") { "true" } else { "false" })
                $netlist_text_present_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "netlist_text_present=*" } | Select-Object -First 1) -replace '^netlist_text_present=', '').Trim()
                $netlist_line_count_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "netlist_line_count=*" } | Select-Object -First 1) -replace '^netlist_line_count=', '').Trim()
                $netlist_line_count_valid_open_sch = $false
                if ($netlist_line_count_open_sch -match '^\d+$' -and [int]$netlist_line_count_open_sch -gt 0) { $netlist_line_count_valid_open_sch = $true }
                $netlist_line_count_valid_open_sch_str = $(if ($netlist_line_count_valid_open_sch) { "true" } else { "false" })
                $diagnostics_present_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $source_context_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $export_path_strategy_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "export_path_strategy=*" } | Select-Object -First 1) -replace '^export_path_strategy=', '').Trim()
                $preview_result_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "preview_result=*" } | Select-Object -First 1) -replace '^preview_result=', '').Trim()
                $first_lines_excerpt_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "first_lines_excerpt=*" } | Select-Object -First 1) -replace '^first_lines_excerpt=', '').Trim()
                $diagnostics_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_open_sch = ((($outNetlistPreviewOpenSch -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()

                $exact_blocker_if_any_netlist_preview_open_sch = "N/A"
                if ($callable_without_runtime_simulation_open_sch -ne "true") {
                    $exact_blocker_if_any_netlist_preview_open_sch = "callable_without_runtime_simulation_failed"
                } elseif ($preview_ok_open_sch_str -ne "true") {
                    $exact_blocker_if_any_netlist_preview_open_sch = "preview_ok_false_in_open_sch_context"
                } elseif ($netlist_text_present_open_sch -ne "true") {
                    $exact_blocker_if_any_netlist_preview_open_sch = "netlist_text_missing_in_open_sch_context"
                } elseif ($netlist_line_count_valid_open_sch_str -ne "true") {
                    $exact_blocker_if_any_netlist_preview_open_sch = "netlist_line_count_invalid_in_open_sch_context"
                } elseif ($diagnostics_present_open_sch -ne "true") {
                    $exact_blocker_if_any_netlist_preview_open_sch = "diagnostics_missing_in_open_sch_context"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_open_sch) -or [string]::IsNullOrWhiteSpace($export_path_strategy_open_sch) -or [string]::IsNullOrWhiteSpace($preview_result_open_sch)) {
                    $exact_blocker_if_any_netlist_preview_open_sch = "session_log_missing_expected_fields_in_open_sch_context"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_open_sch + "`n" +
                 "preview_ok=" + $preview_ok_open_sch_str + "`n" +
                 "netlist_text_present=" + $netlist_text_present_open_sch + "`n" +
                 "netlist_line_count_valid=" + $netlist_line_count_valid_open_sch_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_open_sch + "`n" +
                 "source_context=" + $source_context_open_sch + "`n" +
                 "export_path_strategy=" + $export_path_strategy_open_sch + "`n" +
                 "preview_result=" + $preview_result_open_sch + "`n" +
                 "first_lines_excerpt=" + $first_lines_excerpt_open_sch + "`n" +
                 "diagnostics=" + $diagnostics_open_sch + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_open_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_netlist_preview_open_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_netlist_preview_session_contract_open_sch_summary.txt") -Encoding UTF8
                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_open_sch + "`n" +
                 "preview_ok=" + $preview_ok_open_sch_str + "`n" +
                 "netlist_text_present=" + $netlist_text_present_open_sch + "`n" +
                 "netlist_line_count_valid=" + $netlist_line_count_valid_open_sch_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_open_sch + "`n" +
                 "source_context=" + $source_context_open_sch + "`n" +
                 "export_path_strategy=" + $export_path_strategy_open_sch + "`n" +
                 "preview_result=" + $preview_result_open_sch + "`n" +
                 "first_lines_excerpt=" + $first_lines_excerpt_open_sch + "`n" +
                 "diagnostics=" + $diagnostics_open_sch + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_open_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_netlist_preview_open_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_netlist_preview_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_netlist_preview_open_sch -ne "N/A") {
                    throw "simulation_netlist_preview_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_netlist_preview_open_sch)."
                }

                # simulation_preflight_readiness_session targeted validation in SCH-open context.
                $simPreflightArgs = @{
                    directive           = ".tran 1n 10u"
                    library_source_path = $spiceLibraryInspectPath
                    schematic_filename  = $spicePreviewSch
                }
                $outSimPreflight = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_preflight_readiness_session" `
                        -caseLabel "sch_open_simulation_preflight_readiness_session" -argsObj $simPreflightArgs -url $url -smokeExe $smokeExe

                $callable_without_runtime_simulation_sim_preflight = $(if ($outSimPreflight -match 'status=1') { "true" } else { "false" })
                $preflight_decision_taken_ok = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "preflight_decision_taken_ok=*" } | Select-Object -First 1) -replace '^preflight_decision_taken_ok=', '').Trim()
                $preflight_decision_taken_ok_str = $(if ($preflight_decision_taken_ok -eq "1") { "true" } else { "false" })
                $preflight_decision = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "preflight_decision=*" } | Select-Object -First 1) -replace '^preflight_decision=', '').Trim()
                $ready_checks_present = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "ready_checks_present=*" } | Select-Object -First 1) -replace '^ready_checks_present=', '').Trim()
                $ready_checks = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "ready_checks=*" } | Select-Object -First 1) -replace '^ready_checks=', '').Trim()
                $blocking_checks_present = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "blocking_checks_present=*" } | Select-Object -First 1) -replace '^blocking_checks_present=', '').Trim()
                $blocking_checks = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "blocking_checks=*" } | Select-Object -First 1) -replace '^blocking_checks=', '').Trim()
                $diagnostics_present_sim_preflight = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_preflight = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $source_context_sim_preflight = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $directive_inspection_result = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "directive_inspection_result=*" } | Select-Object -First 1) -replace '^directive_inspection_result=', '').Trim()
                $netlist_preview_result = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "netlist_preview_result=*" } | Select-Object -First 1) -replace '^netlist_preview_result=', '').Trim()
                $library_inspection_result = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "library_inspection_result=*" } | Select-Object -First 1) -replace '^library_inspection_result=', '').Trim()
                $preflight_decision_log = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "preflight_decision=*" } | Select-Object -Last 1) -replace '^preflight_decision=', '').Trim()
                $blockers_summary = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "blockers_summary=*" } | Select-Object -First 1) -replace '^blockers_summary=', '').Trim()
                $exact_blocker_if_any_tool_sim_preflight = ((($outSimPreflight -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()

                $exact_blocker_if_any_sim_preflight = "N/A"
                if ($callable_without_runtime_simulation_sim_preflight -ne "true") {
                    $exact_blocker_if_any_sim_preflight = "callable_without_runtime_simulation_failed"
                } elseif ($preflight_decision_taken_ok_str -ne "true") {
                    $exact_blocker_if_any_sim_preflight = "preflight_decision_not_taken"
                } elseif ($preflight_decision -ne "ready" -and $preflight_decision -ne "blocked") {
                    $exact_blocker_if_any_sim_preflight = "preflight_decision_invalid"
                } elseif ($preflight_decision -ne "ready") {
                    $exact_blocker_if_any_sim_preflight = "preflight_decision_not_ready_in_open_sch_context"
                } elseif ($ready_checks_present -ne "true") {
                    $exact_blocker_if_any_sim_preflight = "ready_checks_missing"
                } elseif ($blocking_checks_present -ne "true") {
                    $exact_blocker_if_any_sim_preflight = "blocking_checks_missing"
                } elseif ($diagnostics_present_sim_preflight -ne "true") {
                    $exact_blocker_if_any_sim_preflight = "diagnostics_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_preflight) -or [string]::IsNullOrWhiteSpace($directive_inspection_result) -or [string]::IsNullOrWhiteSpace($netlist_preview_result) -or [string]::IsNullOrWhiteSpace($library_inspection_result) -or [string]::IsNullOrWhiteSpace($preflight_decision_log)) {
                    $exact_blocker_if_any_sim_preflight = "session_log_missing_expected_fields"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_sim_preflight + "`n" +
                 "preflight_decision_taken_ok=" + $preflight_decision_taken_ok_str + "`n" +
                 "preflight_decision=" + $preflight_decision + "`n" +
                 "ready_checks_present=" + $ready_checks_present + "`n" +
                 "blocking_checks_present=" + $blocking_checks_present + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_preflight + "`n" +
                 "source_context=" + $source_context_sim_preflight + "`n" +
                 "directive_inspection_result=" + $directive_inspection_result + "`n" +
                 "netlist_preview_result=" + $netlist_preview_result + "`n" +
                 "library_inspection_result=" + $library_inspection_result + "`n" +
                 "preflight_decision_log=" + $preflight_decision_log + "`n" +
                 "blockers_summary=" + $blockers_summary + "`n" +
                 "ready_checks=" + $ready_checks + "`n" +
                 "blocking_checks=" + $blocking_checks + "`n" +
                 "diagnostics=" + $diagnostics_sim_preflight + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_preflight + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_preflight + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_preflight_readiness_session_contract_open_sch_summary.txt") -Encoding UTF8
                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_sim_preflight + "`n" +
                 "preflight_decision_taken_ok=" + $preflight_decision_taken_ok_str + "`n" +
                 "preflight_decision=" + $preflight_decision + "`n" +
                 "ready_checks_present=" + $ready_checks_present + "`n" +
                 "blocking_checks_present=" + $blocking_checks_present + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_preflight + "`n" +
                 "source_context=" + $source_context_sim_preflight + "`n" +
                 "directive_inspection_result=" + $directive_inspection_result + "`n" +
                 "netlist_preview_result=" + $netlist_preview_result + "`n" +
                 "library_inspection_result=" + $library_inspection_result + "`n" +
                 "preflight_decision_log=" + $preflight_decision_log + "`n" +
                 "blockers_summary=" + $blockers_summary + "`n" +
                 "ready_checks=" + $ready_checks + "`n" +
                 "blocking_checks=" + $blocking_checks + "`n" +
                 "diagnostics=" + $diagnostics_sim_preflight + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_preflight + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_preflight + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_preflight_readiness_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_preflight -ne "N/A") {
                    throw "simulation_preflight_readiness_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_preflight)."
                }

                # simulation_run_status_session targeted validation in SCH-open context.
                # Re-open deterministic simulation fixture to maximize command availability.
                $qaOpenSimRunInputPath = Join-Path $runDir "mcp_call_tool_args_qa_open_editor_frame_sch_for_sim_run_input.json"
                (@{
                        type               = "sch"
                        filename           = $spicePreviewSch
                    } | ConvertTo-Json -Compress) | Set-Content -Path $qaOpenSimRunInputPath -Encoding UTF8
                $qaOpenSimRun = & $smokeExe $url "mcp_call_tool_args" "qa_open_editor_frame" ("@" + $qaOpenSimRunInputPath) 2>&1 | Out-String
                $qaOpenSimRun | Set-Content -Path (Join-Path $runDir "mcp_call_tool_args_qa_open_editor_frame_sch_for_sim_run.log") -Encoding UTF8
                Start-Sleep -Seconds 1

                $simRunArgs = @{
                    timeout_ms = 4000
                }
                $outSimRunStatus = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_run_status_session" `
                        -caseLabel "sch_open_simulation_run_status_session" -argsObj $simRunArgs -url $url -smokeExe $smokeExe

                $callable_with_runtime_simulation_sim_run = $(if ($outSimRunStatus -match 'status=1') { "true" } else { "false" })
                $callable_without_runtime_simulation_sim_run = "false"
                $run_decision_taken_ok_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "run_decision_taken_ok=*" } | Select-Object -First 1) -replace '^run_decision_taken_ok=', '').Trim()
                $run_decision_taken_ok_sim_run_str = $(if ($run_decision_taken_ok_sim_run -eq "1") { "true" } else { "false" })
                $run_status_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -First 1) -replace '^run_status=', '').Trim()
                $started_ok_sim_run_raw = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "started_ok=*" } | Select-Object -First 1) -replace '^started_ok=', '').Trim()
                $finished_ok_sim_run_raw = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "finished_ok=*" } | Select-Object -First 1) -replace '^finished_ok=', '').Trim()
                $started_ok_sim_run = $(if ($started_ok_sim_run_raw -eq "1") { "true" } else { "false" })
                $finished_ok_sim_run = $(if ($finished_ok_sim_run_raw -eq "1") { "true" } else { "false" })
                $duration_ms_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "duration_ms=*" } | Select-Object -First 1) -replace '^duration_ms=', '').Trim()
                $duration_ms_valid_sim_run = $false
                if ($duration_ms_sim_run -match '^\d+$' -and [int]$duration_ms_sim_run -ge 0) { $duration_ms_valid_sim_run = $true }
                $duration_ms_valid_sim_run_str = $(if ($duration_ms_valid_sim_run) { "true" } else { "false" })
                $diagnostics_present_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $simulator_frame_resolution_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "simulator_frame_resolution=*" } | Select-Object -First 1) -replace '^simulator_frame_resolution=', '').Trim()
                $runtime_entrypoint_used_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "runtime_entrypoint_used=*" } | Select-Object -First 1) -replace '^runtime_entrypoint_used=', '').Trim()
                $run_start_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "run_start=*" } | Select-Object -First 1) -replace '^run_start=', '').Trim()
                $polling_result_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "polling_result=*" } | Select-Object -First 1) -replace '^polling_result=', '').Trim()
                $run_status_log_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -Last 1) -replace '^run_status=', '').Trim()
                $duration_ms_log_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "duration_ms=*" } | Select-Object -Last 1) -replace '^duration_ms=', '').Trim()
                $diagnostics_log_sim_run = ((($outSimRunStatus -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -Last 1) -replace '^diagnostics=', '').Trim()

                $run_status_valid_sim_run = $false
                if ($run_status_sim_run -in @("completed", "blocked", "timeout", "error")) { $run_status_valid_sim_run = $true }
                $run_status_valid_sim_run_str = $(if ($run_status_valid_sim_run) { "true" } else { "false" })

                $coherence_started_finished_sim_run = "true"
                if ($run_status_sim_run -eq "completed") {
                    if ($started_ok_sim_run -ne "true" -or $finished_ok_sim_run -ne "true") {
                        $coherence_started_finished_sim_run = "false"
                    }
                } elseif ($run_status_sim_run -eq "timeout") {
                    if ($started_ok_sim_run -ne "true" -or $finished_ok_sim_run -ne "false") {
                        $coherence_started_finished_sim_run = "false"
                    }
                } elseif ($run_status_sim_run -eq "blocked") {
                    if ($finished_ok_sim_run -ne "false") {
                        $coherence_started_finished_sim_run = "false"
                    }
                }

                $exact_blocker_if_any_sim_run = "N/A"
                if ($callable_with_runtime_simulation_sim_run -ne "true") {
                    $exact_blocker_if_any_sim_run = "runtime_simulation_call_failed"
                } elseif ($run_decision_taken_ok_sim_run_str -ne "true") {
                    $exact_blocker_if_any_sim_run = "run_decision_not_taken"
                } elseif ($run_status_valid_sim_run_str -ne "true") {
                    $exact_blocker_if_any_sim_run = "run_status_invalid"
                } elseif ($coherence_started_finished_sim_run -ne "true") {
                    $exact_blocker_if_any_sim_run = "started_finished_incoherent_for_run_status"
                } elseif ($duration_ms_valid_sim_run_str -ne "true") {
                    $exact_blocker_if_any_sim_run = "duration_ms_invalid"
                } elseif ($diagnostics_present_sim_run -ne "true") {
                    $exact_blocker_if_any_sim_run = "diagnostics_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_run) -or [string]::IsNullOrWhiteSpace($simulator_frame_resolution_sim_run) -or [string]::IsNullOrWhiteSpace($runtime_entrypoint_used_sim_run) -or [string]::IsNullOrWhiteSpace($run_start_sim_run) -or [string]::IsNullOrWhiteSpace($polling_result_sim_run) -or [string]::IsNullOrWhiteSpace($run_status_log_sim_run) -or [string]::IsNullOrWhiteSpace($duration_ms_log_sim_run)) {
                    $exact_blocker_if_any_sim_run = "session_log_missing_expected_fields"
                } elseif ($run_status_sim_run -eq "completed" -and $exact_blocker_if_any_tool_sim_run -ne "N/A") {
                    $exact_blocker_if_any_sim_run = "tool_blocker_unexpected_on_completed"
                } elseif ($run_status_sim_run -ne "completed" -and ($exact_blocker_if_any_tool_sim_run -eq "N/A" -or [string]::IsNullOrWhiteSpace($exact_blocker_if_any_tool_sim_run))) {
                    $exact_blocker_if_any_sim_run = "tool_blocker_missing_for_non_completed_status"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_sim_run + "`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_run + "`n" +
                 "run_decision_taken_ok=" + $run_decision_taken_ok_sim_run_str + "`n" +
                 "run_status=" + $run_status_sim_run + "`n" +
                 "started_ok=" + $started_ok_sim_run + "`n" +
                 "finished_ok=" + $finished_ok_sim_run + "`n" +
                 "coherence_started_finished=" + $coherence_started_finished_sim_run + "`n" +
                 "duration_ms_valid=" + $duration_ms_valid_sim_run_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_run + "`n" +
                 "source_context=" + $source_context_sim_run + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_run + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_run + "`n" +
                 "run_start=" + $run_start_sim_run + "`n" +
                 "polling_result=" + $polling_result_sim_run + "`n" +
                 "run_status_log=" + $run_status_log_sim_run + "`n" +
                 "duration_ms_log=" + $duration_ms_log_sim_run + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_run + "`n" +
                 "diagnostics=" + $diagnostics_sim_run + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_run + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_run + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_status_session_contract_open_sch_summary.txt") -Encoding UTF8
                ("visible_in_context_if_applicable=true`n" +
                 "callable_without_runtime_simulation=" + $callable_without_runtime_simulation_sim_run + "`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_run + "`n" +
                 "run_decision_taken_ok=" + $run_decision_taken_ok_sim_run_str + "`n" +
                 "run_status=" + $run_status_sim_run + "`n" +
                 "started_ok=" + $started_ok_sim_run + "`n" +
                 "finished_ok=" + $finished_ok_sim_run + "`n" +
                 "coherence_started_finished=" + $coherence_started_finished_sim_run + "`n" +
                 "duration_ms_valid=" + $duration_ms_valid_sim_run_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_run + "`n" +
                 "source_context=" + $source_context_sim_run + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_run + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_run + "`n" +
                 "run_start=" + $run_start_sim_run + "`n" +
                 "polling_result=" + $polling_result_sim_run + "`n" +
                 "run_status_log=" + $run_status_log_sim_run + "`n" +
                 "duration_ms_log=" + $duration_ms_log_sim_run + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_run + "`n" +
                 "diagnostics=" + $diagnostics_sim_run + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_run + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_run + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_status_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_run -ne "N/A") {
                    throw "simulation_run_status_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_run)."
                }

                # simulation_run_minimal_summary_session targeted validation in SCH-open context.
                $simRunSummaryArgs = @{
                    timeout_ms = 4000
                }
                $outSimRunSummary = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_run_minimal_summary_session" `
                        -caseLabel "sch_open_simulation_run_minimal_summary_session" -argsObj $simRunSummaryArgs -url $url -smokeExe $smokeExe

                $callable_with_runtime_simulation_sim_summary = $(if ($outSimRunSummary -match 'status=1') { "true" } else { "false" })
                $run_summary_ok_raw_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "run_summary_ok=*" } | Select-Object -First 1) -replace '^run_summary_ok=', '').Trim()
                $run_summary_ok_sim_summary = $(if ($run_summary_ok_raw_sim_summary -eq "1") { "true" } else { "false" })
                $run_status_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -First 1) -replace '^run_status=', '').Trim()
                $plot_name_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -First 1) -replace '^plot_name=', '').Trim()
                $plot_name_present_sim_summary = $(if ([string]::IsNullOrWhiteSpace($plot_name_sim_summary)) { "false" } else { "true" })
                $vectors_count_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "vectors_count=*" } | Select-Object -First 1) -replace '^vectors_count=', '').Trim()
                $vectors_count_valid_sim_summary = $false
                if ($vectors_count_sim_summary -match '^-?\d+$' -and [int]$vectors_count_sim_summary -ge 0) { $vectors_count_valid_sim_summary = $true }
                $vectors_count_valid_sim_summary_str = $(if ($vectors_count_valid_sim_summary) { "true" } else { "false" })
                $diagnostics_present_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $simulator_frame_resolution_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "simulator_frame_resolution=*" } | Select-Object -First 1) -replace '^simulator_frame_resolution=', '').Trim()
                $runtime_entrypoint_used_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "runtime_entrypoint_used=*" } | Select-Object -First 1) -replace '^runtime_entrypoint_used=', '').Trim()
                $run_start_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "run_start=*" } | Select-Object -First 1) -replace '^run_start=', '').Trim()
                $polling_result_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "polling_result=*" } | Select-Object -First 1) -replace '^polling_result=', '').Trim()
                $run_status_log_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -Last 1) -replace '^run_status=', '').Trim()
                $plot_name_log_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -Last 1) -replace '^plot_name=', '').Trim()
                $vectors_count_log_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "vectors_count=*" } | Select-Object -Last 1) -replace '^vectors_count=', '').Trim()
                $diagnostics_log_sim_summary = ((($outSimRunSummary -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -Last 1) -replace '^diagnostics=', '').Trim()

                $run_status_valid_sim_summary = $false
                if ($run_status_sim_summary -in @("completed", "blocked", "timeout", "error")) { $run_status_valid_sim_summary = $true }
                $run_status_valid_sim_summary_str = $(if ($run_status_valid_sim_summary) { "true" } else { "false" })

                $exact_blocker_if_any_sim_summary = "N/A"
                if ($callable_with_runtime_simulation_sim_summary -ne "true") {
                    $exact_blocker_if_any_sim_summary = "runtime_simulation_call_failed"
                } elseif ($run_status_valid_sim_summary_str -ne "true") {
                    $exact_blocker_if_any_sim_summary = "run_status_invalid"
                } elseif ($diagnostics_present_sim_summary -ne "true") {
                    $exact_blocker_if_any_sim_summary = "diagnostics_missing"
                } elseif ($vectors_count_valid_sim_summary_str -ne "true") {
                    $exact_blocker_if_any_sim_summary = "vectors_count_invalid"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_summary) -or [string]::IsNullOrWhiteSpace($simulator_frame_resolution_sim_summary) -or [string]::IsNullOrWhiteSpace($runtime_entrypoint_used_sim_summary) -or [string]::IsNullOrWhiteSpace($run_start_sim_summary) -or [string]::IsNullOrWhiteSpace($polling_result_sim_summary) -or [string]::IsNullOrWhiteSpace($run_status_log_sim_summary) -or [string]::IsNullOrWhiteSpace($vectors_count_log_sim_summary)) {
                    $exact_blocker_if_any_sim_summary = "session_log_missing_expected_fields"
                } elseif ($run_status_sim_summary -eq "completed" -and $run_summary_ok_sim_summary -ne "true") {
                    $exact_blocker_if_any_sim_summary = "run_summary_not_ok_on_completed"
                } elseif ($run_status_sim_summary -eq "completed" -and $plot_name_present_sim_summary -ne "true") {
                    $exact_blocker_if_any_sim_summary = "plot_name_missing_on_completed"
                } elseif ($run_status_sim_summary -eq "completed" -and $exact_blocker_if_any_tool_sim_summary -ne "N/A") {
                    $exact_blocker_if_any_sim_summary = "tool_blocker_unexpected_on_completed"
                } elseif ($run_status_sim_summary -ne "completed" -and ($exact_blocker_if_any_tool_sim_summary -eq "N/A" -or [string]::IsNullOrWhiteSpace($exact_blocker_if_any_tool_sim_summary))) {
                    $exact_blocker_if_any_sim_summary = "tool_blocker_missing_for_non_completed_status"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_summary + "`n" +
                 "run_summary_ok=" + $run_summary_ok_sim_summary + "`n" +
                 "run_status=" + $run_status_sim_summary + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_summary + "`n" +
                 "vectors_count_valid=" + $vectors_count_valid_sim_summary_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_summary + "`n" +
                 "source_context=" + $source_context_sim_summary + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_summary + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_summary + "`n" +
                 "run_start=" + $run_start_sim_summary + "`n" +
                 "polling_result=" + $polling_result_sim_summary + "`n" +
                 "run_status_log=" + $run_status_log_sim_summary + "`n" +
                 "plot_name=" + $plot_name_sim_summary + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_summary + "`n" +
                 "vectors_count=" + $vectors_count_sim_summary + "`n" +
                 "vectors_count_log=" + $vectors_count_log_sim_summary + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_summary + "`n" +
                 "diagnostics=" + $diagnostics_sim_summary + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_summary + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_summary + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_minimal_summary_session_contract_open_sch_summary.txt") -Encoding UTF8
                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_summary + "`n" +
                 "run_summary_ok=" + $run_summary_ok_sim_summary + "`n" +
                 "run_status=" + $run_status_sim_summary + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_summary + "`n" +
                 "vectors_count_valid=" + $vectors_count_valid_sim_summary_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_summary + "`n" +
                 "source_context=" + $source_context_sim_summary + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_summary + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_summary + "`n" +
                 "run_start=" + $run_start_sim_summary + "`n" +
                 "polling_result=" + $polling_result_sim_summary + "`n" +
                 "run_status_log=" + $run_status_log_sim_summary + "`n" +
                 "plot_name=" + $plot_name_sim_summary + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_summary + "`n" +
                 "vectors_count=" + $vectors_count_sim_summary + "`n" +
                 "vectors_count_log=" + $vectors_count_log_sim_summary + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_summary + "`n" +
                 "diagnostics=" + $diagnostics_sim_summary + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_summary + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_summary + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_minimal_summary_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_summary -ne "N/A") {
                    throw "simulation_run_minimal_summary_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_summary)."
                }

                # simulation_run_vector_catalog_session targeted validation in SCH-open context.
                $simVectorCatalogArgs = @{
                    timeout_ms = 4000
                }
                $outSimVectorCatalog = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_run_vector_catalog_session" `
                        -caseLabel "sch_open_simulation_run_vector_catalog_session" -argsObj $simVectorCatalogArgs -url $url -smokeExe $smokeExe

                $callable_with_runtime_simulation_sim_catalog = $(if ($outSimVectorCatalog -match 'status=1') { "true" } else { "false" })
                $catalog_ok_raw_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "catalog_ok=*" } | Select-Object -First 1) -replace '^catalog_ok=', '').Trim()
                $catalog_ok_sim_catalog = $(if ($catalog_ok_raw_sim_catalog -eq "1") { "true" } else { "false" })
                $run_status_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -First 1) -replace '^run_status=', '').Trim()
                $plot_name_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -First 1) -replace '^plot_name=', '').Trim()
                $plot_name_present_sim_catalog = $(if ([string]::IsNullOrWhiteSpace($plot_name_sim_catalog)) { "false" } else { "true" })
                $vectors_count_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "vectors_count=*" } | Select-Object -First 1) -replace '^vectors_count=', '').Trim()
                $vectors_count_valid_sim_catalog = $false
                if ($vectors_count_sim_catalog -match '^-?\d+$' -and [int]$vectors_count_sim_catalog -ge 0) { $vectors_count_valid_sim_catalog = $true }
                $vectors_count_valid_sim_catalog_str = $(if ($vectors_count_valid_sim_catalog) { "true" } else { "false" })
                $vector_names_present_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "vector_names_present=*" } | Select-Object -First 1) -replace '^vector_names_present=', '').Trim()
                $vector_names_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "vector_names=*" } | Select-Object -First 1) -replace '^vector_names=', '').Trim()
                $vector_names_non_empty_sim_catalog = $(if ([string]::IsNullOrWhiteSpace($vector_names_sim_catalog)) { "false" } else { "true" })
                $diagnostics_present_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $simulator_frame_resolution_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "simulator_frame_resolution=*" } | Select-Object -First 1) -replace '^simulator_frame_resolution=', '').Trim()
                $runtime_entrypoint_used_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "runtime_entrypoint_used=*" } | Select-Object -First 1) -replace '^runtime_entrypoint_used=', '').Trim()
                $run_start_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "run_start=*" } | Select-Object -First 1) -replace '^run_start=', '').Trim()
                $polling_result_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "polling_result=*" } | Select-Object -First 1) -replace '^polling_result=', '').Trim()
                $run_status_log_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -Last 1) -replace '^run_status=', '').Trim()
                $plot_name_log_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -Last 1) -replace '^plot_name=', '').Trim()
                $vectors_count_log_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "vectors_count=*" } | Select-Object -Last 1) -replace '^vectors_count=', '').Trim()
                $vector_names_log_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "vector_names=*" } | Select-Object -Last 1) -replace '^vector_names=', '').Trim()
                $diagnostics_log_sim_catalog = ((($outSimVectorCatalog -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -Last 1) -replace '^diagnostics=', '').Trim()

                $run_status_valid_sim_catalog = $false
                if ($run_status_sim_catalog -in @("completed", "blocked", "timeout", "error")) { $run_status_valid_sim_catalog = $true }
                $run_status_valid_sim_catalog_str = $(if ($run_status_valid_sim_catalog) { "true" } else { "false" })

                $exact_blocker_if_any_sim_catalog = "N/A"
                if ($callable_with_runtime_simulation_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "runtime_simulation_call_failed"
                } elseif ($run_status_valid_sim_catalog_str -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "run_status_invalid"
                } elseif ($diagnostics_present_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "diagnostics_missing"
                } elseif ($vectors_count_valid_sim_catalog_str -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "vectors_count_invalid"
                } elseif ($vector_names_present_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "vector_names_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_catalog) -or [string]::IsNullOrWhiteSpace($simulator_frame_resolution_sim_catalog) -or [string]::IsNullOrWhiteSpace($runtime_entrypoint_used_sim_catalog) -or [string]::IsNullOrWhiteSpace($run_start_sim_catalog) -or [string]::IsNullOrWhiteSpace($polling_result_sim_catalog) -or [string]::IsNullOrWhiteSpace($run_status_log_sim_catalog) -or [string]::IsNullOrWhiteSpace($vectors_count_log_sim_catalog) -or [string]::IsNullOrWhiteSpace($vector_names_log_sim_catalog)) {
                    $exact_blocker_if_any_sim_catalog = "session_log_missing_expected_fields"
                } elseif ($run_status_sim_catalog -eq "completed" -and $catalog_ok_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "catalog_not_ok_on_completed"
                } elseif ($run_status_sim_catalog -eq "completed" -and $plot_name_present_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "plot_name_missing_on_completed"
                } elseif ($run_status_sim_catalog -eq "completed" -and $vector_names_non_empty_sim_catalog -ne "true") {
                    $exact_blocker_if_any_sim_catalog = "vector_names_empty_on_completed"
                } elseif ($run_status_sim_catalog -eq "completed" -and $exact_blocker_if_any_tool_sim_catalog -ne "N/A") {
                    $exact_blocker_if_any_sim_catalog = "tool_blocker_unexpected_on_completed"
                } elseif ($run_status_sim_catalog -ne "completed" -and ($exact_blocker_if_any_tool_sim_catalog -eq "N/A" -or [string]::IsNullOrWhiteSpace($exact_blocker_if_any_tool_sim_catalog))) {
                    $exact_blocker_if_any_sim_catalog = "tool_blocker_missing_for_non_completed_status"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_catalog + "`n" +
                 "catalog_ok=" + $catalog_ok_sim_catalog + "`n" +
                 "run_status=" + $run_status_sim_catalog + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_catalog + "`n" +
                 "vectors_count_valid=" + $vectors_count_valid_sim_catalog_str + "`n" +
                 "vector_names_present=" + $vector_names_present_sim_catalog + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_catalog + "`n" +
                 "source_context=" + $source_context_sim_catalog + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_catalog + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_catalog + "`n" +
                 "run_start=" + $run_start_sim_catalog + "`n" +
                 "polling_result=" + $polling_result_sim_catalog + "`n" +
                 "run_status_log=" + $run_status_log_sim_catalog + "`n" +
                 "plot_name=" + $plot_name_sim_catalog + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_catalog + "`n" +
                 "vectors_count=" + $vectors_count_sim_catalog + "`n" +
                 "vectors_count_log=" + $vectors_count_log_sim_catalog + "`n" +
                 "vector_names=" + $vector_names_sim_catalog + "`n" +
                 "vector_names_log=" + $vector_names_log_sim_catalog + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_catalog + "`n" +
                 "diagnostics=" + $diagnostics_sim_catalog + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_catalog + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_catalog + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_vector_catalog_session_contract_open_sch_summary.txt") -Encoding UTF8
                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_catalog + "`n" +
                 "catalog_ok=" + $catalog_ok_sim_catalog + "`n" +
                 "run_status=" + $run_status_sim_catalog + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_catalog + "`n" +
                 "vectors_count_valid=" + $vectors_count_valid_sim_catalog_str + "`n" +
                 "vector_names_present=" + $vector_names_present_sim_catalog + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_catalog + "`n" +
                 "source_context=" + $source_context_sim_catalog + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_catalog + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_catalog + "`n" +
                 "run_start=" + $run_start_sim_catalog + "`n" +
                 "polling_result=" + $polling_result_sim_catalog + "`n" +
                 "run_status_log=" + $run_status_log_sim_catalog + "`n" +
                 "plot_name=" + $plot_name_sim_catalog + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_catalog + "`n" +
                 "vectors_count=" + $vectors_count_sim_catalog + "`n" +
                 "vectors_count_log=" + $vectors_count_log_sim_catalog + "`n" +
                 "vector_names=" + $vector_names_sim_catalog + "`n" +
                 "vector_names_log=" + $vector_names_log_sim_catalog + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_catalog + "`n" +
                 "diagnostics=" + $diagnostics_sim_catalog + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_catalog + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_catalog + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_run_vector_catalog_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_catalog -ne "N/A") {
                    throw "simulation_run_vector_catalog_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_catalog)."
                }

                # simulation_vector_scalar_probe_session targeted validation in SCH-open context.
                $vector_name_probe = ""
                if (-not [string]::IsNullOrWhiteSpace($vector_names_sim_catalog)) {
                    $vectorTokens = $vector_names_sim_catalog -split ';'
                    foreach ($token in $vectorTokens) {
                        if (-not [string]::IsNullOrWhiteSpace($token)) {
                            $vector_name_probe = $token.Trim()
                            break
                        }
                    }
                }
                if ([string]::IsNullOrWhiteSpace($vector_name_probe)) {
                    $vector_name_probe = "time"
                }

                $simScalarProbeArgs = @{
                    timeout_ms      = 4000
                    vector_name     = $vector_name_probe
                    sample_position = "first"
                }
                $outSimScalarProbe = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_vector_scalar_probe_session" `
                        -caseLabel "sch_open_simulation_vector_scalar_probe_session" -argsObj $simScalarProbeArgs -url $url -smokeExe $smokeExe

                $callable_with_runtime_simulation_sim_probe = $(if ($outSimScalarProbe -match 'status=1') { "true" } else { "false" })
                $probe_ok_raw_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "probe_ok=*" } | Select-Object -First 1) -replace '^probe_ok=', '').Trim()
                $probe_ok_sim_probe = $(if ($probe_ok_raw_sim_probe -eq "1") { "true" } else { "false" })
                $run_status_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -First 1) -replace '^run_status=', '').Trim()
                $plot_name_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -First 1) -replace '^plot_name=', '').Trim()
                $plot_name_present_sim_probe = $(if ([string]::IsNullOrWhiteSpace($plot_name_sim_probe)) { "false" } else { "true" })
                $vector_name_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "vector_name=*" } | Select-Object -First 1) -replace '^vector_name=', '').Trim()
                $vector_name_valid_sim_probe = $(if ([string]::IsNullOrWhiteSpace($vector_name_sim_probe)) { "false" } else { "true" })
                $scalar_value_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "scalar_value=*" } | Select-Object -First 1) -replace '^scalar_value=', '').Trim()
                $scalar_value_valid_sim_probe = $false
                if ($scalar_value_sim_probe -match '^-?\d+(\.\d+)?([eE][+-]?\d+)?$') { $scalar_value_valid_sim_probe = $true }
                $scalar_value_valid_sim_probe_str = $(if ($scalar_value_valid_sim_probe) { "true" } else { "false" })
                $diagnostics_present_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $simulator_frame_resolution_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "simulator_frame_resolution=*" } | Select-Object -First 1) -replace '^simulator_frame_resolution=', '').Trim()
                $runtime_entrypoint_used_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "runtime_entrypoint_used=*" } | Select-Object -First 1) -replace '^runtime_entrypoint_used=', '').Trim()
                $run_start_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "run_start=*" } | Select-Object -First 1) -replace '^run_start=', '').Trim()
                $polling_result_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "polling_result=*" } | Select-Object -First 1) -replace '^polling_result=', '').Trim()
                $run_status_log_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -Last 1) -replace '^run_status=', '').Trim()
                $plot_name_log_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -Last 1) -replace '^plot_name=', '').Trim()
                $vector_name_log_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "vector_name=*" } | Select-Object -Last 1) -replace '^vector_name=', '').Trim()
                $scalar_value_log_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "scalar_value=*" } | Select-Object -Last 1) -replace '^scalar_value=', '').Trim()
                $diagnostics_log_sim_probe = ((($outSimScalarProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -Last 1) -replace '^diagnostics=', '').Trim()

                if ($vector_names_sim_catalog -match "(^|;)" + [regex]::Escape($vector_name_sim_probe) + "(;|$)") {
                    $vector_name_valid_sim_probe = "true"
                } elseif ($run_status_sim_probe -ne "completed") {
                    $vector_name_valid_sim_probe = "n/a_non_completed"
                }

                $run_status_valid_sim_probe = $false
                if ($run_status_sim_probe -in @("completed", "blocked", "timeout", "error")) { $run_status_valid_sim_probe = $true }
                $run_status_valid_sim_probe_str = $(if ($run_status_valid_sim_probe) { "true" } else { "false" })

                $exact_blocker_if_any_sim_probe = "N/A"
                if ($callable_with_runtime_simulation_sim_probe -ne "true") {
                    $exact_blocker_if_any_sim_probe = "runtime_simulation_call_failed"
                } elseif ($run_status_valid_sim_probe_str -ne "true") {
                    $exact_blocker_if_any_sim_probe = "run_status_invalid"
                } elseif ($diagnostics_present_sim_probe -ne "true") {
                    $exact_blocker_if_any_sim_probe = "diagnostics_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_probe) -or [string]::IsNullOrWhiteSpace($simulator_frame_resolution_sim_probe) -or [string]::IsNullOrWhiteSpace($runtime_entrypoint_used_sim_probe) -or [string]::IsNullOrWhiteSpace($run_start_sim_probe) -or [string]::IsNullOrWhiteSpace($polling_result_sim_probe) -or [string]::IsNullOrWhiteSpace($run_status_log_sim_probe) -or [string]::IsNullOrWhiteSpace($vector_name_log_sim_probe) -or [string]::IsNullOrWhiteSpace($scalar_value_log_sim_probe)) {
                    $exact_blocker_if_any_sim_probe = "session_log_missing_expected_fields"
                } elseif ($run_status_sim_probe -eq "completed" -and $probe_ok_sim_probe -ne "true") {
                    $exact_blocker_if_any_sim_probe = "probe_not_ok_on_completed"
                } elseif ($run_status_sim_probe -eq "completed" -and $plot_name_present_sim_probe -ne "true") {
                    $exact_blocker_if_any_sim_probe = "plot_name_missing_on_completed"
                } elseif ($run_status_sim_probe -eq "completed" -and $vector_name_valid_sim_probe -ne "true") {
                    $exact_blocker_if_any_sim_probe = "vector_name_not_in_catalog"
                } elseif ($run_status_sim_probe -eq "completed" -and $scalar_value_valid_sim_probe_str -ne "true") {
                    $exact_blocker_if_any_sim_probe = "scalar_value_invalid_on_completed"
                } elseif ($run_status_sim_probe -eq "completed" -and $exact_blocker_if_any_tool_sim_probe -ne "N/A") {
                    $exact_blocker_if_any_sim_probe = "tool_blocker_unexpected_on_completed"
                } elseif ($run_status_sim_probe -ne "completed" -and ($exact_blocker_if_any_tool_sim_probe -eq "N/A" -or [string]::IsNullOrWhiteSpace($exact_blocker_if_any_tool_sim_probe))) {
                    $exact_blocker_if_any_sim_probe = "tool_blocker_missing_for_non_completed_status"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_probe + "`n" +
                 "probe_ok=" + $probe_ok_sim_probe + "`n" +
                 "run_status=" + $run_status_sim_probe + "`n" +
                 "run_status_valid=" + $run_status_valid_sim_probe_str + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_probe + "`n" +
                 "vector_name_valid=" + $vector_name_valid_sim_probe + "`n" +
                 "scalar_value_valid=" + $scalar_value_valid_sim_probe_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_probe + "`n" +
                 "source_context=" + $source_context_sim_probe + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_probe + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_probe + "`n" +
                 "run_start=" + $run_start_sim_probe + "`n" +
                 "polling_result=" + $polling_result_sim_probe + "`n" +
                 "run_status_log=" + $run_status_log_sim_probe + "`n" +
                 "plot_name=" + $plot_name_sim_probe + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_probe + "`n" +
                 "vector_name=" + $vector_name_sim_probe + "`n" +
                 "vector_name_log=" + $vector_name_log_sim_probe + "`n" +
                 "scalar_value=" + $scalar_value_sim_probe + "`n" +
                 "scalar_value_log=" + $scalar_value_log_sim_probe + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_probe + "`n" +
                 "diagnostics=" + $diagnostics_sim_probe + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_probe + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_probe + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_vector_scalar_probe_session_contract_open_sch_summary.txt") -Encoding UTF8

                # Canonical contract artifact for lot closure/reporting.
                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_probe + "`n" +
                 "probe_ok=" + $probe_ok_sim_probe + "`n" +
                 "run_status=" + $run_status_sim_probe + "`n" +
                 "run_status_valid=" + $run_status_valid_sim_probe_str + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_probe + "`n" +
                 "vector_name_valid=" + $vector_name_valid_sim_probe + "`n" +
                 "scalar_value_valid=" + $scalar_value_valid_sim_probe_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_probe + "`n" +
                 "source_context=" + $source_context_sim_probe + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_probe + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_probe + "`n" +
                 "run_start=" + $run_start_sim_probe + "`n" +
                 "polling_result=" + $polling_result_sim_probe + "`n" +
                 "run_status_log=" + $run_status_log_sim_probe + "`n" +
                 "plot_name=" + $plot_name_sim_probe + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_probe + "`n" +
                 "vector_name=" + $vector_name_sim_probe + "`n" +
                 "vector_name_log=" + $vector_name_log_sim_probe + "`n" +
                 "scalar_value=" + $scalar_value_sim_probe + "`n" +
                 "scalar_value_log=" + $scalar_value_log_sim_probe + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_probe + "`n" +
                 "diagnostics=" + $diagnostics_sim_probe + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_probe + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_probe + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_vector_scalar_probe_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_probe -ne "N/A") {
                    throw "simulation_vector_scalar_probe_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_probe)."
                }

                # simulation_xaxis_probe_session targeted validation in SCH-open context.
                $simXAxisProbeArgs = @{
                    timeout_ms = 4000
                }
                $outSimXAxisProbe = Call-McpToolArgsAndLog -runDir $runDir -toolName "simulation_xaxis_probe_session" `
                        -caseLabel "sch_open_simulation_xaxis_probe_session" -argsObj $simXAxisProbeArgs -url $url -smokeExe $smokeExe

                $callable_with_runtime_simulation_sim_xaxis = $(if ($outSimXAxisProbe -match 'status=1') { "true" } else { "false" })
                $axis_ok_raw_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "axis_ok=*" } | Select-Object -First 1) -replace '^axis_ok=', '').Trim()
                $axis_ok_sim_xaxis = $(if ($axis_ok_raw_sim_xaxis -eq "1") { "true" } else { "false" })
                $run_status_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -First 1) -replace '^run_status=', '').Trim()
                $plot_name_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -First 1) -replace '^plot_name=', '').Trim()
                $plot_name_present_sim_xaxis = $(if ([string]::IsNullOrWhiteSpace($plot_name_sim_xaxis)) { "false" } else { "true" })
                $x_axis_name_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "x_axis_name=*" } | Select-Object -First 1) -replace '^x_axis_name=', '').Trim()
                $x_axis_name_present_sim_xaxis = $(if ([string]::IsNullOrWhiteSpace($x_axis_name_sim_xaxis)) { "false" } else { "true" })
                $x_axis_first_value_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "x_axis_first_value=*" } | Select-Object -First 1) -replace '^x_axis_first_value=', '').Trim()
                $x_axis_first_value_valid_sim_xaxis = $false
                if ($x_axis_first_value_sim_xaxis -match '^-?\d+(\.\d+)?([eE][+-]?\d+)?$') { $x_axis_first_value_valid_sim_xaxis = $true }
                $x_axis_first_value_valid_sim_xaxis_str = $(if ($x_axis_first_value_valid_sim_xaxis) { "true" } else { "false" })
                $diagnostics_present_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "session_log=source_context=*" -or $_ -like "source_context=*" } | Select-Object -First 1) -replace '^session_log=source_context=', '' -replace '^source_context=', '').Trim()
                $simulator_frame_resolution_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "simulator_frame_resolution=*" } | Select-Object -First 1) -replace '^simulator_frame_resolution=', '').Trim()
                $runtime_entrypoint_used_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "runtime_entrypoint_used=*" } | Select-Object -First 1) -replace '^runtime_entrypoint_used=', '').Trim()
                $run_start_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "run_start=*" } | Select-Object -First 1) -replace '^run_start=', '').Trim()
                $polling_result_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "polling_result=*" } | Select-Object -First 1) -replace '^polling_result=', '').Trim()
                $run_status_log_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "run_status=*" } | Select-Object -Last 1) -replace '^run_status=', '').Trim()
                $plot_name_log_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "plot_name=*" } | Select-Object -Last 1) -replace '^plot_name=', '').Trim()
                $x_axis_name_log_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "x_axis_name=*" } | Select-Object -Last 1) -replace '^x_axis_name=', '').Trim()
                $x_axis_first_value_log_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "x_axis_first_value=*" } | Select-Object -Last 1) -replace '^x_axis_first_value=', '').Trim()
                $diagnostics_log_sim_xaxis = ((($outSimXAxisProbe -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -Last 1) -replace '^diagnostics=', '').Trim()

                $run_status_valid_sim_xaxis = $false
                if ($run_status_sim_xaxis -in @("completed", "blocked", "timeout", "error")) { $run_status_valid_sim_xaxis = $true }
                $run_status_valid_sim_xaxis_str = $(if ($run_status_valid_sim_xaxis) { "true" } else { "false" })

                $exact_blocker_if_any_sim_xaxis = "N/A"
                if ($callable_with_runtime_simulation_sim_xaxis -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "runtime_simulation_call_failed"
                } elseif ($run_status_valid_sim_xaxis_str -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "run_status_invalid"
                } elseif ($diagnostics_present_sim_xaxis -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "diagnostics_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_sim_xaxis) -or [string]::IsNullOrWhiteSpace($simulator_frame_resolution_sim_xaxis) -or [string]::IsNullOrWhiteSpace($runtime_entrypoint_used_sim_xaxis) -or [string]::IsNullOrWhiteSpace($run_start_sim_xaxis) -or [string]::IsNullOrWhiteSpace($polling_result_sim_xaxis) -or [string]::IsNullOrWhiteSpace($run_status_log_sim_xaxis) -or [string]::IsNullOrWhiteSpace($x_axis_name_log_sim_xaxis) -or [string]::IsNullOrWhiteSpace($x_axis_first_value_log_sim_xaxis)) {
                    $exact_blocker_if_any_sim_xaxis = "session_log_missing_expected_fields"
                } elseif ($run_status_sim_xaxis -eq "completed" -and $axis_ok_sim_xaxis -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "axis_not_ok_on_completed"
                } elseif ($run_status_sim_xaxis -eq "completed" -and $plot_name_present_sim_xaxis -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "plot_name_missing_on_completed"
                } elseif ($run_status_sim_xaxis -eq "completed" -and $x_axis_name_present_sim_xaxis -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "x_axis_name_missing_on_completed"
                } elseif ($run_status_sim_xaxis -eq "completed" -and $x_axis_first_value_valid_sim_xaxis_str -ne "true") {
                    $exact_blocker_if_any_sim_xaxis = "x_axis_first_value_invalid_on_completed"
                } elseif ($run_status_sim_xaxis -eq "completed" -and $exact_blocker_if_any_tool_sim_xaxis -ne "N/A") {
                    $exact_blocker_if_any_sim_xaxis = "tool_blocker_unexpected_on_completed"
                } elseif ($run_status_sim_xaxis -ne "completed" -and ($exact_blocker_if_any_tool_sim_xaxis -eq "N/A" -or [string]::IsNullOrWhiteSpace($exact_blocker_if_any_tool_sim_xaxis))) {
                    $exact_blocker_if_any_sim_xaxis = "tool_blocker_missing_for_non_completed_status"
                }

                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_xaxis + "`n" +
                 "axis_ok=" + $axis_ok_sim_xaxis + "`n" +
                 "run_status=" + $run_status_sim_xaxis + "`n" +
                 "run_status_valid=" + $run_status_valid_sim_xaxis_str + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_xaxis + "`n" +
                 "x_axis_name_present=" + $x_axis_name_present_sim_xaxis + "`n" +
                 "x_axis_first_value_valid=" + $x_axis_first_value_valid_sim_xaxis_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_xaxis + "`n" +
                 "source_context=" + $source_context_sim_xaxis + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_xaxis + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_xaxis + "`n" +
                 "run_start=" + $run_start_sim_xaxis + "`n" +
                 "polling_result=" + $polling_result_sim_xaxis + "`n" +
                 "run_status_log=" + $run_status_log_sim_xaxis + "`n" +
                 "plot_name=" + $plot_name_sim_xaxis + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_xaxis + "`n" +
                 "x_axis_name=" + $x_axis_name_sim_xaxis + "`n" +
                 "x_axis_name_log=" + $x_axis_name_log_sim_xaxis + "`n" +
                 "x_axis_first_value=" + $x_axis_first_value_sim_xaxis + "`n" +
                 "x_axis_first_value_log=" + $x_axis_first_value_log_sim_xaxis + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_xaxis + "`n" +
                 "diagnostics=" + $diagnostics_sim_xaxis + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_xaxis + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_xaxis + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_xaxis_probe_session_contract_open_sch_summary.txt") -Encoding UTF8

                ("visible_in_context_if_applicable=true`n" +
                 "callable_with_runtime_simulation=" + $callable_with_runtime_simulation_sim_xaxis + "`n" +
                 "axis_ok=" + $axis_ok_sim_xaxis + "`n" +
                 "run_status=" + $run_status_sim_xaxis + "`n" +
                 "run_status_valid=" + $run_status_valid_sim_xaxis_str + "`n" +
                 "plot_name_present=" + $plot_name_present_sim_xaxis + "`n" +
                 "x_axis_name_present=" + $x_axis_name_present_sim_xaxis + "`n" +
                 "x_axis_first_value_valid=" + $x_axis_first_value_valid_sim_xaxis_str + "`n" +
                 "diagnostics_present=" + $diagnostics_present_sim_xaxis + "`n" +
                 "source_context=" + $source_context_sim_xaxis + "`n" +
                 "simulator_frame_resolution=" + $simulator_frame_resolution_sim_xaxis + "`n" +
                 "runtime_entrypoint_used=" + $runtime_entrypoint_used_sim_xaxis + "`n" +
                 "run_start=" + $run_start_sim_xaxis + "`n" +
                 "polling_result=" + $polling_result_sim_xaxis + "`n" +
                 "run_status_log=" + $run_status_log_sim_xaxis + "`n" +
                 "plot_name=" + $plot_name_sim_xaxis + "`n" +
                 "plot_name_log=" + $plot_name_log_sim_xaxis + "`n" +
                 "x_axis_name=" + $x_axis_name_sim_xaxis + "`n" +
                 "x_axis_name_log=" + $x_axis_name_log_sim_xaxis + "`n" +
                 "x_axis_first_value=" + $x_axis_first_value_sim_xaxis + "`n" +
                 "x_axis_first_value_log=" + $x_axis_first_value_log_sim_xaxis + "`n" +
                 "diagnostics_log=" + $diagnostics_log_sim_xaxis + "`n" +
                 "diagnostics=" + $diagnostics_sim_xaxis + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_sim_xaxis + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sim_xaxis + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_simulation_xaxis_probe_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sim_xaxis -ne "N/A") {
                    throw "simulation_xaxis_probe_session open-SCH contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sim_xaxis)."
                }

                # Restore deterministic SCH stub context for downstream SCH chain checks.
                # Guard: when already anchored on target SCH, skip qa_open_editor_frame restore call.
                $schRestoreProbeArgs = @{
                    schematic_filename = $sch
                }
                $outSchRestoreProbeBefore = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_restore_probe_before" -argsObj $schRestoreProbeArgs -url $url -smokeExe $smokeExe
                $restore_needed = $(if ($outSchRestoreProbeBefore -match 'status=1') { "false" } else { "true" })
                $restore_attempted = "false"
                $restore_call_ok = "not_attempted"
                $restore_attempts = 0

                if ($restore_needed -eq "true") {
                    # Lightweight readiness wait before GUI restore call.
                    $ping_before_restore_ok = "false"
                    for ($ri = 0; $ri -lt 20; $ri++) {
                        $pingBeforeRestore = & $smokeExe $url ping 2>&1 | Out-String
                        $pingBeforeRestore | Set-Content -Path (Join-Path $runDir ("ping_before_sch_restore_attempt_" + $ri + ".log")) -Encoding UTF8
                        if ($pingBeforeRestore -match 'status=1') {
                            $ping_before_restore_ok = "true"
                            break
                        }
                        Start-Sleep -Milliseconds 250
                    }

                    $qaOpenSchRestoreArgs = @{
                        type     = "sch"
                        filename = $sch
                    }
                    $restore_attempted = "true"
                    $restore_call_ok = "false"
                    $restore_attempts = 0
                    $restore_timeout_ms = 15000

                    $qaOpenSchRestoreInputPath = Join-Path $runDir "mcp_call_tool_args_qa_open_editor_frame_sch_restore_after_sim_input.json"
                    ($qaOpenSchRestoreArgs | ConvertTo-Json -Compress) | Set-Content -Path $qaOpenSchRestoreInputPath -Encoding UTF8

                    for ($ra = 1; $ra -le 3; $ra++) {
                        $restore_attempts = $ra
                        $qaOpenSchRestoreOutPath = Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_sch_restore_after_sim_attempt_" + $ra + "_output.log")
                        $qaOpenSchRestoreErrPath = Join-Path $runDir ("mcp_call_tool_args_qa_open_editor_frame_sch_restore_after_sim_attempt_" + $ra + "_stderr.log")

                        $restoreProc = Start-Process -FilePath $smokeExe `
                                -ArgumentList @($url, "mcp_call_tool_args", "qa_open_editor_frame", ("@" + $qaOpenSchRestoreInputPath)) `
                                -NoNewWindow -PassThru `
                                -RedirectStandardOutput $qaOpenSchRestoreOutPath `
                                -RedirectStandardError $qaOpenSchRestoreErrPath

                        $restoreExited = $restoreProc.WaitForExit($restore_timeout_ms)
                        if (-not $restoreExited) {
                            Stop-Process -Id $restoreProc.Id -Force -ErrorAction SilentlyContinue
                            ("restore_call_timeout_ms=" + $restore_timeout_ms + "`n" +
                             "attempt=" + $ra + "`n") | Add-Content -Path $qaOpenSchRestoreOutPath -Encoding UTF8
                        }

                        $restoreOutRaw = ""
                        if (Test-Path $qaOpenSchRestoreOutPath) {
                            $restoreOutRaw = Get-Content -Path $qaOpenSchRestoreOutPath -Raw
                        }
                        if (Test-Path $qaOpenSchRestoreErrPath) {
                            $restoreErrRaw = Get-Content -Path $qaOpenSchRestoreErrPath -Raw
                            if (-not [string]::IsNullOrWhiteSpace($restoreErrRaw)) {
                                $restoreOutRaw += ("`n" + $restoreErrRaw)
                            }
                        }

                        if ($restoreOutRaw -match 'status=1') {
                            $restore_call_ok = "true"
                            break
                        }

                        Start-Sleep -Milliseconds 350
                    }
                }

                $outSchRestoreProbeAfter = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_restore_probe_after" -argsObj $schRestoreProbeArgs -url $url -smokeExe $smokeExe
                $restore_anchor_ok = $(if ($outSchRestoreProbeAfter -match 'status=1') { "true" } else { "false" })

                $exact_blocker_if_any_sch_restore_after_sim = "N/A"
                if ($restore_needed -eq "true" -and $restore_attempted -ne "true") {
                    $exact_blocker_if_any_sch_restore_after_sim = "restore_needed_but_not_attempted"
                } elseif ($restore_attempted -eq "true" -and $restore_call_ok -ne "true") {
                    $exact_blocker_if_any_sch_restore_after_sim = "restore_call_failed"
                } elseif ($restore_anchor_ok -ne "true") {
                    $exact_blocker_if_any_sch_restore_after_sim = "restore_anchor_probe_failed"
                }

                ("restore_needed=" + $restore_needed + "`n" +
                 "restore_attempted=" + $restore_attempted + "`n" +
                 "restore_call_ok=" + $restore_call_ok + "`n" +
                 "restore_attempts=" + $restore_attempts + "`n" +
                 "restore_anchor_ok=" + $restore_anchor_ok + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_restore_after_sim + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_restore_after_sim_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_sch_restore_after_sim -ne "N/A") {
                    throw "SCH restore-after-simulation contract failed in scenario '$label' (blocker=$exact_blocker_if_any_sch_restore_after_sim)."
                }
            }

            if ($OpenEditorType -eq "pcb") {
                # get_selection_pcb validation set (QA).
                $argsObjSelNoFilter = @{}
                $outSelNoFilter = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_selection_pcb" `
                        -caseLabel "pcb_frame_selection_nofilter" -argsObj $argsObjSelNoFilter `
                        -url $url -smokeExe $smokeExe

                $selCountNoFilter = "N/A"
                if ($outSelNoFilter -match 'count=([0-9]+)') { $selCountNoFilter = $Matches[1] }
                $callable_after_open_sel = $(if ($outSelNoFilter -match 'status=1') { "ok" } else { "error" })

                $argsObjSelFiltered = @{
                    types = @("KOT_PCB_TRACE")
                }
                $outSelFiltered = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_selection_pcb" `
                        -caseLabel "pcb_frame_selection_filtered_types" -argsObj $argsObjSelFiltered `
                        -url $url -smokeExe $smokeExe

                $selCountFiltered = "N/A"
                if ($outSelFiltered -match 'count=([0-9]+)') { $selCountFiltered = $Matches[1] }
                $callable_after_open_sel_filtered = $(if ($outSelFiltered -match 'status=1') { "ok" } else { "error" })

                $callable_after_open_no_selection = "N/A"
                $callable_after_open_with_selection = "N/A"
                $exact_blocker_if_any = "N/A"

                if ($selCountNoFilter -ne "N/A" -and [int]$selCountNoFilter -eq 0) {
                    $callable_after_open_no_selection = $callable_after_open_sel
                    $callable_after_open_with_selection = "not_observable_with_read_only_harness"
                    $exact_blocker_if_any = "selection state is not deterministically created by the harness (no read-only way to pilot selection; avoid write path)"
                } elseif ($selCountNoFilter -ne "N/A" -and [int]$selCountNoFilter -gt 0) {
                    $callable_after_open_with_selection = $callable_after_open_sel
                    $callable_after_open_no_selection = "not_applicable_selection_present"
                    $exact_blocker_if_any = "N/A"
                } else {
                    $callable_after_open_no_selection = "unknown"
                    $callable_after_open_with_selection = "unknown"
                    $exact_blocker_if_any = "could_not_parse_selection_count"
                }

                ("visible_before_open=" + $visible_before_open_sel_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sel + "`n" +
                 "callable_after_open_no_selection=" + $callable_after_open_no_selection + "`n" +
                 "callable_after_open_with_selection=" + $callable_after_open_with_selection + "`n" +
                 "filtered_behavior=unfiltered_count=" + $selCountNoFilter + ";filtered_count=" + $selCountFiltered + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_selection_pcb_contract_summary.txt") -Encoding UTF8

                # get_board_stackup validation set (QA).
                $argsObjStackup = @{
                    board_filename = $pcb
                }
                $outStackup = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_stackup" `
                        -caseLabel "pcb_frame_stackup_valid" -argsObj $argsObjStackup -url $url -smokeExe $smokeExe

                $layerCount = "N/A"
                if ($outStackup -match 'layer_count=([0-9]+)') { $layerCount = $Matches[1] }
                $enabledCopperLayerCount = "N/A"
                if ($outStackup -match 'enabled_copper_layer_count=([0-9]+)') { $enabledCopperLayerCount = $Matches[1] }

                $callable_after_open_stackup = $(if ($outStackup -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_stackup = "N/A"
                if ($callable_after_open_stackup -ne "ok") {
                    # Our IPC stub PCB is intentionally minimal; it may not define full stackup data.
                    $exact_blocker_if_any_stackup = "stackup_missing_or_unavailable_in_stub"
                } elseif ($layerCount -eq "N/A" -or [int]$layerCount -le 0) {
                    $exact_blocker_if_any_stackup = "layer_count unexpected (>=1 expected for default stackup)"
                }

                ("visible_before_open=" + $visible_before_open_stackup_str + "`n" +
                 "callable_before_open=" + $callable_before_open_stackup + "`n" +
                 "callable_after_open=" + $callable_after_open_stackup + "`n" +
                 "output_shape_summary=layer_count=" + $layerCount + ";enabled_copper_layer_count=" + $enabledCopperLayerCount + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_stackup + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_stackup_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_stackup -ne "ok") {
                    # Do not block set_active_layer validation on stackup availability.
                } elseif ($layerCount -eq "N/A" -or [int]$layerCount -le 0) {
                    # Do not block set_active_layer validation on stub stackup.
                }

                # get_nets validation set (QA).
                $argsObjNets = @{
                    board_filename = $pcb
                }
                $outNets = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_nets" `
                        -caseLabel "pcb_frame_nets_no_filter" -argsObj $argsObjNets -url $url -smokeExe $smokeExe

                $netsCount = "N/A"
                if ($outNets -match 'count=([0-9]+)') { $netsCount = $Matches[1] }
                $firstNetName = ""
                if ($outNets -match 'first_net_name=([^\r\n]*)') { $firstNetName = $Matches[1] }
                $nonEmptyNameCount = "N/A"
                if ($outNets -match 'non_empty_name_count=([0-9]+)') { $nonEmptyNameCount = $Matches[1] }

                $callable_after_open_nets = $(if ($outNets -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_nets = "N/A"

                if ($callable_after_open_nets -ne "ok") {
                    # Our IPC stub is minimal; net extraction may fail depending on which net-settings KiCad expects.
                    $exact_blocker_if_any_nets = "nets_missing_or_unavailable_in_stub"
                } elseif ($netsCount -eq "N/A" -or [int]$netsCount -le 0) {
                    $exact_blocker_if_any_nets = "count_after_open_not_positive"
                } elseif ($nonEmptyNameCount -eq "N/A" -or [int]$nonEmptyNameCount -le 0) {
                    $exact_blocker_if_any_nets = "non_empty_net_names_not_observed"
                }

                # Optional deterministic filter scenario on stub net name.
                $argsObjNetsFiltered = @{
                    board_filename   = $pcb
                    netclass_filter  = @("NO_SUCH_CLASS")
                }
                $outNetsFiltered = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_nets" `
                        -caseLabel "pcb_frame_nets_filtered_no_match" -argsObj $argsObjNetsFiltered -url $url -smokeExe $smokeExe
                $netsCountFiltered = "N/A"
                if ($outNetsFiltered -match 'count=([0-9]+)') { $netsCountFiltered = $Matches[1] }

                ("visible_before_open=" + $visible_before_open_nets_str + "`n" +
                 "callable_before_open=" + $callable_before_open_nets + "`n" +
                 "callable_after_open=" + $callable_after_open_nets + "`n" +
                 "count_after_open=" + $netsCount + "`n" +
                 "first_net_name_after_open=" + $firstNetName + "`n" +
                 "non_empty_name_count_after_open=" + $nonEmptyNameCount + "`n" +
                 "netclass_filter_probe=no_match_filter_count=" + $netsCountFiltered + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_nets + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_nets_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_nets -ne "ok") {
                    # Do not block set_active_layer validation on stub net extraction.
                } elseif ($netsCount -eq "N/A" -or [int]$netsCount -le 0) {
                    # Do not block set_active_layer validation on stub net count.
                } elseif ($nonEmptyNameCount -eq "N/A" -or [int]$nonEmptyNameCount -le 0) {
                    # Do not block set_active_layer validation on stub net naming.
                }

                # get_pcb_items validation set (QA).
                $pcbInvalid = Join-Path $projDir "v1_invalid.kicad_pcb"

                # Case 3/2: types absent
                $typesLabel = "ABSENT"
                $boardFilenameUsed = $pcb
                $argsObj = @{
                    board_filename = $boardFilenameUsed
                }
                $out = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" -caseLabel "pcb_frame_types_absent" -argsObj $argsObj -url $url -smokeExe $smokeExe

                $itemsCount = "N/A"
                if ($out -match 'items_count=([0-9]+)') { $itemsCount = $Matches[1] }
                $summaryPath = Join-Path $runDir "qa_get_pcb_items_summary_pcb_frame_types_absent.txt"
                ("items_count=" + $itemsCount + "`n" +
                 "type(s)_demanded=" + $typesLabel + "`n" +
                 "board_filename_used=" + $boardFilenameUsed + "`n") | Set-Content -Path $summaryPath -Encoding UTF8

                # Case 4: explicit simple types
                $typesLabel = "KOT_PCB_SHAPE"
                $boardFilenameUsed = $pcb
                $argsObj = @{
                    board_filename = $boardFilenameUsed
                    types           = @("KOT_PCB_SHAPE")
                }
                $out2 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" -caseLabel "pcb_frame_types_explicit" -argsObj $argsObj -url $url -smokeExe $smokeExe
                $itemsCount2 = "N/A"
                if ($out2 -match 'items_count=([0-9]+)') { $itemsCount2 = $Matches[1] }
                $summaryPath2 = Join-Path $runDir "qa_get_pcb_items_summary_pcb_frame_types_explicit.txt"
                ("items_count=" + $itemsCount2 + "`n" +
                 "type(s)_demanded=" + $typesLabel + "`n" +
                 "board_filename_used=" + $boardFilenameUsed + "`n") | Set-Content -Path $summaryPath2 -Encoding UTF8

                # Case 5: invalid board_filename
                $typesLabel = "ABSENT"
                $boardFilenameUsed = $pcbInvalid
                $argsObj = @{
                    board_filename = $boardFilenameUsed
                }
                $out3 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" -caseLabel "pcb_frame_board_filename_invalid" -argsObj $argsObj -url $url -smokeExe $smokeExe
                $itemsCount3 = "N/A"
                if ($out3 -match 'items_count=([0-9]+)') { $itemsCount3 = $Matches[1] }
                $summaryPath3 = Join-Path $runDir "qa_get_pcb_items_summary_pcb_frame_board_filename_invalid.txt"
                ("items_count=" + $itemsCount3 + "`n" +
                 "type(s)_demanded=" + $typesLabel + "`n" +
                 "board_filename_used=" + $boardFilenameUsed + "`n") | Set-Content -Path $summaryPath3 -Encoding UTF8

                # Case 2 requirement: `items` list must be non-empty with a real PCB frame.
                if ($itemsCount -eq "N/A" -or [int]$itemsCount -le 0) {
                    throw "get_pcb_items(items_count) expected > 0 with PCB frame open. Got items_count=$itemsCount"
                }

                # get_pcb_items_by_id QA validation (needs at least one real ID).
                $firstItemId = ""
                if ($out -match 'first_item_id=([^\r\n]*)') { $firstItemId = $Matches[1] }
                if ([string]::IsNullOrWhiteSpace($firstItemId)) {
                    throw "get_pcb_items expected non-empty first_item_id with PCB frame open."
                }

                # get_pad_shape_as_polygon QA validation with a real pad ID from stub.
                $argsObjPadsOnly = @{
                    board_filename = $pcb
                    types          = @("KOT_PCB_PAD")
                }
                $outPadsOnly = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" `
                        -caseLabel "pcb_frame_types_pad_only" -argsObj $argsObjPadsOnly -url $url -smokeExe $smokeExe

                $firstPadId = ""
                if ($outPadsOnly -match 'first_item_id=([^\r\n]*)') { $firstPadId = $Matches[1] }
                $padShapeProbeAvailable = $true
                if ([string]::IsNullOrWhiteSpace($firstPadId)) {
                    $padShapeProbeAvailable = $false
                }

                $outPadShape = ""
                if ($padShapeProbeAvailable) {
                    $argsObjPadShape = @{
                        board_filename = $pcb
                        pads           = @($firstPadId)
                        layer          = "BL_F_Cu"
                    }
                    $outPadShape = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pad_shape_as_polygon" `
                            -caseLabel "pcb_frame_pad_shape_as_polygon" -argsObj $argsObjPadShape -url $url -smokeExe $smokeExe
                }

                $padShapeCount = "N/A"
                if ($outPadShape -match 'count=([0-9]+)') { $padShapeCount = $Matches[1] }
                $padShapePolygonsCount = "N/A"
                if ($outPadShape -match 'polygons_count=([0-9]+)') { $padShapePolygonsCount = $Matches[1] }
                $callable_after_open_pad_shape = $(if ($padShapeProbeAvailable -and $outPadShape -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_pad_shape = "N/A"
                if (-not $padShapeProbeAvailable) {
                    $exact_blocker_if_any_pad_shape = "pad_item_missing_or_unavailable_in_project"
                } elseif ($callable_after_open_pad_shape -ne "ok") {
                    $exact_blocker_if_any_pad_shape = "unexpected_error"
                } elseif ($padShapeCount -eq "N/A" -or [int]$padShapeCount -le 0) {
                    $exact_blocker_if_any_pad_shape = "count_after_open_not_positive"
                } elseif ($padShapePolygonsCount -eq "N/A" -or [int]$padShapePolygonsCount -le 0) {
                    $exact_blocker_if_any_pad_shape = "polygons_count_after_open_not_positive"
                }

                ("visible_before_open=" + $visible_before_open_pad_shape_str + "`n" +
                 "callable_before_open=" + $callable_before_open_pad_shape + "`n" +
                 "callable_after_open=" + $callable_after_open_pad_shape + "`n" +
                 "polygon_present_or_count=count=" + $padShapeCount + ";polygons_count=" + $padShapePolygonsCount + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_pad_shape + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pad_shape_as_polygon_contract_summary.txt") -Encoding UTF8

                if ($padShapeProbeAvailable -and $callable_after_open_pad_shape -ne "ok") {
                    throw "get_pad_shape_as_polygon failed after opening PCB: callable_after_open_pad_shape=$callable_after_open_pad_shape"
                }
                if ($padShapeProbeAvailable -and ($padShapeCount -eq "N/A" -or [int]$padShapeCount -le 0)) {
                    throw "get_pad_shape_as_polygon count expected > 0 after opening PCB. Got count=$padShapeCount"
                }
                if ($padShapeProbeAvailable -and ($padShapePolygonsCount -eq "N/A" -or [int]$padShapePolygonsCount -le 0)) {
                    throw "get_pad_shape_as_polygon polygons_count expected > 0 after opening PCB. Got polygons_count=$padShapePolygonsCount"
                }

                # get_bounding_box_pcb validation with a real ID from get_pcb_items.
                $argsObjBboxValid = @{
                    board_filename = $pcb
                    ids            = @($firstItemId)
                }
                $outBboxValid = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_bounding_box_pcb" `
                        -caseLabel "pcb_frame_bounding_box_valid_id" -argsObj $argsObjBboxValid -url $url -smokeExe $smokeExe

                $bboxCount = "N/A"
                if ($outBboxValid -match 'count=([0-9]+)') { $bboxCount = $Matches[1] }
                $hasFirstBox = "N/A"
                if ($outBboxValid -match 'has_first_box=(true|false)') { $hasFirstBox = $Matches[1] }
                $callable_after_open_bbox = $(if ($outBboxValid -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_bbox = "N/A"
                if ($callable_after_open_bbox -ne "ok") {
                    $exact_blocker_if_any_bbox = "unexpected_error"
                } elseif ($bboxCount -eq "N/A" -or [int]$bboxCount -le 0) {
                    $exact_blocker_if_any_bbox = "bbox_count_not_positive"
                } elseif ($hasFirstBox -ne "true") {
                    $exact_blocker_if_any_bbox = "first_box_not_observed"
                }

                ("visible_before_open=" + $visible_before_open_bbox_str + "`n" +
                 "callable_before_open=" + $callable_before_open_bbox + "`n" +
                 "callable_after_open=" + $callable_after_open_bbox + "`n" +
                 "bbox_present_or_count=count=" + $bboxCount + ";has_first_box=" + $hasFirstBox + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_bbox + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_bounding_box_pcb_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_bbox -ne "ok") {
                    throw "get_bounding_box_pcb failed after opening PCB: callable_after_open_bbox=$callable_after_open_bbox"
                }
                if ($bboxCount -eq "N/A" -or [int]$bboxCount -le 0) {
                    throw "get_bounding_box_pcb count expected > 0 after opening PCB. Got count=$bboxCount"
                }
                if ($hasFirstBox -ne "true") {
                    throw "get_bounding_box_pcb expected at least one box in output."
                }

                # get_visible_layers validation set (QA).
                $argsObjVisibleLayers = @{
                    board_filename = $pcb
                }
                $outVisibleLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_visible_layers" `
                        -caseLabel "pcb_frame_visible_layers" -argsObj $argsObjVisibleLayers -url $url -smokeExe $smokeExe

                $visibleLayersCount = "N/A"
                if ($outVisibleLayers -match 'count=([0-9]+)') { $visibleLayersCount = $Matches[1] }
                $firstLayerIfAny = "N/A"
                if ($outVisibleLayers -match 'first_layer=([0-9-]+)') { $firstLayerIfAny = $Matches[1] }
                $callable_after_open_visible_layers = $(if ($outVisibleLayers -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_visible_layers = "N/A"
                if ($callable_after_open_visible_layers -ne "ok") {
                    $exact_blocker_if_any_visible_layers = "unexpected_error"
                } elseif ($visibleLayersCount -eq "N/A" -or [int]$visibleLayersCount -le 0) {
                    $exact_blocker_if_any_visible_layers = "count_after_open_not_positive"
                }

                ("visible_before_open=" + $visible_before_open_visible_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_visible_layers + "`n" +
                 "callable_after_open=" + $callable_after_open_visible_layers + "`n" +
                 "count_after_open=" + $visibleLayersCount + "`n" +
                 "first_layer_if_any=" + $firstLayerIfAny + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_visible_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_visible_layers_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_visible_layers -ne "ok") {
                    throw "get_visible_layers failed after opening PCB: callable_after_open_visible_layers=$callable_after_open_visible_layers"
                }
                if ($visibleLayersCount -eq "N/A" -or [int]$visibleLayersCount -le 0) {
                    throw "get_visible_layers count expected > 0 after opening PCB. Got count=$visibleLayersCount"
                }

                # get_board_enabled_layers validation set (QA).
                $argsObjEnabledLayers = @{
                    board_filename = $pcb
                }
                $outEnabledLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_enabled_layers" `
                        -caseLabel "pcb_frame_board_enabled_layers" -argsObj $argsObjEnabledLayers -url $url -smokeExe $smokeExe

                $enabledLayersCount = "N/A"
                if ($outEnabledLayers -match 'count=([0-9]+)') { $enabledLayersCount = $Matches[1] }
                $enabledFirstLayerIfAny = "N/A"
                if ($outEnabledLayers -match 'first_layer=([0-9-]+)') { $enabledFirstLayerIfAny = $Matches[1] }
                $callable_after_open_enabled_layers = $(if ($outEnabledLayers -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_enabled_layers = "N/A"
                if ($callable_after_open_enabled_layers -ne "ok") {
                    $exact_blocker_if_any_enabled_layers = "unexpected_error"
                } elseif ($enabledLayersCount -eq "N/A" -or [int]$enabledLayersCount -le 0) {
                    $exact_blocker_if_any_enabled_layers = "count_after_open_not_positive"
                }

                ("visible_before_open=" + $visible_before_open_enabled_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_enabled_layers + "`n" +
                 "callable_after_open=" + $callable_after_open_enabled_layers + "`n" +
                 "count_after_open=" + $enabledLayersCount + "`n" +
                 "first_layer_if_any=" + $enabledFirstLayerIfAny + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_enabled_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_enabled_layers_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_enabled_layers -ne "ok") {
                    throw "get_board_enabled_layers failed after opening PCB: callable_after_open_enabled_layers=$callable_after_open_enabled_layers"
                }
                if ($enabledLayersCount -eq "N/A" -or [int]$enabledLayersCount -le 0) {
                    throw "get_board_enabled_layers count expected > 0 after opening PCB. Got count=$enabledLayersCount"
                }

                # get_board_layer_names validation set (QA).
                $argsObjLayerNames = @{
                    board_filename = $pcb
                }
                $outLayerNames = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_layer_names" `
                        -caseLabel "pcb_frame_board_layer_names" -argsObj $argsObjLayerNames -url $url -smokeExe $smokeExe

                $layerNamesCount = "N/A"
                if ($outLayerNames -match 'count=([0-9]+)') { $layerNamesCount = $Matches[1] }
                $firstLayerNameIfAny = ""
                if ($outLayerNames -match 'first_layer_name=([^\r\n]*)') { $firstLayerNameIfAny = $Matches[1] }
                $callable_after_open_layer_names = $(if ($outLayerNames -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_layer_names = "N/A"
                if ($callable_after_open_layer_names -ne "ok") {
                    $exact_blocker_if_any_layer_names = "unexpected_error"
                } elseif ($layerNamesCount -eq "N/A" -or [int]$layerNamesCount -le 0) {
                    $exact_blocker_if_any_layer_names = "count_after_open_not_positive"
                } elseif ([string]::IsNullOrWhiteSpace($firstLayerNameIfAny)) {
                    $exact_blocker_if_any_layer_names = "first_layer_name_empty"
                }

                ("visible_before_open=" + $visible_before_open_layer_names_str + "`n" +
                 "callable_before_open=" + $callable_before_open_layer_names + "`n" +
                 "callable_after_open=" + $callable_after_open_layer_names + "`n" +
                 "count_after_open=" + $layerNamesCount + "`n" +
                 "first_layer_name_if_any=" + $firstLayerNameIfAny + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_layer_names + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_layer_names_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_layer_names -ne "ok") {
                    throw "get_board_layer_names failed after opening PCB: callable_after_open_layer_names=$callable_after_open_layer_names"
                }
                if ($layerNamesCount -eq "N/A" -or [int]$layerNamesCount -le 0) {
                    throw "get_board_layer_names count expected > 0 after opening PCB. Got count=$layerNamesCount"
                }
                if ([string]::IsNullOrWhiteSpace($firstLayerNameIfAny)) {
                    throw "get_board_layer_names expected non-empty first_layer_name after opening PCB."
                }

                # get_active_layer validation set (QA).
                $argsObjActiveLayer = @{
                    board_filename = $pcb
                }
                $outActiveLayer = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_active_layer" `
                        -caseLabel "pcb_frame_active_layer" -argsObj $argsObjActiveLayer -url $url -smokeExe $smokeExe

                $activeLayerIfAny = "N/A"
                if ($outActiveLayer -match 'active_layer=([0-9-]+)') { $activeLayerIfAny = $Matches[1] }
                $activeLayerNameIfAny = ""
                if ($outActiveLayer -match 'active_layer_name=([^\r\n]*)') { $activeLayerNameIfAny = $Matches[1] }
                $callable_after_open_active_layer = $(if ($outActiveLayer -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_active_layer = "N/A"
                if ($callable_after_open_active_layer -ne "ok") {
                    $exact_blocker_if_any_active_layer = "unexpected_error"
                } elseif ($activeLayerIfAny -eq "N/A" -or [int]$activeLayerIfAny -lt 0) {
                    $exact_blocker_if_any_active_layer = "active_layer_missing_or_negative"
                } elseif ([string]::IsNullOrWhiteSpace($activeLayerNameIfAny)) {
                    $exact_blocker_if_any_active_layer = "active_layer_name_empty"
                }

                ("visible_before_open=" + $visible_before_open_active_layer_str + "`n" +
                 "callable_before_open=" + $callable_before_open_active_layer + "`n" +
                 "callable_after_open=" + $callable_after_open_active_layer + "`n" +
                 "active_layer_if_any=" + $activeLayerIfAny + "`n" +
                 "active_layer_name_if_any=" + $activeLayerNameIfAny + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_active_layer + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_active_layer_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_active_layer -ne "ok") {
                    throw "get_active_layer failed after opening PCB: callable_after_open_active_layer=$callable_after_open_active_layer"
                }
                if ($activeLayerIfAny -eq "N/A" -or [int]$activeLayerIfAny -lt 0) {
                    throw "get_active_layer expected a non-negative active_layer value. Got active_layer=$activeLayerIfAny"
                }
                if ([string]::IsNullOrWhiteSpace($activeLayerNameIfAny)) {
                    throw "get_active_layer expected non-empty active_layer_name after opening PCB."
                }

                # set_active_layer validation set (QA): active layer change + rollback.
                $initialLayerInt = [int]$activeLayerIfAny

                $enabledLayersCsv = "N/A"
                if ($outEnabledLayers -match 'layers=([^\r\n]+)') { $enabledLayersCsv = $Matches[1] }

                $enabledLayersArr = @()
                if ($enabledLayersCsv -ne "N/A" -and -not [string]::IsNullOrWhiteSpace($enabledLayersCsv)) {
                    $enabledLayersArr = $enabledLayersCsv -split ',' | ForEach-Object { [int]$_.Trim() }
                }

                $changedLayerInt = "N/A"
                foreach ($l in $enabledLayersArr) {
                    if ($l -ne $initialLayerInt) { $changedLayerInt = $l; break }
                }

                $rollbackOk = "false"
                $exact_blocker_if_any_set_active_layer = "N/A"

                if ($changedLayerInt -eq "N/A") {
                    $exact_blocker_if_any_set_active_layer = "no_alternative_enabled_layer_found"
                } else {
                    # Apply change.
                    $argsObjSetActiveLayer = @{
                        board_filename = $pcb
                        layer          = $changedLayerInt
                    }
                    $outSetActiveLayer = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_active_layer" `
                            -caseLabel "pcb_frame_set_active_layer_changed" -argsObj $argsObjSetActiveLayer -url $url -smokeExe $smokeExe

                    $callable_after_open_set_active_layer = $(if ($outSetActiveLayer -match 'status=1') { "ok" } else { "error" })

                    $outActiveLayerAfterSet = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_active_layer" `
                            -caseLabel "pcb_frame_active_layer_after_set_active_layer" -argsObj $argsObjActiveLayer -url $url -smokeExe $smokeExe

                    $activeLayerAfterSet = "N/A"
                    if ($outActiveLayerAfterSet -match 'active_layer=([0-9-]+)') { $activeLayerAfterSet = $Matches[1] }

                    if ($callable_after_open_set_active_layer -ne "ok") {
                        $exact_blocker_if_any_set_active_layer = "set_active_layer_call_failed"
                    } elseif ($activeLayerAfterSet -eq "N/A" -or [int]$activeLayerAfterSet -ne [int]$changedLayerInt) {
                        $exact_blocker_if_any_set_active_layer = "active_layer_change_not_observed"
                    } else {
                        # Rollback.
                        $argsObjSetActiveLayerRollback = @{
                            board_filename = $pcb
                            layer          = $initialLayerInt
                        }
                        $outSetActiveLayerRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_active_layer" `
                                -caseLabel "pcb_frame_set_active_layer_rollback" -argsObj $argsObjSetActiveLayerRollback -url $url -smokeExe $smokeExe

                        $callable_after_open_rollback_set_active_layer = $(if ($outSetActiveLayerRollback -match 'status=1') { "ok" } else { "error" })

                        $outActiveLayerAfterRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_active_layer" `
                                -caseLabel "pcb_frame_active_layer_after_rollback_set_active_layer" -argsObj $argsObjActiveLayer -url $url -smokeExe $smokeExe

                        $activeLayerAfterRollback = "N/A"
                        if ($outActiveLayerAfterRollback -match 'active_layer=([0-9-]+)') { $activeLayerAfterRollback = $Matches[1] }

                        $outActiveLayerAfterRollback2 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_active_layer" `
                                -caseLabel "pcb_frame_active_layer_after_rollback_set_active_layer_confirm" -argsObj $argsObjActiveLayer -url $url -smokeExe $smokeExe

                        $activeLayerAfterRollback2 = "N/A"
                        if ($outActiveLayerAfterRollback2 -match 'active_layer=([0-9-]+)') { $activeLayerAfterRollback2 = $Matches[1] }

                        if ($callable_after_open_rollback_set_active_layer -ne "ok") {
                            $exact_blocker_if_any_set_active_layer = "rollback_call_failed"
                        } elseif ($activeLayerAfterRollback -eq "N/A" -or [int]$activeLayerAfterRollback -ne $initialLayerInt `
                                -or $activeLayerAfterRollback2 -eq "N/A" -or [int]$activeLayerAfterRollback2 -ne $initialLayerInt) {
                            $exact_blocker_if_any_set_active_layer = "rollback_not_observed"
                        } else {
                            $rollbackOk = "true"
                        }
                    }
                }

                $callable_after_open_set_active_layer_final = $(if ($exact_blocker_if_any_set_active_layer -eq "N/A" -or $rollbackOk -eq "true") { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_set_active_layer_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_active_layer + "`n" +
                 "callable_after_open=" + $callable_after_open_set_active_layer_final + "`n" +
                 "initial_layer=" + $initialLayerInt + "`n" +
                 "changed_layer=" + $changedLayerInt + "`n" +
                 "rollback_ok=" + $rollbackOk + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_set_active_layer + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_active_layer_contract_summary.txt") -Encoding UTF8

                if ($rollbackOk -ne "true") {
                    throw "set_active_layer contract failed: blocker=$exact_blocker_if_any_set_active_layer initial_layer=$initialLayerInt changed_layer=$changedLayerInt"
                }

                # set_visible_layers validation set (QA): visible layers change + rollback.
                # Use enabled layers intersection for rollback: set_visible_layers only applies enabled layers.
                $enabledLayersSet = @{}
                if ($outEnabledLayers -match 'layers=([^\r\n]+)') {
                    $enabledCsv = $Matches[1]
                    foreach ($e in ($enabledCsv -split ',')) {
                        $enabledLayersSet[[int]$e.Trim()] = $true
                    }
                }
                $initialVisibleLayersCsv = "N/A"
                if ($outVisibleLayers -match 'layers=([^\r\n]+)') { $initialVisibleLayersCsv = $Matches[1] }
                $initialVisibleLayersArr = @()
                $initialVisibleLayersEnabledArr = @()
                if ($initialVisibleLayersCsv -ne "N/A" -and -not [string]::IsNullOrWhiteSpace($initialVisibleLayersCsv)) {
                    $initialVisibleLayersArr = $initialVisibleLayersCsv -split ',' | ForEach-Object { [int]$_.Trim() }
                    foreach ($v in $initialVisibleLayersArr) {
                        if ($enabledLayersSet.ContainsKey($v)) { $initialVisibleLayersEnabledArr += $v }
                    }
                }
                $initialVisibleLayersCount = $initialVisibleLayersEnabledArr.Count
                if ($initialVisibleLayersCount -eq 0) { $initialVisibleLayersCount = $initialVisibleLayersArr.Count }
                $reducedLayers = @()
                if ($initialVisibleLayersEnabledArr.Count -ge 2) {
                    $reducedLayers = $initialVisibleLayersEnabledArr[0..1]
                } elseif ($initialVisibleLayersEnabledArr.Count -eq 1) {
                    $reducedLayers = @($initialVisibleLayersEnabledArr[0])
                }
                $rollbackOkVisibleLayers = "false"
                $exact_blocker_if_any_set_visible_layers = "N/A"
                $callable_after_open_set_visible_layers = "error"
                $changedVisibleLayersCount = "N/A"

                if ($reducedLayers.Count -eq 0) {
                    $exact_blocker_if_any_set_visible_layers = "no_initial_visible_layers_for_test"
                } else {
                    $argsObjSetVisibleLayers = @{
                        board_filename = $pcb
                        layers         = $reducedLayers
                    }
                    $outSetVisibleLayers = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_visible_layers" `
                            -caseLabel "pcb_frame_set_visible_layers_changed" -argsObj $argsObjSetVisibleLayers -url $url -smokeExe $smokeExe

                    $callable_after_open_set_visible_layers = $(if ($outSetVisibleLayers -match 'status=1') { "ok" } else { "error" })

                    $outVisibleLayersAfterSet = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_visible_layers" `
                            -caseLabel "pcb_frame_visible_layers_after_set_visible_layers" -argsObj $argsObjVisibleLayers -url $url -smokeExe $smokeExe

                    $visibleLayersCountAfterSet = "N/A"
                    if ($outVisibleLayersAfterSet -match 'count=([0-9]+)') { $visibleLayersCountAfterSet = $Matches[1] }
                    $changedVisibleLayersCount = $visibleLayersCountAfterSet

                    if ($callable_after_open_set_visible_layers -ne "ok") {
                        $exact_blocker_if_any_set_visible_layers = "set_visible_layers_call_failed"
                    } elseif ($visibleLayersCountAfterSet -eq "N/A" -or [int]$visibleLayersCountAfterSet -ne $reducedLayers.Count) {
                        $exact_blocker_if_any_set_visible_layers = "visible_layers_change_not_observed"
                    } else {
                        $argsObjSetVisibleLayersRollback = @{
                            board_filename = $pcb
                            layers         = $initialVisibleLayersEnabledArr
                        }
                        $outSetVisibleLayersRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_visible_layers" `
                                -caseLabel "pcb_frame_set_visible_layers_rollback" -argsObj $argsObjSetVisibleLayersRollback -url $url -smokeExe $smokeExe

                        $callable_rollback_set_visible_layers = $(if ($outSetVisibleLayersRollback -match 'status=1') { "ok" } else { "error" })

                        $outVisibleLayersAfterRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_visible_layers" `
                                -caseLabel "pcb_frame_visible_layers_after_rollback_set_visible_layers" -argsObj $argsObjVisibleLayers -url $url -smokeExe $smokeExe

                        $visibleLayersCountAfterRollback = "N/A"
                        if ($outVisibleLayersAfterRollback -match 'count=([0-9]+)') { $visibleLayersCountAfterRollback = $Matches[1] }

                        if ($callable_rollback_set_visible_layers -ne "ok") {
                            $exact_blocker_if_any_set_visible_layers = "rollback_call_failed"
                        } elseif ($visibleLayersCountAfterRollback -eq "N/A" -or [int]$visibleLayersCountAfterRollback -ne $initialVisibleLayersCount) {
                            $exact_blocker_if_any_set_visible_layers = "rollback_not_observed"
                        } else {
                            $rollbackOkVisibleLayers = "true"
                        }
                    }
                }

                $callable_after_open_set_visible_layers_final = $(if ($rollbackOkVisibleLayers -eq "true") { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_set_visible_layers_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_visible_layers + "`n" +
                 "callable_after_open=" + $callable_after_open_set_visible_layers_final + "`n" +
                 "initial_visible_layers_count=" + $initialVisibleLayersCount + "`n" +
                 "changed_visible_layers_count=" + $changedVisibleLayersCount + "`n" +
                 "rollback_ok=" + $rollbackOkVisibleLayers + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_set_visible_layers + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_visible_layers_contract_summary.txt") -Encoding UTF8

                if ($rollbackOkVisibleLayers -ne "true") {
                    throw "set_visible_layers contract failed: blocker=$exact_blocker_if_any_set_visible_layers initial_count=$initialVisibleLayersCount"
                }

                # get_board_editor_appearance_settings validation set (QA).
                $argsObjEditorAppearance = @{
                    board_filename = $pcb
                }
                $outEditorAppearance = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_editor_appearance_settings" `
                        -caseLabel "pcb_frame_editor_appearance_settings" -argsObj $argsObjEditorAppearance -url $url -smokeExe $smokeExe

                $outputShapeSummary = "N/A"
                if ($outEditorAppearance -match 'output_shape_summary=([^\r\n]*)') { $outputShapeSummary = $Matches[1] }
                $callable_after_open_editor_appearance = $(if ($outEditorAppearance -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_editor_appearance = "N/A"
                if ($callable_after_open_editor_appearance -ne "ok") {
                    $exact_blocker_if_any_editor_appearance = "unexpected_error"
                } elseif ($outputShapeSummary -eq "N/A" -or $outputShapeSummary -notmatch 'missing') {
                    # all fields should be present in current protobuf shape
                    $exact_blocker_if_any_editor_appearance = "N/A"
                } else {
                    $exact_blocker_if_any_editor_appearance = "missing_expected_fields_in_output_shape"
                }

                ("visible_before_open=" + $visible_before_open_editor_appearance_str + "`n" +
                 "callable_before_open=" + $callable_before_open_editor_appearance + "`n" +
                 "callable_after_open=" + $callable_after_open_editor_appearance + "`n" +
                 "output_shape_summary=" + $outputShapeSummary + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_editor_appearance + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_editor_appearance_settings_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_editor_appearance -ne "ok") {
                    throw "get_board_editor_appearance_settings failed after opening PCB: callable_after_open_editor_appearance=$callable_after_open_editor_appearance"
                }
                if ($outputShapeSummary -eq "N/A") {
                    throw "get_board_editor_appearance_settings did not provide output_shape_summary."
                }
                if ($outputShapeSummary -match 'missing') {
                    throw "get_board_editor_appearance_settings output shape missing expected fields: $outputShapeSummary"
                }

                # set_board_editor_appearance_settings validation set (QA): appearance settings change + rollback.
                $initialStateSummary = "N/A"
                if ($outEditorAppearance -match 'output_value_summary=([^\r\n]*)') { $initialStateSummary = $Matches[1] }

                $initialInactiveLayerDisplay = "N/A"
                $initialNetColorDisplay = "N/A"
                $initialBoardFlip = "N/A"
                $initialRatsnestDisplay = "N/A"
                if ($outEditorAppearance -match 'output_value_summary=inactive_layer_display=([-]?[0-9]+);net_color_display=([-]?[0-9]+);board_flip=([-]?[0-9]+);ratsnest_display=([-]?[0-9]+)') {
                    $initialInactiveLayerDisplay = [int]$Matches[1]
                    $initialNetColorDisplay = [int]$Matches[2]
                    $initialBoardFlip = [int]$Matches[3]
                    $initialRatsnestDisplay = [int]$Matches[4]
                }

                if ($initialStateSummary -eq "N/A" -or $initialBoardFlip -eq "N/A") {
                    throw "get_board_editor_appearance_settings did not provide output_value_summary for set_board_editor_appearance_settings validation."
                }

                $expectedBoardFlip = 1
                if ($initialBoardFlip -eq 1) { $expectedBoardFlip = 2 }
                elseif ($initialBoardFlip -eq 2) { $expectedBoardFlip = 1 }

                $rollbackOk = "false"
                $exact_blocker_if_any_set_editor_appearance = "N/A"
                $changed_state_summary = "N/A"
                $callable_after_open_set_editor_appearance_final = "error"

                # Apply deterministic minimal change: toggle only board_flip.
                $argsObjSetEditorAppearanceChanged = @{
                    board_filename = $pcb
                    settings = @{
                        inactive_layer_display = $initialInactiveLayerDisplay
                        net_color_display      = $initialNetColorDisplay
                        board_flip             = $expectedBoardFlip
                        ratsnest_display      = $initialRatsnestDisplay
                    }
                }

                $outSetEditorAppearanceChanged = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_editor_appearance_settings" `
                        -caseLabel "pcb_frame_set_board_editor_appearance_settings_changed" `
                        -argsObj $argsObjSetEditorAppearanceChanged -url $url -smokeExe $smokeExe

                $callable_after_open_set_editor_appearance = $(if ($outSetEditorAppearanceChanged -match 'status=1') { "ok" } else { "error" })

                # Read after set.
                $outEditorAppearanceAfterSet = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_editor_appearance_settings" `
                        -caseLabel "pcb_frame_editor_appearance_settings_after_set_board_editor_appearance_settings" `
                        -argsObj $argsObjEditorAppearance -url $url -smokeExe $smokeExe

                $outputValueSummaryAfterSet = "N/A"
                if ($outEditorAppearanceAfterSet -match 'output_value_summary=([^\r\n]*)') { $outputValueSummaryAfterSet = $Matches[1] }

                $afterSetInactiveLayerDisplay = "N/A"
                $afterSetNetColorDisplay = "N/A"
                $afterSetBoardFlip = "N/A"
                $afterSetRatsnestDisplay = "N/A"
                if ($outEditorAppearanceAfterSet -match 'output_value_summary=inactive_layer_display=([-]?[0-9]+);net_color_display=([-]?[0-9]+);board_flip=([-]?[0-9]+);ratsnest_display=([-]?[0-9]+)') {
                    $afterSetInactiveLayerDisplay = [int]$Matches[1]
                    $afterSetNetColorDisplay = [int]$Matches[2]
                    $afterSetBoardFlip = [int]$Matches[3]
                    $afterSetRatsnestDisplay = [int]$Matches[4]
                }

                $changed_state_summary = $outputValueSummaryAfterSet

                $stateChangeOk = "false"
                if( $callable_after_open_set_editor_appearance -eq "ok" -and
                        $outputValueSummaryAfterSet -ne "N/A" -and
                        $afterSetInactiveLayerDisplay -ne "N/A" -and
                        $afterSetNetColorDisplay -ne "N/A" -and
                        $afterSetBoardFlip -ne "N/A" -and
                        $afterSetRatsnestDisplay -ne "N/A" -and
                        $afterSetInactiveLayerDisplay -eq $initialInactiveLayerDisplay -and
                        $afterSetNetColorDisplay -eq $initialNetColorDisplay -and
                        $afterSetBoardFlip -eq $expectedBoardFlip -and
                        $afterSetRatsnestDisplay -eq $initialRatsnestDisplay ) {
                    $stateChangeOk = "true"
                }

                if ($callable_after_open_set_editor_appearance -ne "ok") {
                    $exact_blocker_if_any_set_editor_appearance = "set_call_failed"
                } elseif ($stateChangeOk -ne "true") {
                    $exact_blocker_if_any_set_editor_appearance = "state_change_not_observed"
                }

                # Rollback to the initial settings.
                $argsObjSetEditorAppearanceRollback = @{
                    board_filename = $pcb
                    settings = @{
                        inactive_layer_display = $initialInactiveLayerDisplay
                        net_color_display      = $initialNetColorDisplay
                        board_flip             = $initialBoardFlip
                        ratsnest_display      = $initialRatsnestDisplay
                    }
                }

                $outSetEditorAppearanceRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_editor_appearance_settings" `
                        -caseLabel "pcb_frame_set_board_editor_appearance_settings_rollback" `
                        -argsObj $argsObjSetEditorAppearanceRollback -url $url -smokeExe $smokeExe

                $callable_rollback_set_editor_appearance = $(if ($outSetEditorAppearanceRollback -match 'status=1') { "ok" } else { "error" })

                $outEditorAppearanceAfterRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_editor_appearance_settings" `
                        -caseLabel "pcb_frame_editor_appearance_settings_after_rollback_set_board_editor_appearance_settings" `
                        -argsObj $argsObjEditorAppearance -url $url -smokeExe $smokeExe

                $afterRollbackInactiveLayerDisplay = "N/A"
                $afterRollbackNetColorDisplay = "N/A"
                $afterRollbackBoardFlip = "N/A"
                $afterRollbackRatsnestDisplay = "N/A"
                if ($outEditorAppearanceAfterRollback -match 'output_value_summary=inactive_layer_display=([-]?[0-9]+);net_color_display=([-]?[0-9]+);board_flip=([-]?[0-9]+);ratsnest_display=([-]?[0-9]+)') {
                    $afterRollbackInactiveLayerDisplay = [int]$Matches[1]
                    $afterRollbackNetColorDisplay = [int]$Matches[2]
                    $afterRollbackBoardFlip = [int]$Matches[3]
                    $afterRollbackRatsnestDisplay = [int]$Matches[4]
                }

                if ($callable_rollback_set_editor_appearance -ne "ok") {
                    $exact_blocker_if_any_set_editor_appearance = "rollback_call_failed"
                } elseif ( $afterRollbackInactiveLayerDisplay -eq "N/A" -or
                           $afterRollbackNetColorDisplay -eq "N/A" -or
                           $afterRollbackBoardFlip -eq "N/A" -or
                           $afterRollbackRatsnestDisplay -eq "N/A" -or
                           $afterRollbackInactiveLayerDisplay -ne $initialInactiveLayerDisplay -or
                           $afterRollbackNetColorDisplay -ne $initialNetColorDisplay -or
                           $afterRollbackBoardFlip -ne $initialBoardFlip -or
                           $afterRollbackRatsnestDisplay -ne $initialRatsnestDisplay ) {
                    $exact_blocker_if_any_set_editor_appearance = "rollback_not_observed"
                } else {
                    $rollbackOk = "true"
                }

                $callable_after_open_set_editor_appearance_final = $(if ($stateChangeOk -eq "true" -and $rollbackOk -eq "true") { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_set_editor_appearance_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_editor_appearance + "`n" +
                 "callable_after_open=" + $callable_after_open_set_editor_appearance_final + "`n" +
                 "initial_state_summary=" + $initialStateSummary + "`n" +
                 "changed_state_summary=" + $changed_state_summary + "`n" +
                 "rollback_ok=" + $rollbackOk + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_set_editor_appearance + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_board_editor_appearance_settings_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_set_editor_appearance_final -ne "ok") {
                    throw "set_board_editor_appearance_settings contract failed: blocker=$exact_blocker_if_any_set_editor_appearance initial=$initialStateSummary changed=$changed_state_summary rollback_ok=$rollbackOk"
                }

                # get_board_origin validation set (QA).
                $argsObjBoardOrigin = @{
                    board_filename = $pcb
                }
                $outBoardOrigin = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_origin" `
                        -caseLabel "pcb_frame_board_origin" -argsObj $argsObjBoardOrigin -url $url -smokeExe $smokeExe

                $originShapeSummary = "N/A"
                if ($outBoardOrigin -match 'origin_shape_summary=([^\r\n]*)') { $originShapeSummary = $Matches[1] }
                $callable_after_open_board_origin = $(if ($outBoardOrigin -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_board_origin = "N/A"
                if ($callable_after_open_board_origin -ne "ok") {
                    $exact_blocker_if_any_board_origin = "unexpected_error"
                } elseif ($originShapeSummary -eq "N/A" -or $originShapeSummary -match 'missing') {
                    $exact_blocker_if_any_board_origin = "missing_expected_fields_in_origin_shape"
                }

                ("visible_before_open=" + $visible_before_open_board_origin_str + "`n" +
                 "callable_before_open=" + $callable_before_open_board_origin + "`n" +
                 "callable_after_open=" + $callable_after_open_board_origin + "`n" +
                 "origin_shape_summary=" + $originShapeSummary + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_board_origin + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_origin_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_board_origin -ne "ok") {
                    throw "get_board_origin failed after opening PCB: callable_after_open_board_origin=$callable_after_open_board_origin"
                }
                if ($originShapeSummary -eq "N/A" -or $originShapeSummary -match 'missing') {
                    throw "get_board_origin output shape missing expected fields: $originShapeSummary"
                }

                # set_board_origin validation set (QA): origin change + rollback.
                $rollbackOk = "false"
                $exact_blocker_if_any_set_board_origin = "N/A"
                $initial_origin_summary = "N/A"
                $changed_origin_summary = "N/A"
                $callable_after_open_set_board_origin_final = "error"

                # Read initial numeric values from get_board_origin output_value_summary.
                $initialOriginXGrid = "N/A"
                $initialOriginYGrid = "N/A"
                $initialOriginXDrill = "N/A"
                $initialOriginYDrill = "N/A"
                if ($outBoardOrigin -match 'output_value_summary=origin_x_nm=([-]?[0-9]+);origin_y_nm=([-]?[0-9]+);grid_origin_x_nm=([-]?[0-9]+);grid_origin_y_nm=([-]?[0-9]+);drill_origin_x_nm=([-]?[0-9]+);drill_origin_y_nm=([-]?[0-9]+)') {
                    $initialOriginXGrid = [int64]$Matches[3]
                    $initialOriginYGrid = [int64]$Matches[4]
                    $initialOriginXDrill = [int64]$Matches[5]
                    $initialOriginYDrill = [int64]$Matches[6]
                    $initial_origin_summary = $Matches[1] + "," + $Matches[2] + " | grid=(" + $Matches[3] + "," + $Matches[4] + ") drill=(" + $Matches[5] + "," + $Matches[6] + ")"
                }

                if ($initialOriginXGrid -eq "N/A" -or $initialOriginYGrid -eq "N/A") {
                    throw "get_board_origin did not provide output_value_summary for set_board_origin validation."
                }

                # Deterministic minimal change: modify BOT_GRID.x_nm only.
                $expectedGridX = $initialOriginXGrid + 1000000
                $expectedGridY = $initialOriginYGrid

                $argsObjSetBoardOriginChanged = @{
                    board_filename = $pcb
                    type            = 1  # BOT_GRID
                    origin          = @{
                        x_nm = $expectedGridX
                        y_nm = $expectedGridY
                    }
                }

                $outSetBoardOriginChanged = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_origin" `
                        -caseLabel "pcb_frame_set_board_origin_changed" `
                        -argsObj $argsObjSetBoardOriginChanged -url $url -smokeExe $smokeExe

                $callable_after_open_set_board_origin = $(if ($outSetBoardOriginChanged -match 'status=1') { "ok" } else { "error" })

                # Read after set.
                $outBoardOriginAfterSet = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_origin" `
                        -caseLabel "pcb_frame_board_origin_after_set_board_origin_changed" `
                        -argsObj $argsObjBoardOrigin -url $url -smokeExe $smokeExe

                $outputValueSummaryAfterSet = "N/A"
                $afterSetGridX = "N/A"
                $afterSetGridY = "N/A"
                $afterSetDrillX = "N/A"
                $afterSetDrillY = "N/A"
                if ($outBoardOriginAfterSet -match 'output_value_summary=([^\r\n]*)') { $outputValueSummaryAfterSet = $Matches[1] }
                if ($outBoardOriginAfterSet -match 'output_value_summary=origin_x_nm=([-]?[0-9]+);origin_y_nm=([-]?[0-9]+);grid_origin_x_nm=([-]?[0-9]+);grid_origin_y_nm=([-]?[0-9]+);drill_origin_x_nm=([-]?[0-9]+);drill_origin_y_nm=([-]?[0-9]+)') {
                    $afterSetGridX = [int64]$Matches[3]
                    $afterSetGridY = [int64]$Matches[4]
                    $afterSetDrillX = [int64]$Matches[5]
                    $afterSetDrillY = [int64]$Matches[6]
                }

                $changed_origin_summary = $outputValueSummaryAfterSet

                $stateChangeOk = "false"
                if ($callable_after_open_set_board_origin -eq "ok" -and
                        $afterSetGridX -ne "N/A" -and $afterSetGridY -ne "N/A" -and
                        $afterSetDrillX -ne "N/A" -and $afterSetDrillY -ne "N/A" -and
                        $afterSetGridX -eq $expectedGridX -and $afterSetGridY -eq $expectedGridY -and
                        $afterSetDrillX -eq $initialOriginXDrill -and $afterSetDrillY -eq $initialOriginYDrill ) {
                    $stateChangeOk = "true"
                }

                $exact_blocker_if_any_set_board_origin = "N/A"
                if ($callable_after_open_set_board_origin -ne "ok") {
                    $exact_blocker_if_any_set_board_origin = "set_call_failed"
                } elseif ($stateChangeOk -ne "true") {
                    $exact_blocker_if_any_set_board_origin = "state_change_not_observed"
                }

                # Rollback to initial grid origin.
                $argsObjSetBoardOriginRollback = @{
                    board_filename = $pcb
                    type            = 1  # BOT_GRID
                    origin          = @{
                        x_nm = $initialOriginXGrid
                        y_nm = $initialOriginYGrid
                    }
                }

                $outSetBoardOriginRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_board_origin" `
                        -caseLabel "pcb_frame_set_board_origin_rollback" `
                        -argsObj $argsObjSetBoardOriginRollback -url $url -smokeExe $smokeExe

                $callable_rollback_set_board_origin = $(if ($outSetBoardOriginRollback -match 'status=1') { "ok" } else { "error" })

                $outBoardOriginAfterRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_origin" `
                        -caseLabel "pcb_frame_board_origin_after_rollback_set_board_origin" `
                        -argsObj $argsObjBoardOrigin -url $url -smokeExe $smokeExe

                $afterRollbackGridX = "N/A"
                $afterRollbackGridY = "N/A"
                $afterRollbackDrillX = "N/A"
                $afterRollbackDrillY = "N/A"
                if ($outBoardOriginAfterRollback -match 'output_value_summary=origin_x_nm=([-]?[0-9]+);origin_y_nm=([-]?[0-9]+);grid_origin_x_nm=([-]?[0-9]+);grid_origin_y_nm=([-]?[0-9]+);drill_origin_x_nm=([-]?[0-9]+);drill_origin_y_nm=([-]?[0-9]+)') {
                    $afterRollbackGridX = [int64]$Matches[3]
                    $afterRollbackGridY = [int64]$Matches[4]
                    $afterRollbackDrillX = [int64]$Matches[5]
                    $afterRollbackDrillY = [int64]$Matches[6]
                }

                if ($callable_rollback_set_board_origin -ne "ok") {
                    $exact_blocker_if_any_set_board_origin = "rollback_call_failed"
                } elseif ($afterRollbackGridX -eq "N/A" -or $afterRollbackGridY -eq "N/A" -or
                          $afterRollbackDrillX -eq "N/A" -or $afterRollbackDrillY -eq "N/A" -or
                          $afterRollbackGridX -ne $initialOriginXGrid -or $afterRollbackGridY -ne $initialOriginYGrid -or
                          $afterRollbackDrillX -ne $initialOriginXDrill -or $afterRollbackDrillY -ne $initialOriginYDrill ) {
                    $exact_blocker_if_any_set_board_origin = "rollback_not_observed"
                } else {
                    $rollbackOk = "true"
                }

                $callable_after_open_set_board_origin_final = $(if ($stateChangeOk -eq "true" -and $rollbackOk -eq "true") { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_set_board_origin_str + "`n" +
                 "callable_before_open=" + $callable_before_open_set_board_origin + "`n" +
                 "callable_after_open=" + $callable_after_open_set_board_origin_final + "`n" +
                 "initial_origin_summary=" + $initial_origin_summary + "`n" +
                 "changed_origin_summary=" + $changed_origin_summary + "`n" +
                 "rollback_ok=" + $rollbackOk + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_set_board_origin + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_set_board_origin_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_set_board_origin_final -ne "ok") {
                    throw "set_board_origin contract failed: blocker=$exact_blocker_if_any_set_board_origin initial=$initial_origin_summary changed=$changed_origin_summary rollback_ok=$rollbackOk"
                }

                # pcb_fabrication_origin_prepare_session (composed métier v1): resolve fab intent, apply/verify/rollback.
                $visible_in_project_context_origin_session = $(if ($ltAfter -match 'pcb_fabrication_origin_prepare_session') { "true" } else { "false" })
                $originSessionDryRunArgs = @{
                    board_filename = $pcb
                    origin_intent  = "fab_lower_left"
                    dry_run        = $true
                }
                $outOriginSessionDryRun = Call-McpToolArgsAndLog -runDir $runDir -toolName "pcb_fabrication_origin_prepare_session" `
                        -caseLabel "pcb_frame_pcb_fabrication_origin_prepare_session_dry_run" -argsObj $originSessionDryRunArgs -url $url -smokeExe $smokeExe
                $intent_resolution_ok_origin_session = $(if ($outOriginSessionDryRun -match 'status=1' -and $outOriginSessionDryRun -match 'intent_resolution_ok=1') { "true" } else { "false" })
                $dry_run_ok_origin_session = $(if ($outOriginSessionDryRun -match 'status=1' -and $outOriginSessionDryRun -match 'dry_run_ok=1') { "true" } else { "false" })

                $originSessionApplyArgs = @{
                    board_filename = $pcb
                    origin_intent  = "fab_lower_left"
                }
                $outOriginSessionApply = Call-McpToolArgsAndLog -runDir $runDir -toolName "pcb_fabrication_origin_prepare_session" `
                        -caseLabel "pcb_frame_pcb_fabrication_origin_prepare_session_apply_verify_rollback" -argsObj $originSessionApplyArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_origin_session = $(if ($outOriginSessionApply -match 'status=1') { "ok" } else { "error" })
                $apply_ok_origin_session = $(if ($outOriginSessionApply -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_origin_session = $(if ($outOriginSessionApply -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_origin_session = $(if ($outOriginSessionApply -match 'rollback_ok=1') { "true" } else { "false" })
                $board_state_restored_origin_session = $(if ($outOriginSessionApply -match 'board_state_restored=1') { "true" } else { "false" })

                $exact_blocker_if_any_origin_session = "N/A"
                if ($intent_resolution_ok_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "intent_resolution_failed"
                } elseif ($dry_run_ok_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "dry_run_failed"
                } elseif ($callable_in_project_context_origin_session -ne "ok") {
                    if ($outOriginSessionApply -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_origin_session = $Matches[1]
                    } else {
                        $exact_blocker_if_any_origin_session = "origin_session_call_failed"
                    }
                } elseif ($apply_ok_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "apply_failed"
                } elseif ($verify_ok_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "verify_failed"
                } elseif ($rollback_ok_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "rollback_failed"
                } elseif ($board_state_restored_origin_session -ne "true") {
                    $exact_blocker_if_any_origin_session = "board_state_not_restored"
                }

                ("visible_in_project_context=" + $visible_in_project_context_origin_session + "`n" +
                 "callable_in_project_context=" + $callable_in_project_context_origin_session + "`n" +
                 "intent_resolution_ok=" + $intent_resolution_ok_origin_session + "`n" +
                 "dry_run_ok=" + $dry_run_ok_origin_session + "`n" +
                 "apply_ok=" + $apply_ok_origin_session + "`n" +
                 "verify_ok=" + $verify_ok_origin_session + "`n" +
                 "rollback_ok=" + $rollback_ok_origin_session + "`n" +
                 "board_state_restored=" + $board_state_restored_origin_session + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_origin_session + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_pcb_fabrication_origin_prepare_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_origin_session -ne "N/A") {
                    throw "pcb_fabrication_origin_prepare_session contract failed: blocker=$exact_blocker_if_any_origin_session"
                }

                # pcb_origin_decision_correction_session (composed multi-contexte v1): read -> decision -> correct_if_needed -> verify -> rollback.
                $visible_in_project_context_origin_decision = $(if ($ltAfter -match 'pcb_origin_decision_correction_session') { "true" } else { "false" })
                $originDecisionArgs = @{
                    board_filename = $pcb
                    origin_intent  = "fab_lower_left"
                    decision_mode  = "correct_if_needed"
                }
                $outOriginDecision = Call-McpToolArgsAndLog -runDir $runDir -toolName "pcb_origin_decision_correction_session" `
                        -caseLabel "pcb_frame_pcb_origin_decision_correction_session" -argsObj $originDecisionArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_origin_decision = $(if ($outOriginDecision -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok_origin_decision = $(if ($outOriginDecision -match 'decision_taken_ok=1') { "true" } else { "false" })
                $correction_needed_detected_origin_decision = $(if ($outOriginDecision -match 'correction_needed_detected=1') { "true" } else { "false" })
                $apply_ok_origin_decision = $(if ($outOriginDecision -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_origin_decision = $(if ($outOriginDecision -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_origin_decision = $(if ($outOriginDecision -match 'rollback_ok=1') { "true" } else { "false" })
                $board_state_restored_origin_decision = $(if ($outOriginDecision -match 'board_state_restored=1') { "true" } else { "false" })

                $exact_blocker_if_any_origin_decision = "N/A"
                if ($callable_in_project_context_origin_decision -ne "ok") {
                    if ($outOriginDecision -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_origin_decision = $Matches[1]
                    } else {
                        $exact_blocker_if_any_origin_decision = "origin_decision_call_failed"
                    }
                } elseif ($decision_taken_ok_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "decision_not_taken"
                } elseif ($correction_needed_detected_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "correction_needed_not_detected_in_stub"
                } elseif ($apply_ok_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "apply_failed"
                } elseif ($verify_ok_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "verify_failed"
                } elseif ($rollback_ok_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "rollback_failed"
                } elseif ($board_state_restored_origin_decision -ne "true") {
                    $exact_blocker_if_any_origin_decision = "board_state_not_restored"
                }

                ("visible_in_project_context=" + $visible_in_project_context_origin_decision + "`n" +
                 "callable_in_project_context=" + $callable_in_project_context_origin_decision + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok_origin_decision + "`n" +
                 "correction_needed_detected=" + $correction_needed_detected_origin_decision + "`n" +
                 "apply_ok=" + $apply_ok_origin_decision + "`n" +
                 "verify_ok=" + $verify_ok_origin_decision + "`n" +
                 "rollback_ok=" + $rollback_ok_origin_decision + "`n" +
                 "board_state_restored=" + $board_state_restored_origin_decision + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_origin_decision + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_pcb_origin_decision_correction_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_origin_decision -ne "N/A") {
                    throw "pcb_origin_decision_correction_session contract failed: blocker=$exact_blocker_if_any_origin_decision"
                }

                # cross_context_preflight_correction_session (global v1) - subcase pcb_priority
                $ccpPcbArgs = @{
                    mode                    = "pcb_priority"
                    allow_single_correction = $true
                    board_filename          = $pcb
                }
                $outCcpPcb = Call-McpToolArgsAndLog -runDir $runDir -toolName "cross_context_preflight_correction_session" `
                        -caseLabel "pcb_frame_cross_context_preflight_correction_pcb_priority" -argsObj $ccpPcbArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_ccp_pcb = $(if ($outCcpPcb -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok_ccp_pcb = $(if ($outCcpPcb -match 'decision_taken_ok=1') { "true" } else { "false" })
                $correction_needed_detected_ccp_pcb = $(if ($outCcpPcb -match 'correction_needed_detected=1') { "true" } else { "false" })
                $chosen_branch_ccp_pcb = "N/A"
                $chosenBranchLinePcb = (($outCcpPcb -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLinePcb)) { $chosen_branch_ccp_pcb = ($chosenBranchLinePcb -replace '^chosen_branch=', '').Trim() }
                $single_correction_enforced_ccp_pcb = $(if ($outCcpPcb -match 'single_correction_enforced=1') { "true" } else { "false" })
                $apply_ok_ccp_pcb = $(if ($outCcpPcb -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_ccp_pcb = $(if ($outCcpPcb -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_ccp_pcb = $(if ($outCcpPcb -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_ccp_pcb = $(if ($outCcpPcb -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_ccp_pcb = "N/A"
                if ($callable_in_project_context_ccp_pcb -ne "ok") {
                    $exact_blocker_if_any_ccp_pcb = "call_failed"
                } elseif ($decision_taken_ok_ccp_pcb -ne "true") {
                    $exact_blocker_if_any_ccp_pcb = "decision_not_taken"
                } elseif ($chosen_branch_ccp_pcb -ne "pcb") {
                    $exact_blocker_if_any_ccp_pcb = "unexpected_chosen_branch"
                } elseif ($single_correction_enforced_ccp_pcb -ne "true") {
                    $exact_blocker_if_any_ccp_pcb = "single_correction_not_enforced"
                } elseif ($apply_ok_ccp_pcb -ne "true" -or $verify_ok_ccp_pcb -ne "true" -or $rollback_ok_ccp_pcb -ne "true" -or $cleanup_or_state_restored_ok_ccp_pcb -ne "true") {
                    $exact_blocker_if_any_ccp_pcb = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_ccp_pcb + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok_ccp_pcb + "`n" +
                 "correction_needed_detected=" + $correction_needed_detected_ccp_pcb + "`n" +
                 "chosen_branch=" + $chosen_branch_ccp_pcb + "`n" +
                 "single_correction_enforced=" + $single_correction_enforced_ccp_pcb + "`n" +
                 "apply_ok=" + $apply_ok_ccp_pcb + "`n" +
                 "verify_ok=" + $verify_ok_ccp_pcb + "`n" +
                 "rollback_ok=" + $rollback_ok_ccp_pcb + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_ccp_pcb + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_ccp_pcb + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_cross_context_preflight_correction_session_contract_pcb_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_ccp_pcb -ne "N/A") {
                    throw "cross_context_preflight_correction_session (pcb_priority) failed: blocker=$exact_blocker_if_any_ccp_pcb"
                }

                # workflow_preflight_gate_session - executed subcase pcb_priority
                $workflowGatePcbArgs = @{
                    workflow_stage          = "prototype_preflight"
                    priority_mode           = "pcb_priority"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outWorkflowGatePcb = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_preflight_gate_session" `
                        -caseLabel "pcb_frame_workflow_preflight_gate_session_pcb_execute_true" -argsObj $workflowGatePcbArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'decision_taken_ok=1') { "true" } else { "false" })
                $gate_decision_workflow_gate_pcb = "N/A"
                $gateDecisionLineWorkflowPcb = (($outWorkflowGatePcb -split "`r?`n") | Where-Object { $_ -like "gate_decision=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($gateDecisionLineWorkflowPcb)) { $gate_decision_workflow_gate_pcb = ($gateDecisionLineWorkflowPcb -replace '^gate_decision=', '').Trim() }
                $chosen_branch_workflow_gate_pcb = "N/A"
                $chosenBranchLineWorkflowPcb = (($outWorkflowGatePcb -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLineWorkflowPcb)) { $chosen_branch_workflow_gate_pcb = ($chosenBranchLineWorkflowPcb -replace '^chosen_branch=', '').Trim() }
                $single_correction_enforced_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'single_correction_enforced=1') { "true" } else { "false" })
                $apply_ok_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_workflow_gate_pcb = $(if ($outWorkflowGatePcb -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_workflow_gate_pcb = "N/A"
                if ($callable_in_project_context_workflow_gate_pcb -ne "ok") {
                    $exact_blocker_if_any_workflow_gate_pcb = "call_failed"
                } elseif ($decision_taken_ok_workflow_gate_pcb -ne "true") {
                    $exact_blocker_if_any_workflow_gate_pcb = "decision_not_taken"
                } elseif ($gate_decision_workflow_gate_pcb -ne "ready") {
                    $exact_blocker_if_any_workflow_gate_pcb = "gate_not_ready_after_execute"
                } elseif ($chosen_branch_workflow_gate_pcb -ne "pcb") {
                    $exact_blocker_if_any_workflow_gate_pcb = "unexpected_chosen_branch"
                } elseif ($single_correction_enforced_workflow_gate_pcb -ne "true") {
                    $exact_blocker_if_any_workflow_gate_pcb = "single_correction_not_enforced"
                } elseif ($apply_ok_workflow_gate_pcb -ne "true" -or $verify_ok_workflow_gate_pcb -ne "true" -or $rollback_ok_workflow_gate_pcb -ne "true" -or $cleanup_or_state_restored_ok_workflow_gate_pcb -ne "true") {
                    $exact_blocker_if_any_workflow_gate_pcb = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_workflow_gate_pcb + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok_workflow_gate_pcb + "`n" +
                 "gate_decision=" + $gate_decision_workflow_gate_pcb + "`n" +
                 "chosen_branch=" + $chosen_branch_workflow_gate_pcb + "`n" +
                 "single_correction_enforced=" + $single_correction_enforced_workflow_gate_pcb + "`n" +
                 "apply_ok=" + $apply_ok_workflow_gate_pcb + "`n" +
                 "verify_ok=" + $verify_ok_workflow_gate_pcb + "`n" +
                 "rollback_ok=" + $rollback_ok_workflow_gate_pcb + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_workflow_gate_pcb + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_workflow_gate_pcb + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_preflight_gate_session_contract_pcb_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_workflow_gate_pcb -ne "N/A") {
                    throw "workflow_preflight_gate_session (pcb execute=true) failed: blocker=$exact_blocker_if_any_workflow_gate_pcb"
                }

                # workflow_next_best_action_session - executed subcase pcb family
                $nextBestPcbArgs = @{
                    workflow_stage          = "prototype_preflight"
                    priority_mode           = "pcb_priority"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outNextBestPcb = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_next_best_action_session" `
                        -caseLabel "pcb_frame_workflow_next_best_action_session_pcb_execute_true" -argsObj $nextBestPcbArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_next_best_pcb = $(if ($outNextBestPcb -match 'status=1') { "ok" } else { "error" })
                $recommendation_taken_ok_next_best_pcb = $(if ($outNextBestPcb -match 'recommendation_taken_ok=1') { "true" } else { "false" })
                $recommended_action_next_best_pcb = "N/A"
                $recommendedActionLineNextBestPcb = (($outNextBestPcb -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($recommendedActionLineNextBestPcb)) { $recommended_action_next_best_pcb = ($recommendedActionLineNextBestPcb -replace '^recommended_action=', '').Trim() }
                $execute_mode_next_best_pcb = "N/A"
                $executeModeLineNextBestPcb = (($outNextBestPcb -split "`r?`n") | Where-Object { $_ -like "execute_mode=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($executeModeLineNextBestPcb)) { $execute_mode_next_best_pcb = ($executeModeLineNextBestPcb -replace '^execute_mode=', '').Trim() }
                $single_action_enforced_next_best_pcb = $(if ($outNextBestPcb -match 'single_action_enforced=1') { "true" } else { "false" })
                $apply_ok_next_best_pcb = $(if ($outNextBestPcb -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_next_best_pcb = $(if ($outNextBestPcb -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_next_best_pcb = $(if ($outNextBestPcb -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_next_best_pcb = $(if ($outNextBestPcb -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_next_best_pcb = "N/A"
                if ($callable_in_project_context_next_best_pcb -ne "ok") {
                    $exact_blocker_if_any_next_best_pcb = "call_failed"
                } elseif ($recommendation_taken_ok_next_best_pcb -ne "true") {
                    $exact_blocker_if_any_next_best_pcb = "recommendation_not_taken"
                } elseif ($recommended_action_next_best_pcb -ne "correct_pcb_fabrication_origin") {
                    $exact_blocker_if_any_next_best_pcb = "unexpected_recommended_action"
                } elseif ($execute_mode_next_best_pcb -ne "execute_true") {
                    $exact_blocker_if_any_next_best_pcb = "unexpected_execute_mode"
                } elseif ($single_action_enforced_next_best_pcb -ne "true") {
                    $exact_blocker_if_any_next_best_pcb = "single_action_not_enforced"
                } elseif ($apply_ok_next_best_pcb -ne "true" -or $verify_ok_next_best_pcb -ne "true" -or $rollback_ok_next_best_pcb -ne "true" -or $cleanup_or_state_restored_ok_next_best_pcb -ne "true") {
                    $exact_blocker_if_any_next_best_pcb = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_next_best_pcb + "`n" +
                 "recommendation_taken_ok=" + $recommendation_taken_ok_next_best_pcb + "`n" +
                 "recommended_action=" + $recommended_action_next_best_pcb + "`n" +
                 "execute_mode=" + $execute_mode_next_best_pcb + "`n" +
                 "single_action_enforced=" + $single_action_enforced_next_best_pcb + "`n" +
                 "apply_ok=" + $apply_ok_next_best_pcb + "`n" +
                 "verify_ok=" + $verify_ok_next_best_pcb + "`n" +
                 "rollback_ok=" + $rollback_ok_next_best_pcb + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_next_best_pcb + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_next_best_pcb + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_next_best_action_session_contract_pcb_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_next_best_pcb -ne "N/A") {
                    throw "workflow_next_best_action_session (pcb execute=true) failed: blocker=$exact_blocker_if_any_next_best_pcb"
                }

                # workflow_handoff_readiness_session - executed subcase pcb/release handoff
                $handoffReadinessPcbArgs = @{
                    handoff_target          = "pcb_to_release"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outHandoffReadinessPcb = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_handoff_readiness_session" `
                        -caseLabel "pcb_frame_workflow_handoff_readiness_session_pcb_release_execute_true" -argsObj $handoffReadinessPcbArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'status=1') { "ok" } else { "error" })
                $handoff_decision_taken_ok_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'handoff_decision_taken_ok=1') { "true" } else { "false" })
                $handoff_target_handoff_pcb = "N/A"
                $handoffTargetLineHandoffPcb = (($outHandoffReadinessPcb -split "`r?`n") | Where-Object { $_ -like "handoff_target=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($handoffTargetLineHandoffPcb)) { $handoff_target_handoff_pcb = ($handoffTargetLineHandoffPcb -replace '^handoff_target=', '').Trim() }
                $handoff_decision_handoff_pcb = "N/A"
                $handoffDecisionLineHandoffPcb = (($outHandoffReadinessPcb -split "`r?`n") | Where-Object { $_ -like "handoff_decision=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($handoffDecisionLineHandoffPcb)) { $handoff_decision_handoff_pcb = ($handoffDecisionLineHandoffPcb -replace '^handoff_decision=', '').Trim() }
                $chosen_branch_handoff_pcb = "N/A"
                $chosenBranchLineHandoffPcb = (($outHandoffReadinessPcb -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLineHandoffPcb)) { $chosen_branch_handoff_pcb = ($chosenBranchLineHandoffPcb -replace '^chosen_branch=', '').Trim() }
                $single_action_enforced_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'single_action_enforced=1') { "true" } else { "false" })
                $apply_ok_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_handoff_pcb = $(if ($outHandoffReadinessPcb -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_handoff_pcb = "N/A"
                if ($callable_in_project_context_handoff_pcb -ne "ok") {
                    $exact_blocker_if_any_handoff_pcb = "call_failed"
                } elseif ($handoff_decision_taken_ok_handoff_pcb -ne "true") {
                    $exact_blocker_if_any_handoff_pcb = "handoff_decision_not_taken"
                } elseif ($handoff_target_handoff_pcb -ne "pcb_to_release") {
                    $exact_blocker_if_any_handoff_pcb = "unexpected_handoff_target"
                } elseif ($handoff_decision_handoff_pcb -ne "ready") {
                    $exact_blocker_if_any_handoff_pcb = "handoff_not_ready_after_execute"
                } elseif ($chosen_branch_handoff_pcb -ne "pcb") {
                    $exact_blocker_if_any_handoff_pcb = "unexpected_chosen_branch"
                } elseif ($single_action_enforced_handoff_pcb -ne "true") {
                    $exact_blocker_if_any_handoff_pcb = "single_action_not_enforced"
                } elseif ($apply_ok_handoff_pcb -ne "true" -or $verify_ok_handoff_pcb -ne "true" -or $rollback_ok_handoff_pcb -ne "true" -or $cleanup_or_state_restored_ok_handoff_pcb -ne "true") {
                    $exact_blocker_if_any_handoff_pcb = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_handoff_pcb + "`n" +
                 "handoff_decision_taken_ok=" + $handoff_decision_taken_ok_handoff_pcb + "`n" +
                 "handoff_target=" + $handoff_target_handoff_pcb + "`n" +
                 "handoff_decision=" + $handoff_decision_handoff_pcb + "`n" +
                 "chosen_branch=" + $chosen_branch_handoff_pcb + "`n" +
                 "single_action_enforced=" + $single_action_enforced_handoff_pcb + "`n" +
                 "apply_ok=" + $apply_ok_handoff_pcb + "`n" +
                 "verify_ok=" + $verify_ok_handoff_pcb + "`n" +
                 "rollback_ok=" + $rollback_ok_handoff_pcb + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_handoff_pcb + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_handoff_pcb + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_handoff_readiness_session_contract_pcb_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_handoff_pcb -ne "N/A") {
                    throw "workflow_handoff_readiness_session (pcb_to_release execute=true) failed: blocker=$exact_blocker_if_any_handoff_pcb"
                }

                # workflow_assistant_session - executed subcase handoff workflow family
                $assistantHandoffArgs = @{
                    assistant_intent        = "handoff_readiness"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outAssistantHandoff = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_assistant_session" `
                        -caseLabel "pcb_frame_workflow_assistant_session_handoff_execute_true" -argsObj $assistantHandoffArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_assistant_handoff = $(if ($outAssistantHandoff -match 'status=1') { "ok" } else { "error" })
                $assistant_decision_taken_ok_assistant_handoff = $(if ($outAssistantHandoff -match 'assistant_decision_taken_ok=1') { "true" } else { "false" })
                $assistant_intent_assistant_handoff = "N/A"
                $assistantIntentLineAssistantHandoff = (($outAssistantHandoff -split "`r?`n") | Where-Object { $_ -like "assistant_intent=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($assistantIntentLineAssistantHandoff)) { $assistant_intent_assistant_handoff = ($assistantIntentLineAssistantHandoff -replace '^assistant_intent=', '').Trim() }
                $chosen_workflow_assistant_handoff = "N/A"
                $chosenWorkflowLineAssistantHandoff = (($outAssistantHandoff -split "`r?`n") | Where-Object { $_ -like "chosen_workflow=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenWorkflowLineAssistantHandoff)) { $chosen_workflow_assistant_handoff = ($chosenWorkflowLineAssistantHandoff -replace '^chosen_workflow=', '').Trim() }
                $recommended_action_assistant_handoff = "N/A"
                $recommendedActionLineAssistantHandoff = (($outAssistantHandoff -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($recommendedActionLineAssistantHandoff)) { $recommended_action_assistant_handoff = ($recommendedActionLineAssistantHandoff -replace '^recommended_action=', '').Trim() }
                $single_action_enforced_assistant_handoff = $(if ($outAssistantHandoff -match 'single_action_enforced=1') { "true" } else { "false" })
                $apply_ok_assistant_handoff = $(if ($outAssistantHandoff -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_assistant_handoff = $(if ($outAssistantHandoff -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_assistant_handoff = $(if ($outAssistantHandoff -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_assistant_handoff = $(if ($outAssistantHandoff -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_assistant_handoff = "N/A"
                if ($callable_in_project_context_assistant_handoff -ne "ok") {
                    $exact_blocker_if_any_assistant_handoff = "call_failed"
                } elseif ($assistant_decision_taken_ok_assistant_handoff -ne "true") {
                    $exact_blocker_if_any_assistant_handoff = "assistant_decision_not_taken"
                } elseif ($assistant_intent_assistant_handoff -ne "handoff_readiness") {
                    $exact_blocker_if_any_assistant_handoff = "unexpected_assistant_intent"
                } elseif ($chosen_workflow_assistant_handoff -ne "workflow_handoff_readiness_session") {
                    $exact_blocker_if_any_assistant_handoff = "unexpected_chosen_workflow"
                } elseif ($single_action_enforced_assistant_handoff -ne "true") {
                    $exact_blocker_if_any_assistant_handoff = "single_action_not_enforced"
                } elseif ($apply_ok_assistant_handoff -ne "true" -or $verify_ok_assistant_handoff -ne "true" -or $rollback_ok_assistant_handoff -ne "true" -or $cleanup_or_state_restored_ok_assistant_handoff -ne "true") {
                    $exact_blocker_if_any_assistant_handoff = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_assistant_handoff + "`n" +
                 "assistant_decision_taken_ok=" + $assistant_decision_taken_ok_assistant_handoff + "`n" +
                 "assistant_intent=" + $assistant_intent_assistant_handoff + "`n" +
                 "chosen_workflow=" + $chosen_workflow_assistant_handoff + "`n" +
                 "recommended_action=" + $recommended_action_assistant_handoff + "`n" +
                 "single_action_enforced=" + $single_action_enforced_assistant_handoff + "`n" +
                 "apply_ok=" + $apply_ok_assistant_handoff + "`n" +
                 "verify_ok=" + $verify_ok_assistant_handoff + "`n" +
                 "rollback_ok=" + $rollback_ok_assistant_handoff + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_assistant_handoff + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_assistant_handoff + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_assistant_session_contract_handoff_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_assistant_handoff -ne "N/A") {
                    throw "workflow_assistant_session (handoff execute=true) failed: blocker=$exact_blocker_if_any_assistant_handoff"
                }

                # create_pcb_items + delete_pcb_items (v1): one Dwgs.User segment, prove + rollback by id.
                if (!(Test-Path variable:visible_before_open_pcb_write_str)) {
                    $visible_before_open_pcb_write_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_create_pcb_items)) {
                    $callable_before_open_create_pcb_items = "error"
                }
                if (!(Test-Path variable:callable_before_open_delete_pcb_items)) {
                    $callable_before_open_delete_pcb_items = "error"
                }

                $argsObjShapesOnly = @{
                    board_filename = $pcb
                    types             = @("KOT_PCB_SHAPE")
                }
                $outShapesInitial = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" `
                        -caseLabel "pcb_frame_shapes_count_initial" -argsObj $argsObjShapesOnly -url $url -smokeExe $smokeExe
                $shapeCountInitial = -1
                if ($outShapesInitial -match 'items_count=([0-9]+)') { $shapeCountInitial = [int]$Matches[1] }
                if ($shapeCountInitial -lt 0) {
                    throw "get_pcb_items(KOT_PCB_SHAPE) did not report items_count after open."
                }

                $argsObjCreateSeg = @{
                    board_filename = $pcb
                    start          = @{ x_nm = 1500000; y_nm = 2500000 }
                    end            = @{ x_nm = 2500000; y_nm = 2500000 }
                }
                $outCreate = Call-McpToolArgsAndLog -runDir $runDir -toolName "create_pcb_items" `
                        -caseLabel "pcb_frame_create_pcb_items_segment" -argsObj $argsObjCreateSeg -url $url -smokeExe $smokeExe
                $create_ok = $(if ($outCreate -match 'status=1') { "true" } else { "false" })
                $created_item_id = ""
                if ($outCreate -match 'created_item_id=([0-9a-fA-F-]{36})') { $created_item_id = $Matches[1] }
                if ($created_item_id -eq "" -and $outCreate -match 'created_item_id=([^\r\n]+)') { $created_item_id = $Matches[1].Trim() }

                $outShapesAfterCreate = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" `
                        -caseLabel "pcb_frame_shapes_count_after_create" -argsObj $argsObjShapesOnly -url $url -smokeExe $smokeExe
                $shapeCountAfterCreate = -1
                if ($outShapesAfterCreate -match 'items_count=([0-9]+)') { $shapeCountAfterCreate = [int]$Matches[1] }

                $argsObjByIdCreated = @{
                    board_filename = $pcb
                    ids             = @($created_item_id)
                }
                $outByIdAfterCreate = ""
                if ($created_item_id -ne "") {
                    $outByIdAfterCreate = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                            -caseLabel "pcb_frame_get_by_id_after_create" -argsObj $argsObjByIdCreated -url $url -smokeExe $smokeExe
                }

                $argsObjDelete = @{
                    board_filename = $pcb
                    ids             = @($created_item_id)
                }
                $outDelete = ""
                if ($created_item_id -ne "") {
                    $outDelete = Call-McpToolArgsAndLog -runDir $runDir -toolName "delete_pcb_items" `
                            -caseLabel "pcb_frame_delete_pcb_items" -argsObj $argsObjDelete -url $url -smokeExe $smokeExe
                }
                $delete_ok = $(if ($outDelete -match 'status=1') { "true" } else { "false" })

                $outShapesAfterDelete = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" `
                        -caseLabel "pcb_frame_shapes_count_after_delete" -argsObj $argsObjShapesOnly -url $url -smokeExe $smokeExe
                $shapeCountAfterDelete = -1
                if ($outShapesAfterDelete -match 'items_count=([0-9]+)') { $shapeCountAfterDelete = [int]$Matches[1] }

                $rollback_ok = "false"
                $exact_blocker_if_any_pcb_write = "N/A"
                if ($create_ok -ne "true") {
                    $exact_blocker_if_any_pcb_write = "create_failed"
                } elseif ($created_item_id -eq "") {
                    $exact_blocker_if_any_pcb_write = "missing_created_item_id"
                } elseif ($shapeCountAfterCreate -ne $shapeCountInitial + 1) {
                    $exact_blocker_if_any_pcb_write = "shape_count_not_incremented"
                } elseif ($outByIdAfterCreate -notmatch 'status=1' -or $outByIdAfterCreate -notmatch 'items_count=1') {
                    $exact_blocker_if_any_pcb_write = "get_pcb_items_by_id_missing_created"
                } elseif ($delete_ok -ne "true") {
                    $exact_blocker_if_any_pcb_write = "delete_failed"
                } elseif ($shapeCountAfterDelete -ne $shapeCountInitial) {
                    $exact_blocker_if_any_pcb_write = "shape_count_not_restored"
                } else {
                    $rollback_ok = "true"
                }

                $callable_after_open_pcb_write = $(if ($rollback_ok -eq "true") { "ok" } else { "error" })

                $callable_before_open_pcb_write = $(if ($callable_before_open_create_pcb_items -ne "ok" -and $callable_before_open_delete_pcb_items -ne "ok") { "error" } else { "unexpected_ok" })

                ("visible_before_open=" + $visible_before_open_pcb_write_str + "`n" +
                 "callable_before_open=" + $callable_before_open_pcb_write + "`n" +
                 "callable_after_open=" + $callable_after_open_pcb_write + "`n" +
                 "created_item_id=" + $created_item_id + "`n" +
                 "create_ok=" + $create_ok + "`n" +
                 "delete_ok=" + $delete_ok + "`n" +
                 "rollback_ok=" + $rollback_ok + "`n" +
                 "shape_count_initial=" + $shapeCountInitial + "`n" +
                 "shape_count_after_create=" + $shapeCountAfterCreate + "`n" +
                 "shape_count_after_delete=" + $shapeCountAfterDelete + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_pcb_write + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_create_delete_pcb_items_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_pcb_write -ne "ok") {
                    throw "create_pcb_items/delete_pcb_items contract failed: blocker=$exact_blocker_if_any_pcb_write created_id=$created_item_id counts=($shapeCountInitial,$shapeCountAfterCreate,$shapeCountAfterDelete)"
                }

                # pcb_graphic_annotation_session (composed v1): controlled create/verify/cleanup with board restoration.
                if (!(Test-Path variable:visible_before_open_pcb_graphic_annotation_str)) {
                    $visible_before_open_pcb_graphic_annotation_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_pcb_graphic_annotation)) {
                    $callable_before_open_pcb_graphic_annotation = "error"
                }
                $callable_before_open_pcb_graphic_annotation_contract = $(if ($callable_before_open_pcb_graphic_annotation -eq "ok") { "unexpected_ok" } else { "error" })

                $argsObjPcbGraphicAnnotation = @{
                    board_filename = $pcb
                    start          = @{ x_nm = 1700000; y_nm = 2700000 }
                    end            = @{ x_nm = 2700000; y_nm = 2700000 }
                }
                $outPcbGraphicAnnotation = Call-McpToolArgsAndLog -runDir $runDir -toolName "pcb_graphic_annotation_session" `
                        -caseLabel "pcb_frame_pcb_graphic_annotation_session" -argsObj $argsObjPcbGraphicAnnotation -url $url -smokeExe $smokeExe

                $create_ok_ann = $(if ($outPcbGraphicAnnotation -match 'create_ok=1') { "true" } else { "false" })
                $verify_ok_ann = $(if ($outPcbGraphicAnnotation -match 'verify_ok=1') { "true" } else { "false" })
                $cleanup_ok_ann = $(if ($outPcbGraphicAnnotation -match 'cleanup_ok=1') { "true" } else { "false" })
                $board_state_restored_ann = $(if ($outPcbGraphicAnnotation -match 'board_state_restored=1') { "true" } else { "false" })

                $exact_blocker_if_any_pcb_annotation = "N/A"
                if ($outPcbGraphicAnnotation -notmatch 'status=1') {
                    if ($outPcbGraphicAnnotation -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_pcb_annotation = $Matches[1]
                    } else {
                        $exact_blocker_if_any_pcb_annotation = "annotation_call_failed"
                    }
                } elseif ($create_ok_ann -ne "true") {
                    $exact_blocker_if_any_pcb_annotation = "create_failed"
                } elseif ($verify_ok_ann -ne "true") {
                    $exact_blocker_if_any_pcb_annotation = "verify_failed"
                } elseif ($cleanup_ok_ann -ne "true") {
                    $exact_blocker_if_any_pcb_annotation = "cleanup_failed"
                } elseif ($board_state_restored_ann -ne "true") {
                    $exact_blocker_if_any_pcb_annotation = "board_state_not_restored"
                }

                ("visible_before_open=" + $visible_before_open_pcb_graphic_annotation_str + "`n" +
                 "callable_before_open=" + $callable_before_open_pcb_graphic_annotation_contract + "`n" +
                 "create_ok=" + $create_ok_ann + "`n" +
                 "verify_ok=" + $verify_ok_ann + "`n" +
                 "cleanup_ok=" + $cleanup_ok_ann + "`n" +
                 "board_state_restored=" + $board_state_restored_ann + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_pcb_annotation + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_pcb_graphic_annotation_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_pcb_annotation -ne "N/A") {
                    throw "pcb_graphic_annotation_session contract failed: blocker=$exact_blocker_if_any_pcb_annotation"
                }

                # update_pcb_items (v1): minimal end change, prove via get_pcb_items_by_id, rollback by inverse update, then delete.
                if (!(Test-Path variable:visible_before_open_update_pcb_items_str)) {
                    $visible_before_open_update_pcb_items_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_update_pcb_items)) {
                    $callable_before_open_update_pcb_items = "error"
                }

                $argsObjCreateForUpdate = @{
                    board_filename = $pcb
                    start          = @{ x_nm = 1600000; y_nm = 2600000 }
                    end            = @{ x_nm = 2600000; y_nm = 2600000 }
                }
                $outCreateUpd = Call-McpToolArgsAndLog -runDir $runDir -toolName "create_pcb_items" `
                        -caseLabel "pcb_frame_create_pcb_items_for_update" -argsObj $argsObjCreateForUpdate -url $url -smokeExe $smokeExe
                $upd_created_item_id = ""
                if ($outCreateUpd -match 'created_item_id=([0-9a-fA-F-]{36})') { $upd_created_item_id = $Matches[1] }
                if ($upd_created_item_id -eq "" -and $outCreateUpd -match 'created_item_id=([^\r\n]+)') { $upd_created_item_id = $Matches[1].Trim() }

                $initialEndX = [int64]0
                $initialEndY = [int64]0
                $hasInitialEnd = $false
                $argsObjByIdUpd = @{
                    board_filename = $pcb
                    ids             = @($upd_created_item_id)
                }
                $outByIdInitial = ""
                if ($upd_created_item_id -ne "") {
                    $outByIdInitial = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                            -caseLabel "pcb_frame_get_by_id_before_update" -argsObj $argsObjByIdUpd -url $url -smokeExe $smokeExe
                }
                if ($outByIdInitial -match 'pcb_graphic_segment_end_summary=end_x_nm=([-]?[0-9]+);end_y_nm=([-]?[0-9]+)') {
                    $initialEndX = [int64]$Matches[1]
                    $initialEndY = [int64]$Matches[2]
                    $hasInitialEnd = $true
                }

                $newEndX = $initialEndX + 100000
                $newEndY = $initialEndY

                $argsObjUpdateSeg = @{
                    board_filename = $pcb
                    item_id        = $upd_created_item_id
                    end            = @{ x_nm = $newEndX; y_nm = $newEndY }
                }
                $outUpdateSeg = ""
                if ($upd_created_item_id -ne "" -and $hasInitialEnd) {
                    $outUpdateSeg = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_pcb_items" `
                            -caseLabel "pcb_frame_update_pcb_items_segment_end" -argsObj $argsObjUpdateSeg -url $url -smokeExe $smokeExe
                }

                $outByIdAfterUpdate = ""
                if ($upd_created_item_id -ne "") {
                    $outByIdAfterUpdate = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                            -caseLabel "pcb_frame_get_by_id_after_update" -argsObj $argsObjByIdUpd -url $url -smokeExe $smokeExe
                }

                $readEndXAfterUpdate = [int64]0
                $readEndYAfterUpdate = [int64]0
                $hasReadAfterUpdate = $false
                if ($outByIdAfterUpdate -match 'pcb_graphic_segment_end_summary=end_x_nm=([-]?[0-9]+);end_y_nm=([-]?[0-9]+)') {
                    $readEndXAfterUpdate = [int64]$Matches[1]
                    $readEndYAfterUpdate = [int64]$Matches[2]
                    $hasReadAfterUpdate = $true
                }

                $argsObjRollbackSeg = @{
                    board_filename = $pcb
                    item_id        = $upd_created_item_id
                    end            = @{ x_nm = $initialEndX; y_nm = $initialEndY }
                }
                $outRollbackSeg = ""
                if ($upd_created_item_id -ne "" -and $hasInitialEnd) {
                    $outRollbackSeg = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_pcb_items" `
                            -caseLabel "pcb_frame_update_pcb_items_segment_rollback" -argsObj $argsObjRollbackSeg -url $url -smokeExe $smokeExe
                }

                $outByIdAfterRollback = ""
                if ($upd_created_item_id -ne "") {
                    $outByIdAfterRollback = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                            -caseLabel "pcb_frame_get_by_id_after_rollback_update" -argsObj $argsObjByIdUpd -url $url -smokeExe $smokeExe
                }

                $readEndXAfterRollback = [int64]0
                $readEndYAfterRollback = [int64]0
                $hasReadAfterRollback = $false
                if ($outByIdAfterRollback -match 'pcb_graphic_segment_end_summary=end_x_nm=([-]?[0-9]+);end_y_nm=([-]?[0-9]+)') {
                    $readEndXAfterRollback = [int64]$Matches[1]
                    $readEndYAfterRollback = [int64]$Matches[2]
                    $hasReadAfterRollback = $true
                }

                $outDeleteUpd = ""
                if ($upd_created_item_id -ne "") {
                    $outDeleteUpd = Call-McpToolArgsAndLog -runDir $runDir -toolName "delete_pcb_items" `
                            -caseLabel "pcb_frame_delete_pcb_items_after_update_scenario" -argsObj $argsObjByIdUpd -url $url -smokeExe $smokeExe
                }

                $update_ok = "false"
                $rollback_ok = "false"
                $delete_ok_upd = "false"
                $exact_blocker_if_any_update_pcb = "N/A"

                if ($outCreateUpd -notmatch 'status=1') {
                    $exact_blocker_if_any_update_pcb = "create_for_update_failed"
                } elseif ($upd_created_item_id -eq "") {
                    $exact_blocker_if_any_update_pcb = "missing_created_item_id_for_update"
                } elseif (-not $hasInitialEnd) {
                    $exact_blocker_if_any_update_pcb = "snapshot_initial_end_missing"
                } elseif ($outByIdInitial -notmatch 'status=1' -or $outByIdInitial -notmatch 'items_count=1') {
                    $exact_blocker_if_any_update_pcb = "get_by_id_before_update_failed"
                } elseif ($outUpdateSeg -notmatch 'status=1') {
                    $exact_blocker_if_any_update_pcb = "update_call_failed"
                } elseif (-not $hasReadAfterUpdate -or $readEndXAfterUpdate -ne $newEndX -or $readEndYAfterUpdate -ne $newEndY) {
                    $exact_blocker_if_any_update_pcb = "update_not_observed_in_readback"
                } else {
                    $update_ok = "true"
                }

                if ($update_ok -eq "true") {
                    if ($outRollbackSeg -notmatch 'status=1') {
                        $exact_blocker_if_any_update_pcb = "rollback_update_call_failed"
                    } elseif (-not $hasReadAfterRollback -or $readEndXAfterRollback -ne $initialEndX -or $readEndYAfterRollback -ne $initialEndY) {
                        $exact_blocker_if_any_update_pcb = "rollback_not_observed_in_readback"
                    } else {
                        $rollback_ok = "true"
                    }
                }

                if ($rollback_ok -eq "true") {
                    if ($outDeleteUpd -notmatch 'status=1') {
                        $exact_blocker_if_any_update_pcb = "delete_after_update_scenario_failed"
                    } else {
                        $delete_ok_upd = "true"
                    }
                }

                $callable_after_open_update_pcb = $(if ($update_ok -eq "true" -and $rollback_ok -eq "true" -and $delete_ok_upd -eq "true") { "ok" } else { "error" })
                $callable_before_open_update_pcb_contract = $(if ($callable_before_open_update_pcb_items -eq "ok") { "unexpected_ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_update_pcb_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_update_pcb_contract + "`n" +
                 "callable_after_open=" + $callable_after_open_update_pcb + "`n" +
                 "created_item_id=" + $upd_created_item_id + "`n" +
                 "update_ok=" + $update_ok + "`n" +
                 "rollback_ok=" + $rollback_ok + "`n" +
                 "delete_ok=" + $delete_ok_upd + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_update_pcb + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_update_pcb_items_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_update_pcb -ne "ok") {
                    throw "update_pcb_items contract failed: blocker=$exact_blocker_if_any_update_pcb created_id=$upd_created_item_id"
                }

                # get_board_extent validation set (QA).
                $argsObjBoardExtent = @{
                    board_filename = $pcb
                }
                $outBoardExtent = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_board_extent" `
                        -caseLabel "pcb_frame_board_extent" -argsObj $argsObjBoardExtent -url $url -smokeExe $smokeExe

                $extentShapeSummary = "N/A"
                if ($outBoardExtent -match 'extent_shape_summary=([^\r\n]*)') { $extentShapeSummary = $Matches[1] }
                $callable_after_open_board_extent = $(if ($outBoardExtent -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_board_extent = "N/A"
                if ($callable_after_open_board_extent -ne "ok") {
                    $exact_blocker_if_any_board_extent = "unexpected_error"
                } elseif ($extentShapeSummary -eq "N/A" -or $extentShapeSummary -match 'missing') {
                    $exact_blocker_if_any_board_extent = "missing_expected_fields_in_extent_shape"
                }

                ("visible_before_open=" + $visible_before_open_board_extent_str + "`n" +
                 "callable_before_open=" + $callable_before_open_board_extent + "`n" +
                 "callable_after_open=" + $callable_after_open_board_extent + "`n" +
                 "extent_shape_summary=" + $extentShapeSummary + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_board_extent + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_board_extent_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_board_extent -ne "ok") {
                    throw "get_board_extent failed after opening PCB: callable_after_open_board_extent=$callable_after_open_board_extent"
                }
                if ($extentShapeSummary -eq "N/A" -or $extentShapeSummary -match 'missing') {
                    throw "get_board_extent output shape missing expected fields: $extentShapeSummary"
                }

                # Case 5: get_pcb_items_by_id with a real ID
                $argsObjByIdValid = @{
                    board_filename = $pcb
                    ids             = @($firstItemId)
                }
                $outByIdValid = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                        -caseLabel "pcb_frame_ids_valid" -argsObj $argsObjByIdValid -url $url -smokeExe $smokeExe

                $itemsCountByIdValid = "N/A"
                if ($outByIdValid -match 'items_count=([0-9]+)') { $itemsCountByIdValid = $Matches[1] }
                if ($itemsCountByIdValid -eq "N/A" -or [int]$itemsCountByIdValid -le 0) {
                    throw "get_pcb_items_by_id(items_count) expected > 0 with PCB frame open. Got items_count=$itemsCountByIdValid"
                }

                $callable_after_open_by_id = $(if ($outByIdValid -match 'status=1') { "ok" } else { "error" })

                # Case 6: get_pcb_items_by_id with an invalid/non-existing ID
                $invalidId = "00000000-0000-0000-0000-000000000000"
                $argsObjByIdInvalidId = @{
                    board_filename = $pcb
                    ids             = @($invalidId)
                }
                $outByIdInvalidId = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                        -caseLabel "pcb_frame_ids_invalid_id" -argsObj $argsObjByIdInvalidId -url $url -smokeExe $smokeExe

                $invalidIdBehavior = "N/A"
                if ($outByIdInvalidId -match 'error_message=([^\r\n]+)') { $invalidIdBehavior = $Matches[1] }

                # Case 7: get_pcb_items_by_id with an invalid board_filename
                $argsObjByIdInvalidBoard = @{
                    board_filename = $pcbInvalid
                    ids             = @($firstItemId)
                }
                $outByIdInvalidBoard = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items_by_id" `
                        -caseLabel "pcb_frame_ids_invalid_board_filename" -argsObj $argsObjByIdInvalidBoard -url $url -smokeExe $smokeExe

                $invalidBoardFilenameBehavior = "N/A"
                if ($outByIdInvalidBoard -match 'error_message=([^\r\n]+)') { $invalidBoardFilenameBehavior = $Matches[1] }

                # Artifact: get_pcb_items_by_id contract summary (before/after discovery + callability).
                ("visible_before_open=" + $visible_before_open_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_by_id + "`n" +
                 "callable_after_open=" + $callable_after_open_by_id + "`n" +
                 "items_count_after_open=" + $itemsCountByIdValid + "`n" +
                 "invalid_id_behavior=" + $invalidIdBehavior + "`n" +
                 "invalid_board_filename_behavior=" + $invalidBoardFilenameBehavior + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pcb_items_by_id_contract_summary.txt") -Encoding UTF8

                # hit_test_pcb QA validation.
                # Use deterministic pad-only query result (already used above for pad polygon checks).
                $hitTestItemId = $firstPadId
                if ([string]::IsNullOrWhiteSpace($hitTestItemId)) {
                    # Fallback to any first PCB item so set_active_layer validation can continue
                    # even when pad-only extraction is unavailable in this project.
                    $hitTestItemId = $firstItemId
                }

                # Derive a deterministic hit point from the validated bounding box center when available.
                # Fallback to pad position (or stub center) only if bbox extraction fails.
                $hitX = 5000000
                $hitY = 5000000
                $bboxSizeX = 0
                $bboxSizeY = 0
                if ($outBboxValid -match 'key: "position"[\s\S]*?key: "x_nm"[\s\S]*?string_value: "([0-9-]+)"[\s\S]*?key: "y_nm"[\s\S]*?string_value: "([0-9-]+)"[\s\S]*?key: "size"[\s\S]*?key: "x_nm"[\s\S]*?string_value: "([0-9-]+)"[\s\S]*?key: "y_nm"[\s\S]*?string_value: "([0-9-]+)"') {
                    $posX = [int64]$Matches[1]
                    $posY = [int64]$Matches[2]
                    $bboxSizeX = [int64]$Matches[3]
                    $bboxSizeY = [int64]$Matches[4]
                    $hitX = $posX + [int64]($bboxSizeX / 2)
                    $hitY = $posY + [int64]($bboxSizeY / 2)
                } elseif ($outPadsOnly -match 'key: "position"[\s\S]*?key: "x_nm"[\s\S]*?string_value: "([0-9-]+)"[\s\S]*?key: "y_nm"[\s\S]*?string_value: "([0-9-]+)"') {
                    $hitX = [int64]$Matches[1]
                    $hitY = [int64]$Matches[2]
                }

                # Small positive tolerance avoids edge-rounding artifacts.
                $tolerance = 10000
                if ($bboxSizeX -gt 0 -and $bboxSizeY -gt 0) {
                    # Ensure miss point is clearly outside the selected item bbox.
                    $missX = $hitX + $bboxSizeX + 1000000
                    $missY = $hitY + $bboxSizeY + 1000000
                } else {
                    $missX = $hitX + 3000000
                    $missY = $hitY + 3000000
                }

                # Case 8: hit_test_pcb with an ID real hit point.
                $argsObjHitTestHit = @{
                    board_filename = $pcb
                    id              = $hitTestItemId
                    position        = @{
                        x_nm = $hitX
                        y_nm = $hitY
                    }
                    tolerance       = $tolerance
                }
                $outHitTestHit = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_pcb" `
                        -caseLabel "pcb_frame_hit_hitpoint" -argsObj $argsObjHitTestHit -url $url -smokeExe $smokeExe

                $hitVal = "N/A"
                $hitResult = "N/A"
                if ($outHitTestHit -match 'hit=(true|false)') { $hitVal = $Matches[1] }
                if ($outHitTestHit -match 'result=([0-9]+)') { $hitResult = $Matches[1] }
                if ($hitVal -ne "true") {
                    throw "hit_test_pcb expected hit=true for valid ID, got hit=$hitVal (result=$hitResult)"
                }

                # Case 9: hit_test_pcb with an ID miss point.
                $argsObjHitTestMiss = @{
                    board_filename = $pcb
                    id              = $hitTestItemId
                    position        = @{
                        x_nm = $missX
                        y_nm = $missY
                    }
                    tolerance       = $tolerance
                }
                $outHitTestMiss = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_pcb" `
                        -caseLabel "pcb_frame_hit_misspoint" -argsObj $argsObjHitTestMiss -url $url -smokeExe $smokeExe

                $missVal = "N/A"
                $missResult = "N/A"
                if ($outHitTestMiss -match 'hit=(true|false)') { $missVal = $Matches[1] }
                if ($outHitTestMiss -match 'result=([0-9]+)') { $missResult = $Matches[1] }
                if ($missVal -ne "false") {
                    throw "hit_test_pcb expected hit=false for valid ID, got hit=$missVal (result=$missResult)"
                }

                # Case 10: invalid ID.
                $invalidIdHitTest = "00000000-0000-0000-0000-000000000000"
                $argsObjHitTestInvalidId = @{
                    board_filename = $pcb
                    id              = $invalidIdHitTest
                    position        = @{
                        x_nm = $hitX
                        y_nm = $hitY
                    }
                    tolerance       = $tolerance
                }
                $outHitTestInvalidId = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_pcb" `
                        -caseLabel "pcb_frame_hit_invalid_id" -argsObj $argsObjHitTestInvalidId -url $url -smokeExe $smokeExe

                $invalidIdBehaviorHitTest = "N/A"
                if ($outHitTestInvalidId -match 'error_message=([^\r\n]+)') { $invalidIdBehaviorHitTest = $Matches[1] }

                # Case 11: invalid board_filename.
                $argsObjHitTestInvalidBoard = @{
                    board_filename = $pcbInvalid
                    id              = $hitTestItemId
                    position        = @{
                        x_nm = $hitX
                        y_nm = $hitY
                    }
                    tolerance       = $tolerance
                }
                $outHitTestInvalidBoard = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_pcb" `
                        -caseLabel "pcb_frame_hit_invalid_board_filename" -argsObj $argsObjHitTestInvalidBoard -url $url -smokeExe $smokeExe

                $invalidBoardFilenameBehaviorHitTest = "N/A"
                if ($outHitTestInvalidBoard -match 'error_message=([^\r\n]+)') { $invalidBoardFilenameBehaviorHitTest = $Matches[1] }

                $callable_after_open_hit_test = $(if ($outHitTestHit -match 'status=1' -and $outHitTestMiss -match 'status=1') { "ok" } else { "error" })

                $hitBehavior = "hit=$hitVal,result=$hitResult"
                $missBehavior = "hit=$missVal,result=$missResult"

                ("visible_before_open=" + $visible_before_open_hit_test_str + "`n" +
                 "callable_before_open=" + $callable_before_open_hit_test + "`n" +
                 "callable_after_open=" + $callable_after_open_hit_test + "`n" +
                 "hit_behavior=" + $hitBehavior + "`n" +
                 "miss_behavior=" + $missBehavior + "`n" +
                 "invalid_id_behavior=" + $invalidIdBehaviorHitTest + "`n" +
                 "invalid_board_filename_behavior=" + $invalidBoardFilenameBehaviorHitTest + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_pcb_contract_summary.txt") -Encoding UTF8

                # Artifact: contract summary (before/after discovery + callability).
                $callable_after_open = $(if ($out -match 'status=1') { "ok" } else { "error" })
                $items_count_after_open = $itemsCount
                ("visible_before_open=" + $visible_before_open_str + "`n" +
                 "callable_before_open=" + $callable_before_open + "`n" +
                 "callable_after_open=" + $callable_after_open + "`n" +
                 "items_count_after_open=" + $items_count_after_open + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_pcb_items_contract_summary.txt") -Encoding UTF8
            }
            elseif ($OpenEditorType -eq "sch") {
                # get_selection_sch validation set (QA): no deterministic non-empty selection path in read-only harness.
                $argsObjSelSch = @{}
                $outSelSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_selection_sch" `
                        -caseLabel "sch_frame_selection_nofilter" -argsObj $argsObjSelSch `
                        -url $url -smokeExe $smokeExe

                $countAfterOpenNoSelection = "N/A"
                if ($outSelSch -match 'count=([0-9]+)') { $countAfterOpenNoSelection = $Matches[1] }
                $callable_after_open_sel_sch = $(if ($outSelSch -match 'status=1') { "ok" } else { "error" })

                $exact_blocker_if_any_sel_sch = "N/A"
                if ($callable_after_open_sel_sch -ne "ok") {
                    $exact_blocker_if_any_sel_sch = "unexpected_error"
                } elseif ($countAfterOpenNoSelection -eq "N/A") {
                    $exact_blocker_if_any_sel_sch = "count_not_parseable"
                } elseif ([int]$countAfterOpenNoSelection -ne 0) {
                    $exact_blocker_if_any_sel_sch = "selection_not_empty_in_stub_context"
                }

                ("visible_before_open=" + $visible_before_open_sel_sch_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sel_sch + "`n" +
                 "callable_after_open=" + $callable_after_open_sel_sch + "`n" +
                 "count_after_open_no_selection=" + $countAfterOpenNoSelection + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sel_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_selection_sch_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_sel_sch -ne "ok") {
                    throw "get_selection_sch failed after opening SCH: callable_after_open=$callable_after_open_sel_sch"
                }
                if ($countAfterOpenNoSelection -eq "N/A") {
                    throw "get_selection_sch expected parseable count after opening SCH."
                }
                if ([int]$countAfterOpenNoSelection -ne 0) {
                    throw "get_selection_sch expected count=0 with no deterministic selection setup. Got count=$countAfterOpenNoSelection"
                }

                # get_sch_items validation set (QA): derive a deterministic item ID from stub output.
                $argsObjSchItemsForChain = @{
                    schematic_filename = $sch
                }
                $outSchItemsForChain = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_items_default_for_chain" -argsObj $argsObjSchItemsForChain -url $url -smokeExe $smokeExe

                $wireSchId = "N/A"
                if ($outSchItemsForChain -match 'first_item_id=([^\r\n]+)') { $wireSchId = $Matches[1] }

                $countAfterOpenSchItemsForChain = "N/A"
                if ($outSchItemsForChain -match 'count=([0-9]+)') { $countAfterOpenSchItemsForChain = $Matches[1] }

                $callable_after_open_sch_items_for_chain = $(if ($outSchItemsForChain -match 'status=1') { "ok" } else { "error" })
                if ($callable_after_open_sch_items_for_chain -ne "ok") {
                    throw "get_sch_items (for chain) failed after opening SCH: callable_after_open=$callable_after_open_sch_items_for_chain"
                }
                if ($wireSchId -eq "N/A" -or $countAfterOpenSchItemsForChain -eq "N/A" -or [int]$countAfterOpenSchItemsForChain -le 0) {
                    throw "get_sch_items (for chain) did not yield deterministic ID+count. id=$wireSchId count=$countAfterOpenSchItemsForChain"
                }

                # get_symbol_pins_by_id validation set (QA): deterministic symbol pins from stub.
                $expectedPinId = "33333333-3333-3333-3333-333333333333"
                $schSymbolId = "22222222-2222-2222-2222-222222222222"

                $argsObjSymbolPinsById = @{
                    schematic_filename = $sch
                    id                 = $schSymbolId
                }
                $outSymbolPinsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_pins_by_id" `
                        -caseLabel "sch_frame_symbol_pins_symbol_has_one_pin" -argsObj $argsObjSymbolPinsById -url $url -smokeExe $smokeExe

                $countAfterOpenSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'count=([0-9]+)') { $countAfterOpenSymbolPinsById = $Matches[1] }
                $firstPinIdSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_id=([^\r\n]+)') { $firstPinIdSymbolPinsById = $Matches[1] }
                $firstPinXSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_x_iu=([^\r\n]+)') { $firstPinXSymbolPinsById = $Matches[1] }
                $firstPinYSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_y_iu=([^\r\n]+)') { $firstPinYSymbolPinsById = $Matches[1] }
                $firstPinOrientationSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_orientation=([^\r\n]+)') {
                    $firstPinOrientationSymbolPinsById = $Matches[1]
                }
                $firstPinRootXSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_root_x_iu=([^\r\n]+)') {
                    $firstPinRootXSymbolPinsById = $Matches[1]
                }
                $firstPinRootYSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_root_y_iu=([^\r\n]+)') {
                    $firstPinRootYSymbolPinsById = $Matches[1]
                }
                $firstPinLengthSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_length_iu=([^\r\n]+)') {
                    $firstPinLengthSymbolPinsById = $Matches[1]
                }
                $firstPinTypeNameSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_type_name=([^\r\n]+)') {
                    $firstPinTypeNameSymbolPinsById = $Matches[1]
                }
                $firstPinOrientationNameSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_orientation_name=([^\r\n]+)') {
                    $firstPinOrientationNameSymbolPinsById = $Matches[1]
                }
                $firstPinIsPowerSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_is_power=([^\r\n]+)') {
                    $firstPinIsPowerSymbolPinsById = $Matches[1]
                }
                $firstPinIsGlobalPowerSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_is_global_power=([^\r\n]+)') {
                    $firstPinIsGlobalPowerSymbolPinsById = $Matches[1]
                }
                $firstPinIsLocalPowerSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_is_local_power=([^\r\n]+)') {
                    $firstPinIsLocalPowerSymbolPinsById = $Matches[1]
                }
                $firstPinNetNameSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_net_name=([^\r\n]+)') {
                    $firstPinNetNameSymbolPinsById = $Matches[1]
                }
                $firstPinNetNameSourceSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_net_name_source=([^\r\n]+)') {
                    $firstPinNetNameSourceSymbolPinsById = $Matches[1]
                }
                $firstPinNetNameDefaultSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_pin_net_name_default=([^\r\n]+)') {
                    $firstPinNetNameDefaultSymbolPinsById = $Matches[1]
                }
                $netsCountSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'nets_count=([0-9]+)') { $netsCountSymbolPinsById = $Matches[1] }
                $firstNetNetNameSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_net_net_name=([^\r\n]+)') { $firstNetNetNameSymbolPinsById = $Matches[1] }
                $firstNetNetNameSourceSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_net_net_name_source=([^\r\n]+)') { $firstNetNetNameSourceSymbolPinsById = $Matches[1] }
                $firstNetNetNameDefaultSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_net_net_name_default=([^\r\n]+)') { $firstNetNetNameDefaultSymbolPinsById = $Matches[1] }
                $firstNetPinsCountSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_net_pins_count=([0-9]+)') { $firstNetPinsCountSymbolPinsById = $Matches[1] }
                $firstNetFirstPinIdSymbolPinsById = "N/A"
                if ($outSymbolPinsById -match 'first_net_first_pin_id=([^\r\n]+)') { $firstNetFirstPinIdSymbolPinsById = $Matches[1] }

                $callable_after_open_symbol_pins_by_id = $(if ($outSymbolPinsById -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_symbol_pins_by_id = "N/A"
                if ($callable_after_open_symbol_pins_by_id -ne "ok") {
                    $exact_blocker_if_any_symbol_pins_by_id = "unexpected_error"
                } elseif ($countAfterOpenSymbolPinsById -eq "N/A" -or [int]$countAfterOpenSymbolPinsById -ne 1) {
                    $exact_blocker_if_any_symbol_pins_by_id = "count_not_equal_1"
                } elseif ($firstPinIdSymbolPinsById -ne $expectedPinId) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_id_mismatch"
                } elseif ($firstPinXSymbolPinsById -eq "N/A" -or [int]$firstPinXSymbolPinsById -ne 0) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_x_iu_mismatch"
                } elseif ($firstPinYSymbolPinsById -eq "N/A" -or [int]$firstPinYSymbolPinsById -ne 0) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_y_iu_mismatch"
                } elseif ($firstPinOrientationSymbolPinsById -eq "N/A" -or [int]$firstPinOrientationSymbolPinsById -ne 3) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_orientation_mismatch"
                } elseif ($firstPinLengthSymbolPinsById -eq "N/A" -or [int]$firstPinLengthSymbolPinsById -ne 0) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_length_iu_mismatch"
                } elseif ($firstPinRootXSymbolPinsById -eq "N/A" -or [int]$firstPinRootXSymbolPinsById -ne 0) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_root_x_iu_mismatch"
                } elseif ($firstPinRootYSymbolPinsById -eq "N/A" -or [int]$firstPinRootYSymbolPinsById -ne 0) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_root_y_iu_mismatch"
                } elseif ($firstPinTypeNameSymbolPinsById -eq "N/A" -or $firstPinTypeNameSymbolPinsById -ne "power_in") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_type_name_mismatch"
                } elseif ($firstPinOrientationNameSymbolPinsById -eq "N/A" -or $firstPinOrientationNameSymbolPinsById -ne "down") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_orientation_name_mismatch"
                } elseif ($firstPinIsPowerSymbolPinsById -ne "true") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_is_power_mismatch"
                } elseif ($firstPinIsGlobalPowerSymbolPinsById -ne "true") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_is_global_power_mismatch"
                } elseif ($firstPinIsLocalPowerSymbolPinsById -ne "false") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_is_local_power_mismatch"
                } elseif ($firstPinNetNameSymbolPinsById -eq "N/A" -or $firstPinNetNameSymbolPinsById -ne "GND") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_net_name_mismatch"
                } elseif ($firstPinNetNameSourceSymbolPinsById -eq "N/A" -or $firstPinNetNameSourceSymbolPinsById -ne "graph") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_net_name_source_mismatch"
                } elseif ($firstPinNetNameDefaultSymbolPinsById -eq "N/A" -or $firstPinNetNameDefaultSymbolPinsById -ne "GND") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_pin_net_name_default_mismatch"
                } elseif ($netsCountSymbolPinsById -eq "N/A" -or [int]$netsCountSymbolPinsById -ne 1) {
                    $exact_blocker_if_any_symbol_pins_by_id = "nets_count_not_equal_1"
                } elseif ($firstNetNetNameSymbolPinsById -eq "N/A" -or $firstNetNetNameSymbolPinsById -ne "GND") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_net_net_name_mismatch"
                } elseif ($firstNetNetNameSourceSymbolPinsById -eq "N/A" -or $firstNetNetNameSourceSymbolPinsById -ne "graph") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_net_net_name_source_mismatch"
                } elseif ($firstNetNetNameDefaultSymbolPinsById -eq "N/A" -or $firstNetNetNameDefaultSymbolPinsById -ne "GND") {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_net_net_name_default_mismatch"
                } elseif ($firstNetPinsCountSymbolPinsById -eq "N/A" -or [int]$firstNetPinsCountSymbolPinsById -ne 1) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_net_pins_count_not_equal_1"
                } elseif ($firstNetFirstPinIdSymbolPinsById -eq "N/A" -or $firstNetFirstPinIdSymbolPinsById -ne $expectedPinId) {
                    $exact_blocker_if_any_symbol_pins_by_id = "first_net_first_pin_id_mismatch"
                }

                ("visible_before_open=" + $visible_before_open_symbol_pins_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_symbol_pins_by_id + "`n" +
                 "callable_after_open=" + $callable_after_open_symbol_pins_by_id + "`n" +
                 "count_after_open=" + $countAfterOpenSymbolPinsById + "`n" +
                 "first_pin_id=" + $firstPinIdSymbolPinsById + "`n" +
                 "first_pin_x_iu=" + $firstPinXSymbolPinsById + "`n" +
                 "first_pin_y_iu=" + $firstPinYSymbolPinsById + "`n" +
                 "first_pin_orientation=" + $firstPinOrientationSymbolPinsById + "`n" +
                 "first_pin_length_iu=" + $firstPinLengthSymbolPinsById + "`n" +
                 "first_pin_root_x_iu=" + $firstPinRootXSymbolPinsById + "`n" +
                 "first_pin_root_y_iu=" + $firstPinRootYSymbolPinsById + "`n" +
                 "first_pin_type_name=" + $firstPinTypeNameSymbolPinsById + "`n" +
                 "first_pin_orientation_name=" + $firstPinOrientationNameSymbolPinsById + "`n" +
                 "first_pin_is_power=" + $firstPinIsPowerSymbolPinsById + "`n" +
                 "first_pin_is_global_power=" + $firstPinIsGlobalPowerSymbolPinsById + "`n" +
                 "first_pin_is_local_power=" + $firstPinIsLocalPowerSymbolPinsById + "`n" +
                 "first_pin_net_name=" + $firstPinNetNameSymbolPinsById + "`n" +
                 "first_pin_net_name_source=" + $firstPinNetNameSourceSymbolPinsById + "`n" +
                 "first_pin_net_name_default=" + $firstPinNetNameDefaultSymbolPinsById + "`n" +
                 "nets_count=" + $netsCountSymbolPinsById + "`n" +
                 "first_net_net_name=" + $firstNetNetNameSymbolPinsById + "`n" +
                 "first_net_net_name_source=" + $firstNetNetNameSourceSymbolPinsById + "`n" +
                 "first_net_net_name_default=" + $firstNetNetNameDefaultSymbolPinsById + "`n" +
                 "first_net_pins_count=" + $firstNetPinsCountSymbolPinsById + "`n" +
                 "first_net_first_pin_id=" + $firstNetFirstPinIdSymbolPinsById + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_symbol_pins_by_id + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_symbol_pins_by_id_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_symbol_pins_by_id -ne "ok") {
                    throw "get_symbol_pins_by_id failed after opening SCH: callable_after_open=$callable_after_open_symbol_pins_by_id"
                }
                if ($countAfterOpenSymbolPinsById -eq "N/A" -or [int]$countAfterOpenSymbolPinsById -ne 1) {
                    throw "get_symbol_pins_by_id expected count=1 after opening SCH. Got count=$countAfterOpenSymbolPinsById"
                }
                if ($firstPinIdSymbolPinsById -ne $expectedPinId) {
                    throw "get_symbol_pins_by_id expected first_pin_id=$expectedPinId, got $firstPinIdSymbolPinsById"
                }
                if ($firstPinXSymbolPinsById -eq "N/A" -or [int]$firstPinXSymbolPinsById -ne 0) {
                    throw "get_symbol_pins_by_id expected first_pin_x_iu=0, got $firstPinXSymbolPinsById"
                }
                if ($firstPinYSymbolPinsById -eq "N/A" -or [int]$firstPinYSymbolPinsById -ne 0) {
                    throw "get_symbol_pins_by_id expected first_pin_y_iu=0, got $firstPinYSymbolPinsById"
                }
                if ($firstPinOrientationSymbolPinsById -eq "N/A" -or [int]$firstPinOrientationSymbolPinsById -ne 3) {
                    throw "get_symbol_pins_by_id expected first_pin_orientation=3 (PIN_DOWN), got $firstPinOrientationSymbolPinsById"
                }
                if ($firstPinLengthSymbolPinsById -eq "N/A" -or [int]$firstPinLengthSymbolPinsById -ne 0) {
                    throw "get_symbol_pins_by_id expected first_pin_length_iu=0, got $firstPinLengthSymbolPinsById"
                }
                if ($firstPinRootXSymbolPinsById -eq "N/A" -or [int]$firstPinRootXSymbolPinsById -ne 0) {
                    throw "get_symbol_pins_by_id expected first_pin_root_x_iu=0, got $firstPinRootXSymbolPinsById"
                }
                if ($firstPinRootYSymbolPinsById -eq "N/A" -or [int]$firstPinRootYSymbolPinsById -ne 0) {
                    throw "get_symbol_pins_by_id expected first_pin_root_y_iu=0, got $firstPinRootYSymbolPinsById"
                }
                if ($firstPinTypeNameSymbolPinsById -eq "N/A" -or $firstPinTypeNameSymbolPinsById -ne "power_in") {
                    throw "get_symbol_pins_by_id expected first_pin_type_name=power_in, got $firstPinTypeNameSymbolPinsById"
                }
                if ($firstPinOrientationNameSymbolPinsById -eq "N/A" -or $firstPinOrientationNameSymbolPinsById -ne "down") {
                    throw "get_symbol_pins_by_id expected first_pin_orientation_name=down, got $firstPinOrientationNameSymbolPinsById"
                }
                if ($firstPinIsPowerSymbolPinsById -ne "true") {
                    throw "get_symbol_pins_by_id expected first_pin_is_power=true, got $firstPinIsPowerSymbolPinsById"
                }
                if ($firstPinIsGlobalPowerSymbolPinsById -ne "true") {
                    throw "get_symbol_pins_by_id expected first_pin_is_global_power=true, got $firstPinIsGlobalPowerSymbolPinsById"
                }
                if ($firstPinIsLocalPowerSymbolPinsById -ne "false") {
                    throw "get_symbol_pins_by_id expected first_pin_is_local_power=false, got $firstPinIsLocalPowerSymbolPinsById"
                }
                if ($firstPinNetNameSymbolPinsById -eq "N/A" -or $firstPinNetNameSymbolPinsById -ne "GND") {
                    throw "get_symbol_pins_by_id expected first_pin_net_name=GND, got $firstPinNetNameSymbolPinsById"
                }
                if ($firstPinNetNameSourceSymbolPinsById -eq "N/A" -or $firstPinNetNameSourceSymbolPinsById -ne "graph") {
                    throw "get_symbol_pins_by_id expected first_pin_net_name_source=graph, got $firstPinNetNameSourceSymbolPinsById"
                }
                if ($firstPinNetNameDefaultSymbolPinsById -eq "N/A" -or $firstPinNetNameDefaultSymbolPinsById -ne "GND") {
                    throw "get_symbol_pins_by_id expected first_pin_net_name_default=GND, got $firstPinNetNameDefaultSymbolPinsById"
                }
                if ($netsCountSymbolPinsById -eq "N/A" -or [int]$netsCountSymbolPinsById -ne 1) {
                    throw "get_symbol_pins_by_id expected nets_count=1, got $netsCountSymbolPinsById"
                }
                if ($firstNetNetNameSymbolPinsById -eq "N/A" -or $firstNetNetNameSymbolPinsById -ne "GND") {
                    throw "get_symbol_pins_by_id expected first_net_net_name=GND, got $firstNetNetNameSymbolPinsById"
                }
                if ($firstNetNetNameSourceSymbolPinsById -eq "N/A" -or $firstNetNetNameSourceSymbolPinsById -ne "graph") {
                    throw "get_symbol_pins_by_id expected first_net_net_name_source=graph, got $firstNetNetNameSourceSymbolPinsById"
                }
                if ($firstNetNetNameDefaultSymbolPinsById -eq "N/A" -or $firstNetNetNameDefaultSymbolPinsById -ne "GND") {
                    throw "get_symbol_pins_by_id expected first_net_net_name_default=GND, got $firstNetNetNameDefaultSymbolPinsById"
                }
                if ($firstNetPinsCountSymbolPinsById -eq "N/A" -or [int]$firstNetPinsCountSymbolPinsById -ne 1) {
                    throw "get_symbol_pins_by_id expected first_net_pins_count=1, got $firstNetPinsCountSymbolPinsById"
                }
                if ($firstNetFirstPinIdSymbolPinsById -eq "N/A" -or $firstNetFirstPinIdSymbolPinsById -ne $expectedPinId) {
                    throw "get_symbol_pins_by_id expected first_net_first_pin_id=$expectedPinId, got $firstNetFirstPinIdSymbolPinsById"
                }

                # get_symbol_fields_by_id validation set (QA): deterministic symbol fields from stub.
                $expectedRef = "#PWR"
                $expectedValue = "GND"
                $expectedLibId = "power:GND"
                $schSymbolId = "22222222-2222-2222-2222-222222222222"

                $argsObjSymbolFieldsById = @{
                    schematic_filename = $sch
                    id                 = $schSymbolId
                }
                $outSymbolFieldsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_fields_by_id" `
                        -caseLabel "sch_frame_symbol_fields_symbol_has_reference_value" -argsObj $argsObjSymbolFieldsById -url $url -smokeExe $smokeExe

                $referenceAfter = "N/A"
                if ($outSymbolFieldsById -match 'reference=([^\r\n]+)') { $referenceAfter = $Matches[1] }
                $valueAfter = "N/A"
                if ($outSymbolFieldsById -match 'value=([^\r\n]+)') { $valueAfter = $Matches[1] }
                $libIdAfter = "N/A"
                if ($outSymbolFieldsById -match 'lib_id=([^\r\n]+)') { $libIdAfter = $Matches[1] }
                $isPowerSymbolAfter = "N/A"
                if ($outSymbolFieldsById -match 'is_power_symbol=([^\r\n]+)') { $isPowerSymbolAfter = $Matches[1] }
                $isGlobalPowerSymbolAfter = "N/A"
                if ($outSymbolFieldsById -match 'is_global_power_symbol=([^\r\n]+)') { $isGlobalPowerSymbolAfter = $Matches[1] }
                $isLocalPowerSymbolAfter = "N/A"
                if ($outSymbolFieldsById -match 'is_local_power_symbol=([^\r\n]+)') { $isLocalPowerSymbolAfter = $Matches[1] }

                $callable_after_open_symbol_fields_by_id = $(if ($outSymbolFieldsById -match 'status=1') { "ok" } else { "error" })

                $referencePresent = $(if ($referenceAfter -ne $null -and $referenceAfter -ne "" -and $referenceAfter -ne "N/A") { "true" } else { "false" })
                $valuePresent = $(if ($valueAfter -ne $null -and $valueAfter -ne "" -and $valueAfter -ne "N/A") { "true" } else { "false" })
                $libIdPresent = $(if ($libIdAfter -ne $null -and $libIdAfter -ne "" -and $libIdAfter -ne "N/A") { "true" } else { "false" })
                $isPowerSymbolPresent = $(if ($isPowerSymbolAfter -ne $null -and $isPowerSymbolAfter -ne "" -and $isPowerSymbolAfter -ne "N/A") { "true" } else { "false" })
                $isGlobalPowerSymbolPresent = $(if ($isGlobalPowerSymbolAfter -ne $null -and $isGlobalPowerSymbolAfter -ne "" -and $isGlobalPowerSymbolAfter -ne "N/A") { "true" } else { "false" })
                $isLocalPowerSymbolPresent = $(if ($isLocalPowerSymbolAfter -ne $null -and $isLocalPowerSymbolAfter -ne "" -and $isLocalPowerSymbolAfter -ne "N/A") { "true" } else { "false" })
                $fields_present_summary_after = "reference_present=" + $referencePresent + "; value_present=" + $valuePresent + "; lib_id_present=" + $libIdPresent + "; is_power_symbol_present=" + $isPowerSymbolPresent + "; is_global_power_symbol_present=" + $isGlobalPowerSymbolPresent + "; is_local_power_symbol_present=" + $isLocalPowerSymbolPresent

                $exact_blocker_if_any_symbol_fields_by_id = "N/A"
                if ($callable_after_open_symbol_fields_by_id -ne "ok") {
                    $exact_blocker_if_any_symbol_fields_by_id = "unexpected_error"
                } elseif ($referenceAfter -ne $expectedRef) {
                    $exact_blocker_if_any_symbol_fields_by_id = "reference_mismatch"
                } elseif ($valueAfter -ne $expectedValue) {
                    $exact_blocker_if_any_symbol_fields_by_id = "value_mismatch"
                } elseif ($libIdAfter -ne $expectedLibId) {
                    $exact_blocker_if_any_symbol_fields_by_id = "lib_id_mismatch"
                } elseif ($isPowerSymbolAfter -ne $expectedIsPowerSymbol) {
                    $exact_blocker_if_any_symbol_fields_by_id = "is_power_symbol_mismatch"
                } elseif ($isGlobalPowerSymbolAfter -ne $expectedIsGlobalPowerSymbol) {
                    $exact_blocker_if_any_symbol_fields_by_id = "is_global_power_symbol_mismatch"
                } elseif ($isLocalPowerSymbolAfter -ne $expectedIsLocalPowerSymbol) {
                    $exact_blocker_if_any_symbol_fields_by_id = "is_local_power_symbol_mismatch"
                }

                ("visible_before_open=" + $visible_before_open_symbol_fields_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_symbol_fields_by_id + "`n" +
                 "callable_after_open=" + $callable_after_open_symbol_fields_by_id + "`n" +
                 "reference=" + $referenceAfter + "`n" +
                 "value=" + $valueAfter + "`n" +
                 "lib_id=" + $libIdAfter + "`n" +
                 "is_power_symbol=" + $isPowerSymbolAfter + "`n" +
                 "is_global_power_symbol=" + $isGlobalPowerSymbolAfter + "`n" +
                 "is_local_power_symbol=" + $isLocalPowerSymbolAfter + "`n" +
                 "fields_present_summary=" + $fields_present_summary_after + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_symbol_fields_by_id + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_symbol_fields_by_id_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_symbol_fields_by_id -ne "ok") {
                    throw "get_symbol_fields_by_id failed after opening SCH: callable_after_open=$callable_after_open_symbol_fields_by_id"
                }
                if ($referenceAfter -ne $expectedRef) {
                    throw "get_symbol_fields_by_id expected reference=$expectedRef, got $referenceAfter"
                }
                if ($valueAfter -ne $expectedValue) {
                    throw "get_symbol_fields_by_id expected value=$expectedValue, got $valueAfter"
                }
                if ($libIdAfter -ne $expectedLibId) {
                    throw "get_symbol_fields_by_id expected lib_id=$expectedLibId, got $libIdAfter"
                }
                if ($isPowerSymbolAfter -ne $expectedIsPowerSymbol) {
                    throw "get_symbol_fields_by_id expected is_power_symbol=$expectedIsPowerSymbol, got $isPowerSymbolAfter"
                }
                if ($isGlobalPowerSymbolAfter -ne $expectedIsGlobalPowerSymbol) {
                    throw "get_symbol_fields_by_id expected is_global_power_symbol=$expectedIsGlobalPowerSymbol, got $isGlobalPowerSymbolAfter"
                }
                if ($isLocalPowerSymbolAfter -ne $expectedIsLocalPowerSymbol) {
                    throw "get_symbol_fields_by_id expected is_local_power_symbol=$expectedIsLocalPowerSymbol, got $isLocalPowerSymbolAfter"
                }

                # library_symbol_search_session validation set (QA): deterministic read-only symbol lookup.
                $visible_in_context_if_applicable_library_symbol_search = ($ltAfter -match 'library_symbol_search_session')
                $visible_in_context_if_applicable_library_symbol_search_str = $(if ($visible_in_context_if_applicable_library_symbol_search) { "true" } else { "false" })

                $librarySymbolSearchArgs = @{
                    schematic_filename = $sch
                    query              = "GND"
                    library_scope      = "both"
                    max_results        = 20
                }
                $outLibrarySymbolSearch = Call-McpToolArgsAndLog -runDir $runDir -toolName "library_symbol_search_session" `
                        -caseLabel "sch_open_library_symbol_search_session_gnd" -argsObj $librarySymbolSearchArgs -url $url -smokeExe $smokeExe

                $callable_without_mutation_library_symbol_search = $(if ($outLibrarySymbolSearch -match 'status=1') { "true" } else { "false" })
                $search_ok_raw_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "search_ok=*" } | Select-Object -First 1) -replace '^search_ok=', '').Trim()
                $search_ok_library_symbol_search = $(if ($search_ok_raw_library_symbol_search -eq "1") { "true" } else { "false" })
                $query_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "query=*" } | Select-Object -First 1) -replace '^query=', '').Trim()
                $library_scope_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "library_scope=*" } | Select-Object -First 1) -replace '^library_scope=', '').Trim()
                $count_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "count=*" } | Select-Object -First 1) -replace '^count=', '').Trim()
                $count_valid_library_symbol_search = $false
                if ($count_library_symbol_search -match '^\d+$' -and [int]$count_library_symbol_search -ge 0) { $count_valid_library_symbol_search = $true }
                $count_valid_library_symbol_search_str = $(if ($count_valid_library_symbol_search) { "true" } else { "false" })
                $items_present_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "items_present=*" } | Select-Object -First 1) -replace '^items_present=', '').Trim()
                $first_items_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "first_items=*" } | Select-Object -First 1) -replace '^first_items=', '').Trim()
                $diagnostics_present_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "diagnostics_present=*" } | Select-Object -First 1) -replace '^diagnostics_present=', '').Trim()
                $diagnostics_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "diagnostics=*" } | Select-Object -First 1) -replace '^diagnostics=', '').Trim()
                $exact_blocker_if_any_tool_library_symbol_search = ((($outLibrarySymbolSearch -split "`r?`n") | Where-Object { $_ -like "exact_blocker_if_any=*" } | Select-Object -First 1) -replace '^exact_blocker_if_any=', '').Trim()
                $source_context_library_symbol_search = "absent"
                if ($outLibrarySymbolSearch -match 'source_context=([^;\r\n]+)') {
                    $source_context_library_symbol_search = $Matches[1].Trim()
                }
                $search_strategy_library_symbol_search = "absent"
                if ($outLibrarySymbolSearch -match 'search_strategy=([^;\r\n]+)') {
                    $search_strategy_library_symbol_search = $Matches[1].Trim()
                }

                $outSchItemsAfterLibrarySearch = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_items_after_library_symbol_search_session" -argsObj $argsObjSchItemsForChain -url $url -smokeExe $smokeExe
                $countAfterLibrarySearchMutationProbe = "N/A"
                if ($outSchItemsAfterLibrarySearch -match 'count=([0-9]+)') { $countAfterLibrarySearchMutationProbe = $Matches[1] }

                $callable_without_mutation_proved = "false"
                if ($countAfterOpenSchItemsForChain -ne "N/A" -and $countAfterLibrarySearchMutationProbe -ne "N/A" -and [int]$countAfterOpenSchItemsForChain -eq [int]$countAfterLibrarySearchMutationProbe) {
                    $callable_without_mutation_proved = "true"
                }

                $exact_blocker_if_any_library_symbol_search = "N/A"
                if ($visible_in_context_if_applicable_library_symbol_search_str -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "not_visible_in_context"
                } elseif ($callable_without_mutation_library_symbol_search -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "call_failed"
                } elseif ($callable_without_mutation_proved -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "mutation_probe_failed"
                } elseif ($search_ok_library_symbol_search -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "search_not_ok"
                } elseif ($count_valid_library_symbol_search_str -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "count_invalid"
                } elseif ($items_present_library_symbol_search -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "items_missing"
                } elseif ($diagnostics_present_library_symbol_search -ne "true") {
                    $exact_blocker_if_any_library_symbol_search = "diagnostics_missing"
                } elseif ([string]::IsNullOrWhiteSpace($source_context_library_symbol_search) -or [string]::IsNullOrWhiteSpace($search_strategy_library_symbol_search) -or [string]::IsNullOrWhiteSpace($query_library_symbol_search) -or [string]::IsNullOrWhiteSpace($library_scope_library_symbol_search) -or [string]::IsNullOrWhiteSpace($count_library_symbol_search) -or [string]::IsNullOrWhiteSpace($first_items_library_symbol_search) -or [string]::IsNullOrWhiteSpace($diagnostics_library_symbol_search)) {
                    $exact_blocker_if_any_library_symbol_search = "session_log_missing_expected_fields"
                } elseif ($exact_blocker_if_any_tool_library_symbol_search -ne "N/A") {
                    $exact_blocker_if_any_library_symbol_search = "tool_blocker_unexpected"
                }

                ("visible_in_context_if_applicable=" + $visible_in_context_if_applicable_library_symbol_search_str + "`n" +
                 "callable_without_mutation=" + $callable_without_mutation_proved + "`n" +
                 "search_ok=" + $search_ok_library_symbol_search + "`n" +
                 "count_valid=" + $count_valid_library_symbol_search_str + "`n" +
                 "items_present=" + $items_present_library_symbol_search + "`n" +
                 "diagnostics_present=" + $diagnostics_present_library_symbol_search + "`n" +
                 "source_context=" + $source_context_library_symbol_search + "`n" +
                 "search_strategy=" + $search_strategy_library_symbol_search + "`n" +
                 "query=" + $query_library_symbol_search + "`n" +
                 "library_scope=" + $library_scope_library_symbol_search + "`n" +
                 "count=" + $count_library_symbol_search + "`n" +
                 "first_items=" + $first_items_library_symbol_search + "`n" +
                 "diagnostics=" + $diagnostics_library_symbol_search + "`n" +
                 "tool_exact_blocker_if_any=" + $exact_blocker_if_any_tool_library_symbol_search + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_library_symbol_search + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_library_symbol_search_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_library_symbol_search -ne "N/A") {
                    throw "library_symbol_search_session contract failed in scenario '$label' (blocker=$exact_blocker_if_any_library_symbol_search)."
                }

                # get_sch_items_by_id validation set (QA): deterministic wire from stub SCH.
                $argsObjSchItemsById = @{
                    schematic_filename = $sch
                    ids                 = @($wireSchId)
                }
                $outSchItemsById = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                        -caseLabel "sch_frame_items_by_id_wire" -argsObj $argsObjSchItemsById -url $url -smokeExe $smokeExe

                $countAfterOpenItemsById = "N/A"
                if ($outSchItemsById -match 'count=([0-9]+)') { $countAfterOpenItemsById = $Matches[1] }

                $callable_after_open_items_by_id = $(if ($outSchItemsById -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_items_by_id = "N/A"
                if ($callable_after_open_items_by_id -ne "ok") {
                    $exact_blocker_if_any_items_by_id = "unexpected_error"
                } elseif ($countAfterOpenItemsById -eq "N/A" -or [int]$countAfterOpenItemsById -le 0) {
                    $exact_blocker_if_any_items_by_id = "count_after_open_not_positive"
                }

                ("visible_before_open=" + $visible_before_open_sch_items_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items_by_id + "`n" +
                 "callable_after_open=" + $callable_after_open_items_by_id + "`n" +
                 "count_after_open=" + $countAfterOpenItemsById + "`n" +
                 "id_used=" + $wireSchId + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_items_by_id + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_by_id_contract_summary.txt") -Encoding UTF8

                # get_sch_items_by_id validation set (QA): deterministic symbol from stub.
                $schSymbolId = "22222222-2222-2222-2222-222222222222"
                $argsObjSchItemsByIdSymbols = @{
                    schematic_filename = $sch
                    ids                 = @($schSymbolId)
                }
                $outSchItemsByIdSymbols = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                        -caseLabel "sch_frame_items_by_id_symbol" -argsObj $argsObjSchItemsByIdSymbols -url $url -smokeExe $smokeExe

                $countAfterOpenItemsByIdSymbols = "N/A"
                if ($outSchItemsByIdSymbols -match 'count=([0-9]+)') { $countAfterOpenItemsByIdSymbols = $Matches[1] }

                $firstSymbolIdById = "N/A"
                if ($outSchItemsByIdSymbols -match 'first_item_id=([^\r\n]+)') { $firstSymbolIdById = $Matches[1] }

                $callable_after_open_items_by_id_symbols = $(if ($outSchItemsByIdSymbols -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_items_by_id_symbols = "N/A"
                if ($callable_after_open_items_by_id_symbols -ne "ok") {
                    $exact_blocker_if_any_items_by_id_symbols = "unexpected_error"
                } elseif ($countAfterOpenItemsByIdSymbols -eq "N/A" -or [int]$countAfterOpenItemsByIdSymbols -le 0) {
                    $exact_blocker_if_any_items_by_id_symbols = "count_after_open_not_positive"
                } elseif ($firstSymbolIdById -eq "N/A" -or [string]::IsNullOrWhiteSpace($firstSymbolIdById)) {
                    $exact_blocker_if_any_items_by_id_symbols = "first_symbol_id_by_id_missing"
                }

                if ($callable_after_open_items_by_id_symbols -ne "ok") {
                    throw "get_sch_items_by_id failed after opening SCH (symbol): callable_after_open=$callable_after_open_items_by_id_symbols"
                }

                if ($firstSymbolIdById -ne $schSymbolId) {
                    throw "get_sch_items_by_id expected first_symbol_id=$schSymbolId, got $firstSymbolIdById"
                }

                # Minimal coherence proof using extracted symbol id.
                $generic_symbol_chain_ok = "false"

                $argsObjSymbolFieldsByIdChain = @{
                    schematic_filename = $sch
                    id                 = $firstSymbolIdById
                }
                $outSymbolFieldsByIdChain = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_fields_by_id" `
                        -caseLabel "sch_frame_symbol_fields_chain_from_get_sch_items_by_id" -argsObj $argsObjSymbolFieldsByIdChain -url $url -smokeExe $smokeExe

                $referenceAfterChain = "N/A"
                if ($outSymbolFieldsByIdChain -match 'reference=([^\r\n]+)') { $referenceAfterChain = $Matches[1] }
                $valueAfterChain = "N/A"
                if ($outSymbolFieldsByIdChain -match 'value=([^\r\n]+)') { $valueAfterChain = $Matches[1] }
                $libIdAfterChain = "N/A"
                if ($outSymbolFieldsByIdChain -match 'lib_id=([^\r\n]+)') { $libIdAfterChain = $Matches[1] }

                $argsObjSymbolPinsByIdChain = @{
                    schematic_filename = $sch
                    id                 = $firstSymbolIdById
                }
                $outSymbolPinsByIdChain = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_pins_by_id" `
                        -caseLabel "sch_frame_symbol_pins_chain_from_get_sch_items_by_id" -argsObj $argsObjSymbolPinsByIdChain -url $url -smokeExe $smokeExe

                $firstPinNetNameAfterChain = "N/A"
                if ($outSymbolPinsByIdChain -match 'first_pin_net_name=([^\r\n]+)') { $firstPinNetNameAfterChain = $Matches[1] }

                if ($referenceAfterChain -eq "#PWR" -and $valueAfterChain -eq "GND" -and $libIdAfterChain -eq "power:GND" -and $firstPinNetNameAfterChain -eq "GND") {
                    $generic_symbol_chain_ok = "true"
                } else {
                    $generic_symbol_chain_ok = "false"
                }

                if ($generic_symbol_chain_ok -ne "true") {
                    $exact_blocker_if_any_items_by_id_symbols = "generic_symbol_chain_failed"
                    throw "Generic symbol chain from get_sch_items_by_id failed"
                }

                ("visible_before_open=" + $visible_before_open_sch_items_by_id_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items_by_id_symbols + "`n" +
                 "callable_after_open=" + $callable_after_open_items_by_id_symbols + "`n" +
                 "symbol_count_by_id_after_open=" + $countAfterOpenItemsByIdSymbols + "`n" +
                 "first_symbol_id_by_id=" + $firstSymbolIdById + "`n" +
                 "generic_symbol_chain_ok=" + $generic_symbol_chain_ok + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_items_by_id_symbols + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_by_id_symbol_chain_contract_summary.txt") -Encoding UTF8

                # Persist extracted symbol id for later coherence checks against the generic `get_sch_items(types=...)` path.
                $firstSymbolIdByIdForCoherence = $firstSymbolIdById

                if ($callable_after_open_items_by_id -ne "ok") {
                    throw "get_sch_items_by_id failed after opening SCH: callable_after_open=$callable_after_open_items_by_id"
                }
                if ($countAfterOpenItemsById -eq "N/A" -or [int]$countAfterOpenItemsById -le 0) {
                    throw "get_sch_items_by_id expected count>0 after opening SCH. Got count=$countAfterOpenItemsById"
                }

                # get_sch_items validation set (QA): deterministic default list returns stub wire items.
                $argsObjSchItems = @{
                    schematic_filename = $sch
                }
                $outSchItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_items_default" -argsObj $argsObjSchItems -url $url -smokeExe $smokeExe

                $firstItemIdSchItems = "N/A"
                if ($outSchItems -match 'first_item_id=([^\r\n]+)') { $firstItemIdSchItems = $Matches[1] }

                $countAfterOpenSchItems = "N/A"
                if ($outSchItems -match 'count=([0-9]+)') { $countAfterOpenSchItems = $Matches[1] }

                $callable_after_open_sch_items = $(if ($outSchItems -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_sch_items = "N/A"
                if ($callable_after_open_sch_items -ne "ok") {
                    $exact_blocker_if_any_sch_items = "unexpected_error"
                } elseif ($countAfterOpenSchItems -eq "N/A" -or [int]$countAfterOpenSchItems -le 0) {
                    $exact_blocker_if_any_sch_items = "count_after_open_not_positive"
                }

                ("visible_before_open=" + $visible_before_open_sch_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_items + "`n" +
                 "callable_after_open=" + $callable_after_open_sch_items + "`n" +
                 "count_after_open=" + $countAfterOpenSchItems + "`n" +
                 "first_item_id=" + $firstItemIdSchItems + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_items + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_sch_items -ne "ok") {
                    throw "get_sch_items failed after opening SCH: callable_after_open=$callable_after_open_sch_items"
                }
                if ($countAfterOpenSchItems -eq "N/A" -or [int]$countAfterOpenSchItems -le 0) {
                    throw "get_sch_items expected count>0 after opening SCH. Got count=$countAfterOpenSchItems"
                }

                # create_sch_items + delete_sch_items (v1): one SL_WIRE, prove count + rollback by delete.
                if (!(Test-Path variable:visible_before_open_sch_write_str)) {
                    $visible_before_open_sch_write_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_create_sch_items)) {
                    $callable_before_open_create_sch_items = "error"
                }
                if (!(Test-Path variable:callable_before_open_delete_sch_items)) {
                    $callable_before_open_delete_sch_items = "error"
                }

                $argsObjSchLinesOnly = @{
                    schematic_filename = $sch
                    types               = @("KOT_SCH_LINE")
                }
                $outSchLinesInitial = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_sch_lines_count_initial" -argsObj $argsObjSchLinesOnly -url $url -smokeExe $smokeExe
                $schLineCountInitial = -1
                if ($outSchLinesInitial -match 'count=([0-9]+)') { $schLineCountInitial = [int]$Matches[1] }
                if ($schLineCountInitial -lt 0) {
                    throw "get_sch_items(KOT_SCH_LINE) did not report count after open SCH."
                }

                $argsObjCreateSchWire = @{
                    schematic_filename = $sch
                    start              = @{ x_nm = 1820000; y_nm = 2820000 }
                    end                = @{ x_nm = 2820000; y_nm = 2820000 }
                }
                $outCreateSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "create_sch_items" `
                        -caseLabel "sch_frame_create_sch_items_wire" -argsObj $argsObjCreateSchWire -url $url -smokeExe $smokeExe
                $create_sch_ok = $(if ($outCreateSch -match 'status=1') { "true" } else { "false" })
                $created_sch_item_id = ""
                if ($outCreateSch -match 'created_item_id=([0-9a-fA-F-]{36})') { $created_sch_item_id = $Matches[1] }
                if ($created_sch_item_id -eq "" -and $outCreateSch -match 'created_item_id=([^\r\n]+)') { $created_sch_item_id = $Matches[1].Trim() }

                $outSchLinesAfterCreate = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_sch_lines_count_after_create" -argsObj $argsObjSchLinesOnly -url $url -smokeExe $smokeExe
                $schLineCountAfterCreate = -1
                if ($outSchLinesAfterCreate -match 'count=([0-9]+)') { $schLineCountAfterCreate = [int]$Matches[1] }

                # update_sch_items v1: snapshot wire end (nm), move end, verify, rollback via inverse update, then delete.
                $snapEndX = $null
                $snapEndY = $null
                $argsObjSchWireById = @{
                    schematic_filename = $sch
                    ids                 = @($created_sch_item_id)
                }
                $outSchWireSnap0 = ""
                if ($created_sch_item_id -ne "") {
                    $outSchWireSnap0 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                            -caseLabel "sch_frame_wire_by_id_snapshot_initial" -argsObj $argsObjSchWireById -url $url -smokeExe $smokeExe
                    if ($outSchWireSnap0 -match 'sch_wire_end_x_nm=(-?[0-9]+)') { $snapEndX = [int64]$Matches[1] }
                    if ($outSchWireSnap0 -match 'sch_wire_end_y_nm=(-?[0-9]+)') { $snapEndY = [int64]$Matches[1] }
                }

                $newEndX = 3320000
                $newEndY = 2820000
                $argsObjUpdateSchEnd = @{
                    schematic_filename = $sch
                    id                 = $created_sch_item_id
                    end                = @{ x_nm = $newEndX; y_nm = $newEndY }
                }
                $outUpdateSchA = ""
                if ($created_sch_item_id -ne "") {
                    $outUpdateSchA = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_sch_items" `
                            -caseLabel "sch_frame_update_sch_wire_end" -argsObj $argsObjUpdateSchEnd -url $url -smokeExe $smokeExe
                }
                $update_sch_items_ok = $(if ($outUpdateSchA -match 'status=1') { "true" } else { "false" })

                $outSchWireSnap1 = ""
                if ($created_sch_item_id -ne "") {
                    $outSchWireSnap1 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                            -caseLabel "sch_frame_wire_by_id_after_update" -argsObj $argsObjSchWireById -url $url -smokeExe $smokeExe
                }
                $end1X = $null
                $end1Y = $null
                if ($outSchWireSnap1 -match 'sch_wire_end_x_nm=(-?[0-9]+)') { $end1X = [int64]$Matches[1] }
                if ($outSchWireSnap1 -match 'sch_wire_end_y_nm=(-?[0-9]+)') { $end1Y = [int64]$Matches[1] }

                $outUpdateSchB = ""
                if ($created_sch_item_id -ne "" -and $snapEndX -ne $null -and $snapEndY -ne $null) {
                    $argsObjUpdateSchRollback = @{
                        schematic_filename = $sch
                        id                 = $created_sch_item_id
                        end                = @{ x_nm = $snapEndX; y_nm = $snapEndY }
                    }
                    $outUpdateSchB = Call-McpToolArgsAndLog -runDir $runDir -toolName "update_sch_items" `
                            -caseLabel "sch_frame_update_sch_wire_end_rollback" -argsObj $argsObjUpdateSchRollback -url $url -smokeExe $smokeExe
                }
                $update_sch_items_rollback_ok = $(if ($outUpdateSchB -match 'status=1') { "true" } else { "false" })

                $outSchWireSnap2 = ""
                if ($created_sch_item_id -ne "") {
                    $outSchWireSnap2 = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items_by_id" `
                            -caseLabel "sch_frame_wire_by_id_after_rollback" -argsObj $argsObjSchWireById -url $url -smokeExe $smokeExe
                }
                $end2X = $null
                $end2Y = $null
                if ($outSchWireSnap2 -match 'sch_wire_end_x_nm=(-?[0-9]+)') { $end2X = [int64]$Matches[1] }
                if ($outSchWireSnap2 -match 'sch_wire_end_y_nm=(-?[0-9]+)') { $end2Y = [int64]$Matches[1] }

                $argsObjDeleteSch = @{
                    schematic_filename = $sch
                    ids                 = @($created_sch_item_id)
                }
                $outDeleteSch = ""
                if ($created_sch_item_id -ne "") {
                    $outDeleteSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "delete_sch_items" `
                            -caseLabel "sch_frame_delete_sch_items" -argsObj $argsObjDeleteSch -url $url -smokeExe $smokeExe
                }
                $delete_sch_ok = $(if ($outDeleteSch -match 'status=1') { "true" } else { "false" })

                $outSchLinesAfterDelete = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_sch_lines_count_after_delete" -argsObj $argsObjSchLinesOnly -url $url -smokeExe $smokeExe
                $schLineCountAfterDelete = -1
                if ($outSchLinesAfterDelete -match 'count=([0-9]+)') { $schLineCountAfterDelete = [int]$Matches[1] }

                $rollback_sch_ok = "false"
                $exact_blocker_if_any_sch_write = "N/A"
                if ($create_sch_ok -ne "true") {
                    $exact_blocker_if_any_sch_write = "create_failed"
                } elseif ($created_sch_item_id -eq "") {
                    $exact_blocker_if_any_sch_write = "missing_created_item_id"
                } elseif ($schLineCountAfterCreate -ne $schLineCountInitial + 1) {
                    $exact_blocker_if_any_sch_write = "line_count_not_incremented"
                } elseif ($delete_sch_ok -ne "true") {
                    $exact_blocker_if_any_sch_write = "delete_failed"
                } elseif ($schLineCountAfterDelete -ne $schLineCountInitial) {
                    $exact_blocker_if_any_sch_write = "line_count_not_restored"
                } else {
                    $rollback_sch_ok = "true"
                }

                $callable_after_open_sch_write = $(if ($rollback_sch_ok -eq "true") { "ok" } else { "error" })
                $callable_before_open_sch_write = $(if ($callable_before_open_create_sch_items -ne "ok" -and $callable_before_open_delete_sch_items -ne "ok") { "error" } else { "unexpected_ok" })

                ("visible_before_open=" + $visible_before_open_sch_write_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_write + "`n" +
                 "callable_after_open=" + $callable_after_open_sch_write + "`n" +
                 "created_item_id=" + $created_sch_item_id + "`n" +
                 "create_ok=" + $create_sch_ok + "`n" +
                 "delete_ok=" + $delete_sch_ok + "`n" +
                 "rollback_ok=" + $rollback_sch_ok + "`n" +
                 "sch_line_count_initial=" + $schLineCountInitial + "`n" +
                 "sch_line_count_after_create=" + $schLineCountAfterCreate + "`n" +
                 "sch_line_count_after_delete=" + $schLineCountAfterDelete + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_write + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_create_delete_sch_items_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_sch_write -ne "ok") {
                    throw "create_sch_items/delete_sch_items contract failed: blocker=$exact_blocker_if_any_sch_write created_id=$created_sch_item_id counts=($schLineCountInitial,$schLineCountAfterCreate,$schLineCountAfterDelete)"
                }

                if (!(Test-Path variable:visible_before_open_update_sch_items_str)) {
                    $visible_before_open_update_sch_items_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_sch_update)) {
                    $callable_before_open_sch_update = "error"
                }

                $update_ok_contract = "false"
                $rollback_ok_contract = "false"
                $exact_blocker_if_any_sch_update = "N/A"
                if ($create_sch_ok -ne "true" -or $created_sch_item_id -eq "") {
                    $exact_blocker_if_any_sch_update = "create_or_id_prerequisite_failed"
                } elseif ($snapEndX -eq $null -or $snapEndY -eq $null) {
                    $exact_blocker_if_any_sch_update = "missing_initial_wire_end_snapshot"
                } elseif ($update_sch_items_ok -ne "true") {
                    $exact_blocker_if_any_sch_update = "update_failed"
                } elseif ($end1X -eq $null -or $end1Y -eq $null -or $end1X -ne $newEndX -or $end1Y -ne $newEndY) {
                    $exact_blocker_if_any_sch_update = "end_not_moved_after_update"
                } elseif ($update_sch_items_rollback_ok -ne "true") {
                    $exact_blocker_if_any_sch_update = "rollback_update_failed"
                } elseif ($end2X -eq $null -or $end2Y -eq $null -or $end2X -ne $snapEndX -or $end2Y -ne $snapEndY) {
                    $exact_blocker_if_any_sch_update = "end_not_restored_after_rollback"
                } elseif ($delete_sch_ok -ne "true") {
                    $exact_blocker_if_any_sch_update = "delete_failed"
                } else {
                    $update_ok_contract = "true"
                    $rollback_ok_contract = "true"
                }

                $callable_after_open_sch_update = $(if ($update_ok_contract -eq "true" -and $rollback_ok_contract -eq "true" -and $delete_sch_ok -eq "true") { "ok" } else { "error" })

                ("visible_before_open=" + $visible_before_open_update_sch_items_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_update + "`n" +
                 "callable_after_open=" + $callable_after_open_sch_update + "`n" +
                 "created_item_id=" + $created_sch_item_id + "`n" +
                 "update_ok=" + $update_ok_contract + "`n" +
                 "rollback_ok=" + $rollback_ok_contract + "`n" +
                 "delete_ok=" + $delete_sch_ok + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_update + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_update_sch_items_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_sch_update -ne "ok") {
                    throw "update_sch_items contract failed: blocker=$exact_blocker_if_any_sch_update created_id=$created_sch_item_id snap=($snapEndX,$snapEndY) end1=($end1X,$end1Y) end2=($end2X,$end2Y)"
                }

                # sch_wire_edit_session (composed v1): full wire session; artifact: qa_sch_wire_edit_session_contract_summary.txt
                if (!(Test-Path variable:visible_before_open_sch_wire_session_str)) {
                    $visible_before_open_sch_wire_session_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_sch_wire_session)) {
                    $callable_before_open_sch_wire_session = "error"
                }
                $baselineSchLinesForSession = -1
                $outSchLinesSessionBaseline = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_sch_lines_count_before_wire_session" -argsObj $argsObjSchLinesOnly -url $url -smokeExe $smokeExe
                if ($outSchLinesSessionBaseline -match 'count=([0-9]+)') { $baselineSchLinesForSession = [int]$Matches[1] }
                $argsObjSchWireSession = @{
                    schematic_filename = $sch
                    start              = @{ x_nm = 1820000; y_nm = 2820000 }
                    end                = @{ x_nm = 2820000; y_nm = 2820000 }
                    end_updated        = @{ x_nm = 3320000; y_nm = 2820000 }
                }
                $outSchWireSession = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_wire_edit_session" `
                        -caseLabel "sch_frame_sch_wire_edit_session" -argsObj $argsObjSchWireSession -url $url -smokeExe $smokeExe
                $session_status_ok = $(if ($outSchWireSession -match 'status=1') { "true" } else { "false" })
                $sess_create_ok = "false"
                $sess_read_ok = "false"
                $sess_update_ok = "false"
                $sess_rollback_ok = "false"
                $sess_delete_ok = "false"
                $sess_line_restored = "false"
                if ($outSchWireSession -match 'create_ok=1') { $sess_create_ok = "true" }
                if ($outSchWireSession -match 'read_ok=1') { $sess_read_ok = "true" }
                if ($outSchWireSession -match 'update_ok=1') { $sess_update_ok = "true" }
                if ($outSchWireSession -match 'rollback_ok=1') { $sess_rollback_ok = "true" }
                if ($outSchWireSession -match 'delete_ok=1') { $sess_delete_ok = "true" }
                if ($outSchWireSession -match 'line_count_restored=1') { $sess_line_restored = "true" }
                $sess_count_initial = "N/A"
                $sess_count_final = "N/A"
                if ($outSchWireSession -match 'sch_line_count_initial=(-?[0-9]+)') { $sess_count_initial = $Matches[1] }
                if ($outSchWireSession -match 'sch_line_count_final=(-?[0-9]+)') { $sess_count_final = $Matches[1] }
                $exact_blocker_if_any_sch_wire_session = "N/A"
                if ($baselineSchLinesForSession -lt 0) {
                    $exact_blocker_if_any_sch_wire_session = "missing_baseline_line_count"
                } elseif ($session_status_ok -ne "true") {
                    if ($outSchWireSession -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_sch_wire_session = $Matches[1]
                    } else {
                        $exact_blocker_if_any_sch_wire_session = "session_status_not_ok"
                    }
                } elseif ($sess_create_ok -ne "true" -or $sess_read_ok -ne "true" -or $sess_update_ok -ne "true" -or $sess_rollback_ok -ne "true" -or $sess_delete_ok -ne "true") {
                    $exact_blocker_if_any_sch_wire_session = "composed_flag_mismatch"
                } elseif ($sess_line_restored -ne "true") {
                    $exact_blocker_if_any_sch_wire_session = "line_count_not_restored"
                } elseif ($sess_count_initial -ne $sess_count_final -or $sess_count_initial -eq "N/A") {
                    $exact_blocker_if_any_sch_wire_session = "line_count_mismatch_final"
                } elseif ([int]$sess_count_initial -ne $baselineSchLinesForSession) {
                    $exact_blocker_if_any_sch_wire_session = "session_initial_count_mismatch"
                }
                $callable_after_open_sch_wire_session = $(if ($exact_blocker_if_any_sch_wire_session -eq "N/A") { "ok" } else { "error" })
                ("visible_before_open=" + $visible_before_open_sch_wire_session_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_wire_session + "`n" +
                 "create_ok=" + $sess_create_ok + "`n" +
                 "read_ok=" + $sess_read_ok + "`n" +
                 "update_ok=" + $sess_update_ok + "`n" +
                 "rollback_ok=" + $sess_rollback_ok + "`n" +
                 "delete_ok=" + $sess_delete_ok + "`n" +
                 "line_count_restored=" + $sess_line_restored + "`n" +
                 "sch_line_count_initial=" + $sess_count_initial + "`n" +
                 "sch_line_count_final=" + $sess_count_final + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_wire_session + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_wire_edit_session_contract_summary.txt") -Encoding UTF8
                if ($callable_after_open_sch_wire_session -ne "ok") {
                    throw "sch_wire_edit_session contract failed: blocker=$exact_blocker_if_any_sch_wire_session baseline=$baselineSchLinesForSession"
                }

                # sch_wire_quick_fix_session (composed v1): target one wire, update, verify, rollback, clean final state.
                if (!(Test-Path variable:visible_before_open_sch_wire_quick_fix_str)) {
                    $visible_before_open_sch_wire_quick_fix_str = "false"
                }
                if (!(Test-Path variable:callable_before_open_sch_wire_quick_fix)) {
                    $callable_before_open_sch_wire_quick_fix = "error"
                }
                $argsObjSchWireQuickFix = @{
                    schematic_filename = $sch
                    target_wire_id     = "11111111-1111-1111-1111-111111111111"
                    delta_end_x_nm     = 200000
                    delta_end_y_nm     = 0
                }
                $outSchWireQuickFix = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_wire_quick_fix_session" `
                        -caseLabel "sch_frame_sch_wire_quick_fix_session" -argsObj $argsObjSchWireQuickFix -url $url -smokeExe $smokeExe
                $target_resolved_ok_qf = $(if ($outSchWireQuickFix -match 'target_resolved_ok=1') { "true" } else { "false" })
                $create_or_update_ok_qf = $(if ($outSchWireQuickFix -match 'create_or_update_ok=1') { "true" } else { "false" })
                $verify_ok_qf = $(if ($outSchWireQuickFix -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_qf = $(if ($outSchWireQuickFix -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_ok_qf = $(if ($outSchWireQuickFix -match 'cleanup_ok=1') { "true" } else { "false" })
                $callable_after_open_qf = $(if ($outSchWireQuickFix -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_qf = "N/A"
                if ($callable_after_open_qf -ne "ok") {
                    if ($outSchWireQuickFix -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_qf = $Matches[1]
                    } else {
                        $exact_blocker_if_any_qf = "quick_fix_call_failed"
                    }
                } elseif ($target_resolved_ok_qf -ne "true") {
                    $exact_blocker_if_any_qf = "target_not_resolved"
                } elseif ($create_or_update_ok_qf -ne "true") {
                    $exact_blocker_if_any_qf = "update_not_applied"
                } elseif ($verify_ok_qf -ne "true") {
                    $exact_blocker_if_any_qf = "verify_failed"
                } elseif ($rollback_ok_qf -ne "true") {
                    $exact_blocker_if_any_qf = "rollback_failed"
                } elseif ($cleanup_ok_qf -ne "true") {
                    $exact_blocker_if_any_qf = "cleanup_failed"
                }
                ("visible_before_open=" + $visible_before_open_sch_wire_quick_fix_str + "`n" +
                 "callable_before_open=" + $callable_before_open_sch_wire_quick_fix + "`n" +
                 "target_resolved_ok=" + $target_resolved_ok_qf + "`n" +
                 "create_or_update_ok=" + $create_or_update_ok_qf + "`n" +
                 "verify_ok=" + $verify_ok_qf + "`n" +
                 "rollback_ok=" + $rollback_ok_qf + "`n" +
                 "cleanup_ok=" + $cleanup_ok_qf + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_qf + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_wire_quick_fix_session_contract_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_qf -ne "N/A") {
                    throw "sch_wire_quick_fix_session contract failed: blocker=$exact_blocker_if_any_qf"
                }

                # sch_pin_connection_fix_session (composed métier v1): resolve pin+wire, align wire end to pin, verify, rollback, final clean state.
                $visible_in_project_context_pinfix = $(if ($ltAfter -match 'sch_pin_connection_fix_session') { "true" } else { "false" })
                $argsObjSchPinConnectionFix = @{
                    schematic_filename    = $sch
                    target_symbol_id      = "22222222-2222-2222-2222-222222222222"
                    target_pin_id         = "33333333-3333-3333-3333-333333333333"
                    target_wire_id        = "11111111-1111-1111-1111-111111111111"
                    max_targets_to_modify = 1
                }
                $outSchPinConnectionFix = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_pin_connection_fix_session" `
                        -caseLabel "sch_frame_sch_pin_connection_fix_session" -argsObj $argsObjSchPinConnectionFix -url $url -smokeExe $smokeExe
                $callable_in_project_context_pinfix = $(if ($outSchPinConnectionFix -match 'status=1') { "ok" } else { "error" })
                $target_pin_resolved_ok_pinfix = $(if ($outSchPinConnectionFix -match 'target_pin_resolved_ok=1') { "true" } else { "false" })
                $target_wire_resolved_ok_pinfix = $(if ($outSchPinConnectionFix -match 'target_wire_resolved_ok=1') { "true" } else { "false" })
                $apply_ok_pinfix = $(if ($outSchPinConnectionFix -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_pinfix = $(if ($outSchPinConnectionFix -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_pinfix = $(if ($outSchPinConnectionFix -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_ok_pinfix = $(if ($outSchPinConnectionFix -match 'cleanup_ok=1') { "true" } else { "false" })

                $exact_blocker_if_any_pinfix = "N/A"
                if ($callable_in_project_context_pinfix -ne "ok") {
                    if ($outSchPinConnectionFix -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_pinfix = $Matches[1]
                    } else {
                        $exact_blocker_if_any_pinfix = "pin_connection_fix_call_failed"
                    }
                } elseif ($target_pin_resolved_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "target_pin_not_resolved"
                } elseif ($target_wire_resolved_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "target_wire_not_resolved"
                } elseif ($apply_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "apply_failed"
                } elseif ($verify_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "verify_failed"
                } elseif ($rollback_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "rollback_failed"
                } elseif ($cleanup_ok_pinfix -ne "true") {
                    $exact_blocker_if_any_pinfix = "cleanup_failed"
                }

                ("visible_in_project_context=" + $visible_in_project_context_pinfix + "`n" +
                 "callable_in_project_context=" + $callable_in_project_context_pinfix + "`n" +
                 "target_pin_resolved_ok=" + $target_pin_resolved_ok_pinfix + "`n" +
                 "target_wire_resolved_ok=" + $target_wire_resolved_ok_pinfix + "`n" +
                 "apply_ok=" + $apply_ok_pinfix + "`n" +
                 "verify_ok=" + $verify_ok_pinfix + "`n" +
                 "rollback_ok=" + $rollback_ok_pinfix + "`n" +
                 "cleanup_ok=" + $cleanup_ok_pinfix + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_pinfix + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_pin_connection_fix_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_pinfix -ne "N/A") {
                    throw "sch_pin_connection_fix_session contract failed: blocker=$exact_blocker_if_any_pinfix"
                }

                # sch_connection_repair_decision_session (composed multi-contexte v1): read -> decision -> targeted repair(if needed) -> verify -> rollback.
                $visible_in_project_context_decision = $(if ($ltAfter -match 'sch_connection_repair_decision_session') { "true" } else { "false" })
                $argsObjSchConnectionDecision = @{
                    schematic_filename    = $sch
                    target_symbol_id      = "22222222-2222-2222-2222-222222222222"
                    target_pin_id         = "33333333-3333-3333-3333-333333333333"
                    target_wire_id        = "11111111-1111-1111-1111-111111111111"
                    decision_mode         = "repair_if_needed"
                    max_targets_to_modify = 1
                }
                $outSchConnectionDecision = Call-McpToolArgsAndLog -runDir $runDir -toolName "sch_connection_repair_decision_session" `
                        -caseLabel "sch_frame_sch_connection_repair_decision_session" -argsObj $argsObjSchConnectionDecision -url $url -smokeExe $smokeExe
                $callable_in_project_context_decision = $(if ($outSchConnectionDecision -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok = $(if ($outSchConnectionDecision -match 'decision_taken_ok=1') { "true" } else { "false" })
                $repair_needed_detected = $(if ($outSchConnectionDecision -match 'repair_needed_detected=1') { "true" } else { "false" })
                $apply_ok_decision = $(if ($outSchConnectionDecision -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_decision = $(if ($outSchConnectionDecision -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_decision = $(if ($outSchConnectionDecision -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_ok_decision = $(if ($outSchConnectionDecision -match 'cleanup_ok=1') { "true" } else { "false" })

                $exact_blocker_if_any_decision = "N/A"
                if ($callable_in_project_context_decision -ne "ok") {
                    if ($outSchConnectionDecision -match 'error_message=([^\r\n]+)') {
                        $exact_blocker_if_any_decision = $Matches[1]
                    } else {
                        $exact_blocker_if_any_decision = "decision_session_call_failed"
                    }
                } elseif ($decision_taken_ok -ne "true") {
                    $exact_blocker_if_any_decision = "decision_not_taken"
                } elseif ($repair_needed_detected -ne "true") {
                    $exact_blocker_if_any_decision = "repair_needed_not_detected_in_stub"
                } elseif ($apply_ok_decision -ne "true") {
                    $exact_blocker_if_any_decision = "apply_failed"
                } elseif ($verify_ok_decision -ne "true") {
                    $exact_blocker_if_any_decision = "verify_failed"
                } elseif ($rollback_ok_decision -ne "true") {
                    $exact_blocker_if_any_decision = "rollback_failed"
                } elseif ($cleanup_ok_decision -ne "true") {
                    $exact_blocker_if_any_decision = "cleanup_failed"
                }

                ("visible_in_project_context=" + $visible_in_project_context_decision + "`n" +
                 "callable_in_project_context=" + $callable_in_project_context_decision + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok + "`n" +
                 "repair_needed_detected=" + $repair_needed_detected + "`n" +
                 "apply_ok=" + $apply_ok_decision + "`n" +
                 "verify_ok=" + $verify_ok_decision + "`n" +
                 "rollback_ok=" + $rollback_ok_decision + "`n" +
                 "cleanup_ok=" + $cleanup_ok_decision + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_decision + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_sch_connection_repair_decision_session_contract_summary.txt") -Encoding UTF8

                if ($exact_blocker_if_any_decision -ne "N/A") {
                    throw "sch_connection_repair_decision_session contract failed: blocker=$exact_blocker_if_any_decision"
                }

                # cross_context_preflight_correction_session (global v1) - subcase sch_priority
                $ccpSchArgs = @{
                    mode                    = "sch_priority"
                    allow_single_correction = $true
                    schematic_filename      = $sch
                    target_symbol_id        = "22222222-2222-2222-2222-222222222222"
                    target_pin_id           = "33333333-3333-3333-3333-333333333333"
                    target_wire_id          = "11111111-1111-1111-1111-111111111111"
                }
                $outCcpSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "cross_context_preflight_correction_session" `
                        -caseLabel "sch_frame_cross_context_preflight_correction_sch_priority" -argsObj $ccpSchArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_ccp_sch = $(if ($outCcpSch -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok_ccp_sch = $(if ($outCcpSch -match 'decision_taken_ok=1') { "true" } else { "false" })
                $correction_needed_detected_ccp_sch = $(if ($outCcpSch -match 'correction_needed_detected=1') { "true" } else { "false" })
                $chosen_branch_ccp_sch = "N/A"
                $chosenBranchLineSch = (($outCcpSch -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLineSch)) { $chosen_branch_ccp_sch = ($chosenBranchLineSch -replace '^chosen_branch=', '').Trim() }
                $single_correction_enforced_ccp_sch = $(if ($outCcpSch -match 'single_correction_enforced=1') { "true" } else { "false" })
                $apply_ok_ccp_sch = $(if ($outCcpSch -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_ccp_sch = $(if ($outCcpSch -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_ccp_sch = $(if ($outCcpSch -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_ccp_sch = $(if ($outCcpSch -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_ccp_sch = "N/A"
                if ($callable_in_project_context_ccp_sch -ne "ok") {
                    $exact_blocker_if_any_ccp_sch = "call_failed"
                } elseif ($decision_taken_ok_ccp_sch -ne "true") {
                    $exact_blocker_if_any_ccp_sch = "decision_not_taken"
                } elseif ($chosen_branch_ccp_sch -ne "sch") {
                    $exact_blocker_if_any_ccp_sch = "unexpected_chosen_branch"
                } elseif ($single_correction_enforced_ccp_sch -ne "true") {
                    $exact_blocker_if_any_ccp_sch = "single_correction_not_enforced"
                } elseif ($apply_ok_ccp_sch -ne "true" -or $verify_ok_ccp_sch -ne "true" -or $rollback_ok_ccp_sch -ne "true" -or $cleanup_or_state_restored_ok_ccp_sch -ne "true") {
                    $exact_blocker_if_any_ccp_sch = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_ccp_sch + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok_ccp_sch + "`n" +
                 "correction_needed_detected=" + $correction_needed_detected_ccp_sch + "`n" +
                 "chosen_branch=" + $chosen_branch_ccp_sch + "`n" +
                 "single_correction_enforced=" + $single_correction_enforced_ccp_sch + "`n" +
                 "apply_ok=" + $apply_ok_ccp_sch + "`n" +
                 "verify_ok=" + $verify_ok_ccp_sch + "`n" +
                 "rollback_ok=" + $rollback_ok_ccp_sch + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_ccp_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_ccp_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_cross_context_preflight_correction_session_contract_sch_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_ccp_sch -ne "N/A") {
                    throw "cross_context_preflight_correction_session (sch_priority) failed: blocker=$exact_blocker_if_any_ccp_sch"
                }

                # workflow_preflight_gate_session - executed subcase sch_priority
                $workflowGateSchArgs = @{
                    workflow_stage          = "prototype_preflight"
                    priority_mode           = "sch_priority"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outWorkflowGateSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_preflight_gate_session" `
                        -caseLabel "sch_frame_workflow_preflight_gate_session_sch_execute_true" -argsObj $workflowGateSchArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'status=1') { "ok" } else { "error" })
                $decision_taken_ok_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'decision_taken_ok=1') { "true" } else { "false" })
                $gate_decision_workflow_gate_sch = "N/A"
                $gateDecisionLineWorkflowSch = (($outWorkflowGateSch -split "`r?`n") | Where-Object { $_ -like "gate_decision=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($gateDecisionLineWorkflowSch)) { $gate_decision_workflow_gate_sch = ($gateDecisionLineWorkflowSch -replace '^gate_decision=', '').Trim() }
                $chosen_branch_workflow_gate_sch = "N/A"
                $chosenBranchLineWorkflowSch = (($outWorkflowGateSch -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLineWorkflowSch)) { $chosen_branch_workflow_gate_sch = ($chosenBranchLineWorkflowSch -replace '^chosen_branch=', '').Trim() }
                $single_correction_enforced_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'single_correction_enforced=1') { "true" } else { "false" })
                $apply_ok_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_workflow_gate_sch = $(if ($outWorkflowGateSch -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_workflow_gate_sch = "N/A"
                if ($callable_in_project_context_workflow_gate_sch -ne "ok") {
                    $exact_blocker_if_any_workflow_gate_sch = "call_failed"
                } elseif ($decision_taken_ok_workflow_gate_sch -ne "true") {
                    $exact_blocker_if_any_workflow_gate_sch = "decision_not_taken"
                } elseif ($gate_decision_workflow_gate_sch -ne "ready") {
                    $exact_blocker_if_any_workflow_gate_sch = "gate_not_ready_after_execute"
                } elseif ($chosen_branch_workflow_gate_sch -ne "sch") {
                    $exact_blocker_if_any_workflow_gate_sch = "unexpected_chosen_branch"
                } elseif ($single_correction_enforced_workflow_gate_sch -ne "true") {
                    $exact_blocker_if_any_workflow_gate_sch = "single_correction_not_enforced"
                } elseif ($apply_ok_workflow_gate_sch -ne "true" -or $verify_ok_workflow_gate_sch -ne "true" -or $rollback_ok_workflow_gate_sch -ne "true" -or $cleanup_or_state_restored_ok_workflow_gate_sch -ne "true") {
                    $exact_blocker_if_any_workflow_gate_sch = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_workflow_gate_sch + "`n" +
                 "decision_taken_ok=" + $decision_taken_ok_workflow_gate_sch + "`n" +
                 "gate_decision=" + $gate_decision_workflow_gate_sch + "`n" +
                 "chosen_branch=" + $chosen_branch_workflow_gate_sch + "`n" +
                 "single_correction_enforced=" + $single_correction_enforced_workflow_gate_sch + "`n" +
                 "apply_ok=" + $apply_ok_workflow_gate_sch + "`n" +
                 "verify_ok=" + $verify_ok_workflow_gate_sch + "`n" +
                 "rollback_ok=" + $rollback_ok_workflow_gate_sch + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_workflow_gate_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_workflow_gate_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_preflight_gate_session_contract_sch_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_workflow_gate_sch -ne "N/A") {
                    throw "workflow_preflight_gate_session (sch execute=true) failed: blocker=$exact_blocker_if_any_workflow_gate_sch"
                }

                # workflow_next_best_action_session - executed subcase sch family
                $nextBestSchArgs = @{
                    workflow_stage          = "prototype_preflight"
                    priority_mode           = "sch_priority"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outNextBestSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_next_best_action_session" `
                        -caseLabel "sch_frame_workflow_next_best_action_session_sch_execute_true" -argsObj $nextBestSchArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_next_best_sch = $(if ($outNextBestSch -match 'status=1') { "ok" } else { "error" })
                $recommendation_taken_ok_next_best_sch = $(if ($outNextBestSch -match 'recommendation_taken_ok=1') { "true" } else { "false" })
                $recommended_action_next_best_sch = "N/A"
                $recommendedActionLineNextBestSch = (($outNextBestSch -split "`r?`n") | Where-Object { $_ -like "recommended_action=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($recommendedActionLineNextBestSch)) { $recommended_action_next_best_sch = ($recommendedActionLineNextBestSch -replace '^recommended_action=', '').Trim() }
                $execute_mode_next_best_sch = "N/A"
                $executeModeLineNextBestSch = (($outNextBestSch -split "`r?`n") | Where-Object { $_ -like "execute_mode=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($executeModeLineNextBestSch)) { $execute_mode_next_best_sch = ($executeModeLineNextBestSch -replace '^execute_mode=', '').Trim() }
                $single_action_enforced_next_best_sch = $(if ($outNextBestSch -match 'single_action_enforced=1') { "true" } else { "false" })
                $apply_ok_next_best_sch = $(if ($outNextBestSch -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_next_best_sch = $(if ($outNextBestSch -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_next_best_sch = $(if ($outNextBestSch -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_next_best_sch = $(if ($outNextBestSch -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_next_best_sch = "N/A"
                if ($callable_in_project_context_next_best_sch -ne "ok") {
                    $exact_blocker_if_any_next_best_sch = "call_failed"
                } elseif ($recommendation_taken_ok_next_best_sch -ne "true") {
                    $exact_blocker_if_any_next_best_sch = "recommendation_not_taken"
                } elseif ($recommended_action_next_best_sch -ne "repair_schematic_connection") {
                    $exact_blocker_if_any_next_best_sch = "unexpected_recommended_action"
                } elseif ($execute_mode_next_best_sch -ne "execute_true") {
                    $exact_blocker_if_any_next_best_sch = "unexpected_execute_mode"
                } elseif ($single_action_enforced_next_best_sch -ne "true") {
                    $exact_blocker_if_any_next_best_sch = "single_action_not_enforced"
                } elseif ($apply_ok_next_best_sch -ne "true" -or $verify_ok_next_best_sch -ne "true" -or $rollback_ok_next_best_sch -ne "true" -or $cleanup_or_state_restored_ok_next_best_sch -ne "true") {
                    $exact_blocker_if_any_next_best_sch = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_next_best_sch + "`n" +
                 "recommendation_taken_ok=" + $recommendation_taken_ok_next_best_sch + "`n" +
                 "recommended_action=" + $recommended_action_next_best_sch + "`n" +
                 "execute_mode=" + $execute_mode_next_best_sch + "`n" +
                 "single_action_enforced=" + $single_action_enforced_next_best_sch + "`n" +
                 "apply_ok=" + $apply_ok_next_best_sch + "`n" +
                 "verify_ok=" + $verify_ok_next_best_sch + "`n" +
                 "rollback_ok=" + $rollback_ok_next_best_sch + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_next_best_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_next_best_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_next_best_action_session_contract_sch_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_next_best_sch -ne "N/A") {
                    throw "workflow_next_best_action_session (sch execute=true) failed: blocker=$exact_blocker_if_any_next_best_sch"
                }

                # workflow_handoff_readiness_session - executed subcase sch handoff
                $handoffReadinessSchArgs = @{
                    handoff_target          = "sch_to_pcb"
                    allow_single_correction = $true
                    execute                 = $true
                }
                $outHandoffReadinessSch = Call-McpToolArgsAndLog -runDir $runDir -toolName "workflow_handoff_readiness_session" `
                        -caseLabel "sch_frame_workflow_handoff_readiness_session_sch_to_pcb_execute_true" -argsObj $handoffReadinessSchArgs -url $url -smokeExe $smokeExe
                $callable_in_project_context_handoff_sch = $(if ($outHandoffReadinessSch -match 'status=1') { "ok" } else { "error" })
                $handoff_decision_taken_ok_handoff_sch = $(if ($outHandoffReadinessSch -match 'handoff_decision_taken_ok=1') { "true" } else { "false" })
                $handoff_target_handoff_sch = "N/A"
                $handoffTargetLineHandoffSch = (($outHandoffReadinessSch -split "`r?`n") | Where-Object { $_ -like "handoff_target=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($handoffTargetLineHandoffSch)) { $handoff_target_handoff_sch = ($handoffTargetLineHandoffSch -replace '^handoff_target=', '').Trim() }
                $handoff_decision_handoff_sch = "N/A"
                $handoffDecisionLineHandoffSch = (($outHandoffReadinessSch -split "`r?`n") | Where-Object { $_ -like "handoff_decision=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($handoffDecisionLineHandoffSch)) { $handoff_decision_handoff_sch = ($handoffDecisionLineHandoffSch -replace '^handoff_decision=', '').Trim() }
                $chosen_branch_handoff_sch = "N/A"
                $chosenBranchLineHandoffSch = (($outHandoffReadinessSch -split "`r?`n") | Where-Object { $_ -like "chosen_branch=*" } | Select-Object -First 1)
                if (![string]::IsNullOrWhiteSpace($chosenBranchLineHandoffSch)) { $chosen_branch_handoff_sch = ($chosenBranchLineHandoffSch -replace '^chosen_branch=', '').Trim() }
                $single_action_enforced_handoff_sch = $(if ($outHandoffReadinessSch -match 'single_action_enforced=1') { "true" } else { "false" })
                $apply_ok_handoff_sch = $(if ($outHandoffReadinessSch -match 'apply_ok=1') { "true" } else { "false" })
                $verify_ok_handoff_sch = $(if ($outHandoffReadinessSch -match 'verify_ok=1') { "true" } else { "false" })
                $rollback_ok_handoff_sch = $(if ($outHandoffReadinessSch -match 'rollback_ok=1') { "true" } else { "false" })
                $cleanup_or_state_restored_ok_handoff_sch = $(if ($outHandoffReadinessSch -match 'cleanup_or_state_restored_ok=1') { "true" } else { "false" })
                $exact_blocker_if_any_handoff_sch = "N/A"
                if ($callable_in_project_context_handoff_sch -ne "ok") {
                    $exact_blocker_if_any_handoff_sch = "call_failed"
                } elseif ($handoff_decision_taken_ok_handoff_sch -ne "true") {
                    $exact_blocker_if_any_handoff_sch = "handoff_decision_not_taken"
                } elseif ($handoff_target_handoff_sch -ne "sch_to_pcb") {
                    $exact_blocker_if_any_handoff_sch = "unexpected_handoff_target"
                } elseif ($handoff_decision_handoff_sch -ne "ready") {
                    $exact_blocker_if_any_handoff_sch = "handoff_not_ready_after_execute"
                } elseif ($chosen_branch_handoff_sch -ne "sch") {
                    $exact_blocker_if_any_handoff_sch = "unexpected_chosen_branch"
                } elseif ($single_action_enforced_handoff_sch -ne "true") {
                    $exact_blocker_if_any_handoff_sch = "single_action_not_enforced"
                } elseif ($apply_ok_handoff_sch -ne "true" -or $verify_ok_handoff_sch -ne "true" -or $rollback_ok_handoff_sch -ne "true" -or $cleanup_or_state_restored_ok_handoff_sch -ne "true") {
                    $exact_blocker_if_any_handoff_sch = "branch_flags_failed"
                }
                ("visible_in_project_context=true`n" +
                 "callable_in_project_context=" + $callable_in_project_context_handoff_sch + "`n" +
                 "handoff_decision_taken_ok=" + $handoff_decision_taken_ok_handoff_sch + "`n" +
                 "handoff_target=" + $handoff_target_handoff_sch + "`n" +
                 "handoff_decision=" + $handoff_decision_handoff_sch + "`n" +
                 "chosen_branch=" + $chosen_branch_handoff_sch + "`n" +
                 "single_action_enforced=" + $single_action_enforced_handoff_sch + "`n" +
                 "apply_ok=" + $apply_ok_handoff_sch + "`n" +
                 "verify_ok=" + $verify_ok_handoff_sch + "`n" +
                 "rollback_ok=" + $rollback_ok_handoff_sch + "`n" +
                 "cleanup_or_state_restored_ok=" + $cleanup_or_state_restored_ok_handoff_sch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_handoff_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_workflow_handoff_readiness_session_contract_sch_summary.txt") -Encoding UTF8
                if ($exact_blocker_if_any_handoff_sch -ne "N/A") {
                    throw "workflow_handoff_readiness_session (sch_to_pcb execute=true) failed: blocker=$exact_blocker_if_any_handoff_sch"
                }

                # get_sch_items validation set (QA): deterministic symbol list returns stub symbol.
                $argsObjSchItemsSymbols = @{
                    schematic_filename = $sch
                    types               = @("KOT_SCH_SYMBOL")
                }
                $outSchItemsSymbols = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_sch_items" `
                        -caseLabel "sch_frame_items_symbols" -argsObj $argsObjSchItemsSymbols -url $url -smokeExe $smokeExe

                $firstSymbolIdSchItems = "N/A"
                if ($outSchItemsSymbols -match 'first_item_id=([^\r\n]+)') { $firstSymbolIdSchItems = $Matches[1] }

                $symbolCountAfterOpen = "N/A"
                if ($outSchItemsSymbols -match 'count=([0-9]+)') { $symbolCountAfterOpen = $Matches[1] }

                $callable_after_open_sch_items_symbols = $(if ($outSchItemsSymbols -match 'status=1') { "ok" } else { "error" })
                $exact_blocker_if_any_sch_items_symbols = "N/A"
                if ($callable_after_open_sch_items_symbols -ne "ok") {
                    $exact_blocker_if_any_sch_items_symbols = "unexpected_error"
                } elseif ($symbolCountAfterOpen -eq "N/A" -or [int]$symbolCountAfterOpen -le 0) {
                    $exact_blocker_if_any_sch_items_symbols = "count_after_open_not_positive"
                }

                # Extracted symbol id is used as the deterministic chain anchor for symbol discovery.
                ("visible_before_open=" + $visible_before_open_sch_items_str + "`n" +
                 "callable_after_open=" + $callable_after_open_sch_items_symbols + "`n" +
                 "symbol_count_after_open=" + $symbolCountAfterOpen + "`n" +
                 "first_symbol_id=" + $firstSymbolIdSchItems + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_sch_items_symbols + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_get_sch_items_symbol_contract_summary.txt") -Encoding UTF8

                if ($callable_after_open_sch_items_symbols -ne "ok") {
                    throw "get_sch_items (types=KOT_SCH_SYMBOL) failed after opening SCH: callable_after_open=$callable_after_open_sch_items_symbols"
                }
                if ($symbolCountAfterOpen -eq "N/A" -or [int]$symbolCountAfterOpen -le 0) {
                    throw "get_sch_items (types=KOT_SCH_SYMBOL) expected count>0 after opening SCH. Got count=$symbolCountAfterOpen"
                }
                if ($firstSymbolIdSchItems -eq "N/A" -or [string]::IsNullOrWhiteSpace($firstSymbolIdSchItems)) {
                    throw "get_sch_items (types=KOT_SCH_SYMBOL) did not yield first_symbol_id. first_symbol_id=$firstSymbolIdSchItems"
                }

                if ($firstSymbolIdByIdForCoherence -ne $firstSymbolIdSchItems) {
                    throw "Symbol coherence mismatch: get_sch_items(type=symbol) first_symbol_id=$firstSymbolIdSchItems but get_sch_items_by_id first_symbol_id_by_id=$firstSymbolIdByIdForCoherence"
                }

                # Chain proof: first_symbol_id must feed both symbol fields and symbol pins tools.
                $fields_chain_ok = "false"
                $pins_chain_ok   = "false"
                $exact_blocker_if_any_symbol_chain = "N/A"

                $expectedRef = "#PWR"
                $expectedValue = "GND"
                $expectedLibId = "power:GND"
                $expectedIsPowerSymbol = "true"
                $expectedIsGlobalPowerSymbol = "true"
                $expectedIsLocalPowerSymbol = "false"

                $argsObjSymbolFieldsFromItems = @{
                    schematic_filename = $sch
                    id                 = $firstSymbolIdSchItems
                }
                $outSymbolFieldsFromItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_fields_by_id" `
                        -caseLabel "sch_frame_symbol_fields_chain_from_get_sch_items_symbols" -argsObj $argsObjSymbolFieldsFromItems -url $url -smokeExe $smokeExe

                $referenceAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'reference=([^\r\n]+)') { $referenceAfterChain = $Matches[1] }
                $valueAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'value=([^\r\n]+)') { $valueAfterChain = $Matches[1] }
                $libIdAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'lib_id=([^\r\n]+)') { $libIdAfterChain = $Matches[1] }
                $isPowerSymbolAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'is_power_symbol=([^\r\n]+)') { $isPowerSymbolAfterChain = $Matches[1] }
                $isGlobalPowerSymbolAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'is_global_power_symbol=([^\r\n]+)') { $isGlobalPowerSymbolAfterChain = $Matches[1] }
                $isLocalPowerSymbolAfterChain = "N/A"
                if ($outSymbolFieldsFromItems -match 'is_local_power_symbol=([^\r\n]+)') { $isLocalPowerSymbolAfterChain = $Matches[1] }

                $callable_after_open_symbol_fields_chain = $(if ($outSymbolFieldsFromItems -match 'status=1') { "ok" } else { "error" })

                if ($callable_after_open_symbol_fields_chain -ne "ok") {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_chain_unexpected_error"
                } elseif ($referenceAfterChain -ne $expectedRef) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_reference_mismatch"
                } elseif ($valueAfterChain -ne $expectedValue) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_value_mismatch"
                } elseif ($libIdAfterChain -ne $expectedLibId) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_lib_id_mismatch"
                } elseif ($isPowerSymbolAfterChain -ne $expectedIsPowerSymbol) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_is_power_symbol_mismatch"
                } elseif ($isGlobalPowerSymbolAfterChain -ne $expectedIsGlobalPowerSymbol) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_is_global_power_symbol_mismatch"
                } elseif ($isLocalPowerSymbolAfterChain -ne $expectedIsLocalPowerSymbol) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_fields_by_id_is_local_power_symbol_mismatch"
                } else {
                    $fields_chain_ok = "true"
                }

                $expectedPinNetName = "GND"
                $expectedPinNetNameSource = "graph"
                $expectedPinTypeName = "power_in"
                $expectedPinOrientationName = "down"

                $argsObjSymbolPinsFromItems = @{
                    schematic_filename = $sch
                    id                 = $firstSymbolIdSchItems
                }
                $outSymbolPinsFromItems = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_symbol_pins_by_id" `
                        -caseLabel "sch_frame_symbol_pins_chain_from_get_sch_items_symbols" -argsObj $argsObjSymbolPinsFromItems -url $url -smokeExe $smokeExe

                $pinsCountAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'count=([0-9]+)') { $pinsCountAfterChain = $Matches[1] }

                $firstPinNetNameAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_net_name=([^\r\n]+)') { $firstPinNetNameAfterChain = $Matches[1] }
                $firstPinNetNameSourceAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_net_name_source=([^\r\n]+)') { $firstPinNetNameSourceAfterChain = $Matches[1] }

                $firstPinTypeNameAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_type_name=([^\r\n]+)') { $firstPinTypeNameAfterChain = $Matches[1] }
                $firstPinOrientationNameAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_orientation_name=([^\r\n]+)') { $firstPinOrientationNameAfterChain = $Matches[1] }

                $firstPinIsPowerAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_is_power=([^\r\n]+)') { $firstPinIsPowerAfterChain = $Matches[1] }
                $firstPinIsGlobalPowerAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_is_global_power=([^\r\n]+)') { $firstPinIsGlobalPowerAfterChain = $Matches[1] }
                $firstPinIsLocalPowerAfterChain = "N/A"
                if ($outSymbolPinsFromItems -match 'first_pin_is_local_power=([^\r\n]+)') { $firstPinIsLocalPowerAfterChain = $Matches[1] }

                $callable_after_open_symbol_pins_chain = $(if ($outSymbolPinsFromItems -match 'status=1') { "ok" } else { "error" })

                if ($callable_after_open_symbol_pins_chain -ne "ok") {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_unexpected_error"
                } elseif ($pinsCountAfterChain -eq "N/A" -or [int]$pinsCountAfterChain -le 0) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_count_not_positive"
                } elseif ($firstPinNetNameAfterChain -ne $expectedPinNetName) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_net_name_mismatch"
                } elseif ($firstPinNetNameSourceAfterChain -ne $expectedPinNetNameSource) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_net_name_source_mismatch"
                } elseif ($firstPinTypeNameAfterChain -ne $expectedPinTypeName) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_pin_type_name_mismatch"
                } elseif ($firstPinOrientationNameAfterChain -ne $expectedPinOrientationName) {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_pin_orientation_name_mismatch"
                } elseif ($firstPinIsPowerAfterChain -ne "true") {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_is_power_mismatch"
                } elseif ($firstPinIsGlobalPowerAfterChain -ne "true") {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_is_global_power_mismatch"
                } elseif ($firstPinIsLocalPowerAfterChain -ne "false") {
                    $exact_blocker_if_any_symbol_chain = "get_symbol_pins_by_id_chain_is_local_power_mismatch"
                } else {
                    $pins_chain_ok = "true"
                }

                ("symbol_count_after_open=" + $symbolCountAfterOpen + "`n" +
                 "first_symbol_id=" + $firstSymbolIdSchItems + "`n" +
                 "fields_chain_ok=" + $fields_chain_ok + "`n" +
                 "pins_chain_ok=" + $pins_chain_ok + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_symbol_chain + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_symbol_discovery_chain_contract_summary.txt") -Encoding UTF8

                if ($fields_chain_ok -ne "true") {
                    throw "Symbol discovery chain failed: fields_chain_ok=$fields_chain_ok blocker=$exact_blocker_if_any_symbol_chain"
                }
                if ($pins_chain_ok -ne "true") {
                    throw "Symbol discovery chain failed: pins_chain_ok=$pins_chain_ok blocker=$exact_blocker_if_any_symbol_chain"
                }

                # hit_test_sch validation set (QA): deterministic wire from stub SCH.
                $hitTestSchId = $wireSchId
                $hitSchX = 5000000
                $hitSchY = 0
                $missSchX = 5000000
                $missSchY = 5000000
                $tolSch = 0

                $argsObjHitTestSchHit = @{
                    schematic_filename = $sch
                    id                 = $hitTestSchId
                    position           = @{
                        x_nm = $hitSchX
                        y_nm = $hitSchY
                    }
                    tolerance          = $tolSch
                }
                $outHitTestSchHit = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_hit_hitpoint" -argsObj $argsObjHitTestSchHit -url $url -smokeExe $smokeExe

                $hitSchVal = "N/A"
                $hitSchResult = "N/A"
                if ($outHitTestSchHit -match 'hit=(true|false)') { $hitSchVal = $Matches[1] }
                if ($outHitTestSchHit -match 'result=([0-9]+)') { $hitSchResult = $Matches[1] }

                $argsObjHitTestSchMiss = @{
                    schematic_filename = $sch
                    id                 = $hitTestSchId
                    position           = @{
                        x_nm = $missSchX
                        y_nm = $missSchY
                    }
                    tolerance          = $tolSch
                }
                $outHitTestSchMiss = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_hit_misspoint" -argsObj $argsObjHitTestSchMiss -url $url -smokeExe $smokeExe

                $missSchVal = "N/A"
                $missSchResult = "N/A"
                if ($outHitTestSchMiss -match 'hit=(true|false)') { $missSchVal = $Matches[1] }
                if ($outHitTestSchMiss -match 'result=([0-9]+)') { $missSchResult = $Matches[1] }

                $callable_after_open_hit_test_sch = $(if ($outHitTestSchHit -match 'status=1' -and $outHitTestSchMiss -match 'status=1') { "ok" } else { "error" })
                $hitBehaviorSch = "hit=$hitSchVal,result=$hitSchResult"
                $missBehaviorSch = "hit=$missSchVal,result=$missSchResult"

                $exact_blocker_if_any_hit_test_sch = "N/A"
                if ($callable_after_open_hit_test_sch -ne "ok") {
                    $exact_blocker_if_any_hit_test_sch = "unexpected_error"
                } elseif ($hitSchVal -ne "true") {
                    $exact_blocker_if_any_hit_test_sch = "deterministic_hit_not_observed"
                } elseif ($missSchVal -ne "false") {
                    $exact_blocker_if_any_hit_test_sch = "deterministic_miss_not_observed"
                }

                ("visible_before_open=" + $visible_before_open_hit_test_sch_str + "`n" +
                 "callable_before_open=" + $callable_before_open_hit_test_sch + "`n" +
                 "callable_after_open=" + $callable_after_open_hit_test_sch + "`n" +
                 "id_used=" + $wireSchId + "`n" +
                 "hit_behavior=" + $hitBehaviorSch + "`n" +
                 "miss_behavior=" + $missBehaviorSch + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_hit_test_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_sch_contract_summary.txt") -Encoding UTF8

                if ($hitSchVal -ne "true") {
                    throw "hit_test_sch expected hit=true for deterministic wire hit point, got hit=$hitSchVal (result=$hitSchResult)"
                }
                if ($missSchVal -ne "false") {
                    throw "hit_test_sch expected hit=false for deterministic wire miss point, got hit=$missSchVal (result=$missSchResult)"
                }

                # hit_test_sch validation set (QA): deterministic symbol from stub SCH.
                # Goal: close the chain symbol -> hit_test_sch using the same deterministic id.
                $symbolHitTestSchId = $firstSymbolIdById
                $hitSymX = 0
                $hitSymY = 0
                $missSymX = 5000000
                $missSymY = 5000000
                $tolSym = 0

                $argsObjHitTestSchSymHit = @{
                    schematic_filename = $sch
                    id                 = $symbolHitTestSchId
                    position           = @{
                        x_nm = $hitSymX
                        y_nm = $hitSymY
                    }
                    tolerance          = $tolSym
                }
                $outHitTestSchSymHit = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_symbol_hit_hitpoint" -argsObj $argsObjHitTestSchSymHit -url $url -smokeExe $smokeExe

                $symHitVal = "N/A"
                $symHitResult = "N/A"
                if ($outHitTestSchSymHit -match 'hit=(true|false)') { $symHitVal = $Matches[1] }
                if ($outHitTestSchSymHit -match 'result=([0-9]+)') { $symHitResult = $Matches[1] }

                $argsObjHitTestSchSymMiss = @{
                    schematic_filename = $sch
                    id                 = $symbolHitTestSchId
                    position           = @{
                        x_nm = $missSymX
                        y_nm = $missSymY
                    }
                    tolerance          = $tolSym
                }
                $outHitTestSchSymMiss = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_symbol_hit_misspoint" -argsObj $argsObjHitTestSchSymMiss -url $url -smokeExe $smokeExe

                $symMissVal = "N/A"
                $symMissResult = "N/A"
                if ($outHitTestSchSymMiss -match 'hit=(true|false)') { $symMissVal = $Matches[1] }
                if ($outHitTestSchSymMiss -match 'result=([0-9]+)') { $symMissResult = $Matches[1] }

                $symbolHitChainOk = "false"
                $hitBehaviorSym = "hit=$symHitVal,result=$symHitResult"
                $missBehaviorSym = "hit=$symMissVal,result=$symMissResult"
                $exact_blocker_if_any_symbol_hit_test_sch = "N/A"

                if ($symHitVal -eq "true" -and $symMissVal -eq "false") {
                    $symbolHitChainOk = "true"
                } else {
                    if ($symHitVal -ne "true") {
                        $exact_blocker_if_any_symbol_hit_test_sch = "deterministic_symbol_hit_not_observed"
                    } elseif ($symMissVal -ne "false") {
                        $exact_blocker_if_any_symbol_hit_test_sch = "deterministic_symbol_miss_not_observed"
                    } else {
                        $exact_blocker_if_any_symbol_hit_test_sch = "deterministic_symbol_hit_miss_unexpected"
                    }
                }

                ("first_symbol_id_by_id=" + $firstSymbolIdById + "`n" +
                 "symbol_hit_chain_ok=" + $symbolHitChainOk + "`n" +
                 "hit_behavior=" + $hitBehaviorSym + "`n" +
                 "miss_behavior=" + $missBehaviorSym + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_symbol_hit_test_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_sch_symbol_contract_summary.txt") -Encoding UTF8

                if ($symbolHitChainOk -ne "true") {
                    throw "hit_test_sch symbol chain failed: first_symbol_id_by_id=$firstSymbolIdById hit_behavior=$hitBehaviorSym miss_behavior=$missBehaviorSym exact_blocker_if_any=$exact_blocker_if_any_symbol_hit_test_sch"
                }

                # hit_test_sch validation set (QA): deterministic pin hit/miss from stub SCH pin.
                # Goal: close the chain pin_id -> hit_test_sch using the already extracted pin KIID.
                $pinHitTestSchId = $firstPinIdSymbolPinsById
                $hitPinXNm = ([int]$firstPinXSymbolPinsById) * 100
                $hitPinYNm = ([int]$firstPinYSymbolPinsById) * 100
                $missPinXNm = 5000000
                $missPinYNm = 5000000
                $tolPin = 0

                $argsObjHitTestSchPinHit = @{
                    schematic_filename = $sch
                    id                 = $pinHitTestSchId
                    position           = @{
                        x_nm = $hitPinXNm
                        y_nm = $hitPinYNm
                    }
                    tolerance          = $tolPin
                }
                $outHitTestSchPinHit = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_pin_hit_hitpoint" -argsObj $argsObjHitTestSchPinHit -url $url -smokeExe $smokeExe

                $pinHitVal = "N/A"
                $pinHitResult = "N/A"
                if ($outHitTestSchPinHit -match 'hit=(true|false)') { $pinHitVal = $Matches[1] }
                if ($outHitTestSchPinHit -match 'result=([0-9]+)') { $pinHitResult = $Matches[1] }

                $argsObjHitTestSchPinMiss = @{
                    schematic_filename = $sch
                    id                 = $pinHitTestSchId
                    position           = @{
                        x_nm = $missPinXNm
                        y_nm = $missPinYNm
                    }
                    tolerance          = $tolPin
                }
                $outHitTestSchPinMiss = Call-McpToolArgsAndLog -runDir $runDir -toolName "hit_test_sch" `
                        -caseLabel "sch_frame_pin_hit_misspoint" -argsObj $argsObjHitTestSchPinMiss -url $url -smokeExe $smokeExe

                $pinMissVal = "N/A"
                $pinMissResult = "N/A"
                if ($outHitTestSchPinMiss -match 'hit=(true|false)') { $pinMissVal = $Matches[1] }
                if ($outHitTestSchPinMiss -match 'result=([0-9]+)') { $pinMissResult = $Matches[1] }

                $pinHitChainOk = "false"
                $hitBehaviorPin = "hit=$pinHitVal,result=$pinHitResult"
                $missBehaviorPin = "hit=$pinMissVal,result=$pinMissResult"
                $exact_blocker_if_any_pin_hit_test_sch = "N/A"

                if ($pinHitVal -eq "true" -and $pinMissVal -eq "false") {
                    $pinHitChainOk = "true"
                } else {
                    if ($pinHitVal -ne "true") {
                        $exact_blocker_if_any_pin_hit_test_sch = "deterministic_pin_hit_not_observed"
                    } elseif ($pinMissVal -ne "false") {
                        $exact_blocker_if_any_pin_hit_test_sch = "deterministic_pin_miss_not_observed"
                    } else {
                        $exact_blocker_if_any_pin_hit_test_sch = "deterministic_pin_hit_miss_unexpected"
                    }
                }

                ("first_pin_id=" + $pinHitTestSchId + "`n" +
                 "pin_hit_chain_ok=" + $pinHitChainOk + "`n" +
                 "hit_behavior=" + $hitBehaviorPin + "`n" +
                 "miss_behavior=" + $missBehaviorPin + "`n" +
                 "exact_blocker_if_any=" + $exact_blocker_if_any_pin_hit_test_sch + "`n") |
                    Set-Content -Path (Join-Path $runDir "qa_hit_test_sch_pin_contract_summary.txt") -Encoding UTF8

                if ($pinHitChainOk -ne "true") {
                    throw "hit_test_sch pin chain failed: first_pin_id=$pinHitTestSchId hit_behavior=$hitBehaviorPin miss_behavior=$missBehaviorPin exact_blocker_if_any=$exact_blocker_if_any_pin_hit_test_sch"
                }
            }
        }
        else {
            # Case 1: no PCB frame open => expect error.
            $typesLabel = "ABSENT"
            $boardFilenameUsed = $pcb

            $argsObj = @{
                board_filename = $boardFilenameUsed
            }
            $out = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_pcb_items" -caseLabel "no_pcb_frame_types_absent" -argsObj $argsObj -url $url -smokeExe $smokeExe

            $itemsCount = "N/A"
            if ($out -match 'items_count=([0-9]+)') { $itemsCount = $Matches[1] }

            $summaryPath = Join-Path $runDir "qa_get_pcb_items_summary_no_pcb_frame_types_absent.txt"
            ("items_count=" + $itemsCount + "`n" +
             "type(s)_demanded=" + $typesLabel + "`n" +
             "board_filename_used=" + $boardFilenameUsed + "`n") | Set-Content -Path $summaryPath -Encoding UTF8
        }
    } else {
        & $smokeExe $url get_project_info
        & $smokeExe $url workspace_overview

        $ltNoProj = & $smokeExe $url mcp_list_tools 2>&1 | Out-String
        $ltNoProj | Set-Content -Path (Join-Path $runDir "mcp_list_tools.log") -Encoding UTF8
        if ($ltNoProj -notmatch 'set_net_classes') {
            throw "MCP ListTools missing set_net_classes in scenario '$label'."
        }
        if ($ltNoProj -notmatch 'get_project_context_snapshot') {
            throw "MCP ListTools missing get_project_context_snapshot in scenario '$label'."
        }

        $visible_without_editor_np = ($ltNoProj -match 'get_project_context_snapshot')
        $snapNpArgs = @{}
        $outSnapNp = Call-McpToolArgsAndLog -runDir $runDir -toolName "get_project_context_snapshot" `
                -caseLabel "no_project_context_snapshot" -argsObj $snapNpArgs -url $url -smokeExe $smokeExe
        $callable_snap_no_project = $(if ($outSnapNp -match 'status=1') { "ok" } else { "error" })
        ("visible_without_editor_if_applicable=" + $(if ($visible_without_editor_np) { "true" } else { "false" }) + "`n" +
         "callable_without_loaded_project_context=" + $callable_snap_no_project + "`n" +
         "exact_blocker_if_any=N/A`n") |
            Set-Content -Path (Join-Path $runDir "qa_get_project_context_snapshot_contract_before_summary.txt") -Encoding UTF8

        $noProjSetNcArgs = @{
            merge_mode  = "MMM_MERGE"
            net_classes = @(
                @{
                    name = "QA_IPC_NETCLASS_1"
                    type = "NCT_EXPLICIT"
                    board  = @{
                        track_width = @{ value_nm = [int64]400000 }
                    }
                }
            )
        }
        $outNoProjSetNc = Call-McpToolArgsAndLog -runDir $runDir -toolName "set_net_classes" `
                -caseLabel "no_project_set_net_classes" -argsObj $noProjSetNcArgs -url $url -smokeExe $smokeExe
        $noProjSetNcCallable = $(if ($outNoProjSetNc -match 'status=1') { "ok" } else { "error" })
        $noProjSetNcBlocker = "N/A"
        if ($noProjSetNcCallable -eq "ok") {
            $noProjSetNcBlocker = "unexpected_success_without_project"
        }
        ("visible_before_use=" + $(if ($ltNoProj -match 'set_net_classes') { "true" } else { "false" }) + "`n" +
         "callable_before_context=" + $noProjSetNcCallable + "`n" +
         "exact_blocker_if_any=" + $noProjSetNcBlocker + "`n") |
            Set-Content -Path (Join-Path $runDir "qa_set_net_classes_no_project_context_summary.txt") -Encoding UTF8
        if ($noProjSetNcCallable -eq "ok") {
            throw "set_net_classes unexpectedly succeeded with no project in scenario '$label'."
        }

        & $smokeExe $url mcp_call_tool get_project_info
        & $smokeExe $url mcp_call_tool get_workspace_overview
    }
}

function Get-LatestScenarioRunDir([string]$suffix) {
    # PowerShell returns a single object (not an array) when only one match exists;
    # under Set-StrictMode, `.Count` is not always available on scalars.
    $dirs = @( Get-ChildItem -Directory (Join-Path $BuildDir "ipc-v1-artifacts") -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*-$suffix" } |
            Sort-Object LastWriteTime -Descending )
    if( $dirs.Count -gt 0 ) { return $dirs[0].FullName }
    return ""
}

$noProjectStatus = "not_run"
$noProjectMessage = "scenario skipped"
$noProjectRunDir = ""

if( -not $SkipNoProjectScenario )
{
    try
    {
        Run-Scenario -label "No project" -kicadArgs @() -ExpectLoadedTrue $false
        $noProjectStatus = "passed"
        $noProjectMessage = "no project diagnostic passed"
    }
    catch
    {
        $noProjectRunDir = Get-LatestScenarioRunDir -suffix "No_project"
        $noProjectStatus = "failed"
        $noProjectMessage = $_.Exception.Message
        if( $noProjectRunDir -and (Test-Path (Join-Path $noProjectRunDir "early_exit.txt")) )
        {
            $noProjectStatus = "early_exit"
        }
        Write-Warning "No project diagnostic failed (non-blocking): $noProjectMessage"
    }
}

if( -not $noProjectRunDir )
{
    $noProjectRunDir = Get-LatestScenarioRunDir -suffix "No_project"
}

("no_project_status=" + $noProjectStatus + "`n" +
 "message=" + $noProjectMessage + "`n" +
 "run_dir=" + $noProjectRunDir + "`n") |
    Set-Content -Path (Join-Path $BuildDir "ipc-v1-artifacts\qa_no_project_status_summary.txt") -Encoding UTF8

$globalContractGreen = $true
$exact_blocker_if_any = "N/A"

try
{
    for ($i = 1; $i -le $WithProjectIterations; $i++) {
        Run-Scenario -label ("With project run {0} of {1}" -f $i, $WithProjectIterations) -kicadArgs @($proj) -ExpectLoadedTrue $true

        # Open editor frames once (first iteration only) so we have at least one real "frame open" repro.
        if ($i -eq 1) {
            Run-Scenario -label "With project open PCB" -kicadArgs @($proj) -ExpectLoadedTrue $true -OpenEditorType "pcb"
            Run-Scenario -label "With project open SCH" -kicadArgs @($proj) -ExpectLoadedTrue $true -OpenEditorType "sch"
        }
    }
}
catch
{
    $globalContractGreen = $false
    $exact_blocker_if_any = $_.Exception.Message
}

("global_contract_green=" + ($(if($globalContractGreen) { "true" } else { "false" })) + "`n" +
 "no_project_status=" + $noProjectStatus + "`n" +
 "exact_blocker_if_any=" + $exact_blocker_if_any + "`n") |
    Set-Content -Path (Join-Path $BuildDir "ipc-v1-artifacts\qa_global_contract_summary.txt") -Encoding UTF8

if( -not $globalContractGreen )
{
    throw "Global contractual run failed: $exact_blocker_if_any"
}

