Set-StrictMode -Version Latest

function Get-IpcQaNormalizedPathKey {
    <#
    .NOTES
        Windows path key for PATH deduplication (case-insensitive, slash-normalized).
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathSegment
    )

    if( [string]::IsNullOrWhiteSpace( $PathSegment ) )
    {
        return $null
    }

    $trimmed = $PathSegment.Trim().TrimEnd( '\', '/' )
    try
    {
        if( Test-Path -LiteralPath $trimmed )
        {
            return ( (Resolve-Path -LiteralPath $trimmed).Path ).ToLowerInvariant()
        }
    }
    catch
    {
        # Fall through to literal normalization when the segment does not exist yet.
    }

    return ( $trimmed -replace '/', '\' ).ToLowerInvariant()
}


function Set-IpcQaPathPrefixIdempotent {
    <#
    .NOTES
        Prepends KiCad build prefixes once. Repeated Initialize-IpcQaEnvironment calls in the
        same PowerShell session must not grow PATH (R84).
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$PrefixPaths,

        [Parameter(Mandatory = $true)]
        [string]$CurrentPath
    )

    $prefixKeys = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $orderedPrefix = New-Object System.Collections.Generic.List[string]

    foreach( $prefixPath in $PrefixPaths )
    {
        if( [string]::IsNullOrWhiteSpace( $prefixPath ) ) { continue }

        $key = Get-IpcQaNormalizedPathKey -PathSegment $prefixPath
        if( [string]::IsNullOrWhiteSpace( $key ) ) { continue }
        if( $prefixKeys.Add( $key ) )
        {
            [void]$orderedPrefix.Add( $prefixPath )
        }
    }

    $rest = New-Object System.Collections.Generic.List[string]
    $seenRest = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)

    foreach( $segment in ( $CurrentPath -split ';' ) )
    {
        if( [string]::IsNullOrWhiteSpace( $segment ) ) { continue }

        $key = Get-IpcQaNormalizedPathKey -PathSegment $segment
        if( [string]::IsNullOrWhiteSpace( $key ) ) { continue }
        if( $prefixKeys.Contains( $key ) ) { continue }
        if( $seenRest.Add( $key ) )
        {
            [void]$rest.Add( $segment )
        }
    }

    if( $rest.Count -gt 0 )
    {
        return ( ( $orderedPrefix -join ';' ) + ';' + ( $rest -join ';' ) )
    }

    return ( $orderedPrefix -join ';' )
}


function Initialize-IpcQaEnvironment {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,

        [string]$ConfigRoot,

        [switch]$EnableAutomation
    )

    $resolvedBuildDir = (Resolve-Path -LiteralPath $BuildDir).Path
    $resolvedRepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
    $cfgRoot = if( [string]::IsNullOrWhiteSpace( $ConfigRoot ) )
    {
        Join-Path $resolvedBuildDir "ipc-v1-config"
    }
    else
    {
        $ConfigRoot
    }

    $pythonHomeDir = Join-Path $resolvedBuildDir "vcpkg_installed\x64-windows\tools\python3"

    $env:KICAD_CONFIG_HOME = $cfgRoot
    $env:KICAD_RUN_FROM_BUILD_DIR = "1"
    $env:KICAD_USE_EXTERNAL_PYTHONHOME = "1"
    $env:PYTHONHOME = $pythonHomeDir

    if( $EnableAutomation )
    {
        $env:KICAD_IPC_AUTOMATION = "1"
    }

    # Order matters for DLL resolution (_pcbnew.dll, _eeschema.dll, kicommon.dll, etc.).
    $pathPrefixList = @(
        (Join-Path $resolvedBuildDir "kicad"),
        (Join-Path $resolvedBuildDir "eeschema"),
        (Join-Path $resolvedBuildDir "pcbnew"),
        (Join-Path $resolvedBuildDir "common"),
        (Join-Path $resolvedBuildDir "common\gal"),
        (Join-Path $resolvedBuildDir "api"),
        (Join-Path $resolvedBuildDir "vcpkg_installed\x64-windows\bin"),
        $pythonHomeDir
    )

    $env:PATH = Set-IpcQaPathPrefixIdempotent -PrefixPaths $pathPrefixList -CurrentPath $env:PATH

    return [pscustomobject]@{
        BuildDir = $resolvedBuildDir
        RepoRoot = $resolvedRepoRoot
        ConfigRoot = $cfgRoot
        PythonHome = $pythonHomeDir
    }
}


function Test-IpcKifaceDllLoad {
    <#
    .SYNOPSIS
        Windows smoke: LoadLibraryW on pcbnew and eeschema KIFACE DLLs using the current PATH
        (must run after Initialize-IpcQaEnvironment so run-from-build prefixes apply).

    .NOTES
        Activate only via KICAD_IPC_KIFACE_SMOKE=1 from callers (checkpoint); do not call from every probe by default.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [string]$ArtifactsDir
    )

    if( -not [string]::IsNullOrWhiteSpace( $ArtifactsDir ) )
    {
        New-Item -ItemType Directory -Force -Path $ArtifactsDir -ErrorAction SilentlyContinue | Out-Null
    }

    if( -not ( 'McpKifaceLoadNative10' -as [type] ) )
    {
        $src = @'
using System;
using System.Runtime.InteropServices;
public static class McpKifaceLoadNative10 {
    [DllImport("kernel32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern IntPtr LoadLibraryW(string lpLibFileName);
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeLibrary(IntPtr hModule);
}
'@
        Add-Type -TypeDefinition $src -ErrorAction Stop
    }
    $native = [McpKifaceLoadNative10]

    $resolvedBuild = (Resolve-Path -LiteralPath $BuildDir).Path
    $targets = @(
        @{ Label = "pcbnew"; RelPath = "pcbnew\_pcbnew.dll" },
        @{ Label = "eeschema"; RelPath = "eeschema\_eeschema.dll" }
    )

    $rows = @()
    $allOk = $true

    foreach( $t in $targets )
    {
        $full = Join-Path $resolvedBuild $t.RelPath
        if( !(Test-Path -LiteralPath $full) )
        {
            $allOk = $false
            $rows += [pscustomobject]@{
                Label = $t.Label
                Path = $full
                Ok = $false
                LastError = 0
                Detail = "file_missing"
            }
            continue
        }

        $h = $native::LoadLibraryW( $full )
        $err = [System.Runtime.InteropServices.Marshal]::GetLastWin32Error()

        if( $h -ne [IntPtr]::Zero )
        {
            [void]$native::FreeLibrary( $h )
            $rows += [pscustomobject]@{
                Label = $t.Label
                Path = $full
                Ok = $true
                LastError = 0
                Detail = ""
            }
        }
        else
        {
            $allOk = $false
            $rows += [pscustomobject]@{
                Label = $t.Label
                Path = $full
                Ok = $false
                LastError = $err
                Detail = "loadlibrary_failed"
            }
        }
    }

    if( -not $allOk -and !( [string]::IsNullOrWhiteSpace( $ArtifactsDir ) ) )
    {
        $pathDump = Join-Path $ArtifactsDir "kiface_smoke_path_env.txt"
        @(
            "captured=$([DateTimeOffset]::Now.ToString('o'))"
            "PWD=$PWD"
            "PATH=$env:PATH"
        ) | Set-Content -LiteralPath $pathDump -Encoding UTF8
    }

    return [pscustomobject]@{
        AllOk = $allOk
        Rows = $rows
        PathEnvDumpPath = if( -not $allOk -and !( [string]::IsNullOrWhiteSpace( $ArtifactsDir ) ) ) {
            (Join-Path $ArtifactsDir "kiface_smoke_path_env.txt")
        } else { "" }
    }
}


function Stop-KiCadProcesses {
    [CmdletBinding()]
    param()

    Stop-Process -Name kicad -Force -ErrorAction SilentlyContinue
    Stop-Process -Name qa_ipc_smoke -Force -ErrorAction SilentlyContinue
}


function Clear-IpcSockets {
    [CmdletBinding()]
    param()

    $sockDir = Join-Path ([System.IO.Path]::GetTempPath().TrimEnd([char]0x005C, [char]0x002F)) "kicad"
    if( Test-Path -LiteralPath $sockDir )
    {
        Get-ChildItem -LiteralPath $sockDir -Filter "api*.sock" -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
    }

    return $sockDir
}


function Format-SmokeArgumentLine {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$SmokeArgs
    )

    $parts = @()
    foreach( $a in $SmokeArgs )
    {
        if( $null -eq $a )
            { continue }

        if( $a.StartsWith( "ipc://" ) -or $a -match '[\s"]' )
        {
            $parts += '"' + ( $a.Replace( '"', '""' ) ) + '"'
        }
        else
        {
            $parts += $a
        }
    }

    return ( $parts -join ' ' )
}


function Invoke-SmokeBounded {
    <#
    .NOTES
        Starts ReadToEndAsync on stdout/stderr immediately after process start so pipes drain
        while qa_ipc_smoke runs. Synchronous ReadToEnd only after HasExited deadlocks on Windows
        when output exceeds ~4 KiB (pipe buffer fills, child blocks on write) — see R80/R81.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SmokeExePath,

        [Parameter(Mandatory = $true)]
        [string[]]$SmokeArgs,

        [int]$TimeoutMs = 20000,

        [string]$WorkingDirectory
    )

    if( !(Test-Path -LiteralPath $SmokeExePath) )
    {
        throw "qa_ipc_smoke executable not found: $SmokeExePath"
    }

    if( [string]::IsNullOrWhiteSpace( $WorkingDirectory ) )
    {
        $WorkingDirectory = Split-Path -Parent $SmokeExePath
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $SmokeExePath
    $psi.Arguments = (Format-SmokeArgumentLine -SmokeArgs $SmokeArgs)
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.StandardOutputEncoding = [System.Text.Encoding]::UTF8
    $psi.StandardErrorEncoding = [System.Text.Encoding]::UTF8
    $psi.CreateNoWindow = $true
    $psi.WorkingDirectory = $WorkingDirectory

    $proc = New-Object System.Diagnostics.Process
    $proc.StartInfo = $psi
    $proc.Start() | Out-Null

    $stdoutTask = $proc.StandardOutput.ReadToEndAsync()
    $stderrTask = $proc.StandardError.ReadToEndAsync()

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while( -not $proc.HasExited )
    {
        if( $sw.ElapsedMilliseconds -ge $TimeoutMs )
        {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            $proc.WaitForExit( 1000 ) | Out-Null
            return @{
                Exited = $false
                ExitCode = -1
                Text = "timeout"
            }
        }

        Start-Sleep -Milliseconds 25
    }

    $proc.WaitForExit()

    $drainTimeoutMs = [Math]::Max( 5000, $TimeoutMs )
    $null = $stdoutTask.Wait( $drainTimeoutMs )
    $null = $stderrTask.Wait( $drainTimeoutMs )

    $outText = ""
    $errText = ""
    try
    {
        if( $stdoutTask.IsCompleted )
        {
            $outText = $stdoutTask.Result
        }
    }
    catch
    {
        $outText = ""
    }

    try
    {
        if( $stderrTask.IsCompleted )
        {
            $errText = $stderrTask.Result
        }
    }
    catch
    {
        $errText = ""
    }

    return @{
        Exited = $true
        ExitCode = $proc.ExitCode
        Text = ( $outText + $errText )
    }
}


function Test-IpcSmokeOutputLooksLikeConnectionRefused {
    <#
    .SYNOPSIS
        Heuristic for transient IPC failures (qa_ipc_smoke nng_dial / Windows pipe) seen occasionally under automation.
    #>
    [CmdletBinding()]
    param( [string]$Text )

    if( [string]::IsNullOrWhiteSpace( $Text ) ) { return $false }
    if( $Text -match '(?i)nng_dial:\s*Connection refused' ) { return $true }
    if( $Text -match '(?i)Connection refused' ) { return $true }
    return $false
}


function Invoke-SmokeBoundedIpcRetry {
    <#
    .SYNOPSIS
        Wraps Invoke-SmokeBounded with a short bounded retry when stdout/stderr looks like Connection refused.
    .NOTES
        Does not retry on timeout or logical API errors; only on flake-shaped output (R16 harness hardening).
    #>
    [CmdletBinding()]
    param(
        [Parameter( Mandatory = $true )]
        [string]$SmokeExePath,

        [Parameter( Mandatory = $true )]
        [string[]]$SmokeArgs,

        [int]$TimeoutMs = 20000,

        [string]$WorkingDirectory,

        [int]$MaxAttempts = 3,

        [int]$RetryDelayMs = 400
    )

    if( [string]::IsNullOrWhiteSpace( $WorkingDirectory ) )
    {
        $WorkingDirectory = Split-Path -Parent $SmokeExePath
    }

    $extraAttempts = 0
    $last = $null

    for( $attempt = 1; $attempt -le $MaxAttempts; $attempt++ )
    {
        $last = Invoke-SmokeBounded -SmokeExePath $SmokeExePath -SmokeArgs $SmokeArgs -TimeoutMs $TimeoutMs `
            -WorkingDirectory $WorkingDirectory

        if( -not ( Test-IpcSmokeOutputLooksLikeConnectionRefused -Text $last.Text ) )
        {
            $last['IpcSmokeRetryExtraAttempts'] = $extraAttempts
            return $last
        }

        if( $attempt -lt $MaxAttempts )
        {
            $extraAttempts++
            Start-Sleep -Milliseconds $RetryDelayMs
        }
    }

    $last['IpcSmokeRetryExtraAttempts'] = $extraAttempts
    return $last
}


function Resolve-IpcUrlFromSockets {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SmokeExePath,

        [Parameter(Mandatory = $true)]
        [int]$KiCadPid,

        [int]$MaxIterations = 100,

        [int]$PingTimeoutMs = 1500,

        [int]$SleepBetweenIterationsMs = 250,

        [string]$WorkingDirectory
    )

    $sockDir = Join-Path ([System.IO.Path]::GetTempPath().TrimEnd([char]0x005C, [char]0x002F)) "kicad"
    $pidSock = Join-Path $sockDir ("api-" + $KiCadPid + ".sock")
    $defaultSock = Join-Path $sockDir "api.sock"

    for( $i = 0; $i -lt $MaxIterations; $i++ )
    {
        $candidateSet = New-Object 'System.Collections.Generic.HashSet[string]'
        [void]$candidateSet.Add("ipc://$pidSock")
        [void]$candidateSet.Add("ipc://$defaultSock")

        if( Test-Path -LiteralPath $sockDir )
        {
            foreach( $sock in ( Get-ChildItem -LiteralPath $sockDir -Filter "api*.sock" -ErrorAction SilentlyContinue ) )
            {
                [void]$candidateSet.Add("ipc://" + $sock.FullName)
            }
        }

        $candidates = @($candidateSet)
        foreach( $candidate in $candidates )
        {
            $probe = Invoke-SmokeBoundedIpcRetry -SmokeExePath $SmokeExePath -SmokeArgs @($candidate, "ping") `
                                         -TimeoutMs $PingTimeoutMs -WorkingDirectory $WorkingDirectory `
                                         -MaxAttempts 3 -RetryDelayMs 400
            if( $probe.Exited -and $probe.Text -match 'status=1' )
            {
                return [pscustomobject]@{
                    Resolved = $true
                    Url = $candidate
                    Iteration = $i
                    Candidates = $candidates
                }
            }
        }

        Start-Sleep -Milliseconds $SleepBetweenIterationsMs
    }

    return [pscustomobject]@{
        Resolved = $false
        Url = ""
        Iteration = $MaxIterations
        Candidates = @()
    }
}


function Wait-McpProjectLoaded {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SmokeExePath,

        [Parameter(Mandatory = $true)]
        [string]$Url,

        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory,

        [int]$MaxWaitMs = 120000,

        [int]$PollMs = 250
    )

    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while( $sw.ElapsedMilliseconds -lt $MaxWaitMs )
    {
        $res = Invoke-SmokeBoundedIpcRetry -SmokeExePath $SmokeExePath `
            -SmokeArgs @($Url, "mcp_call_tool", "get_project_info") -TimeoutMs 20000 -WorkingDirectory $WorkingDirectory `
            -MaxAttempts 3 -RetryDelayMs 400
        if( $res.Text -match 'loaded=true' )
        {
            return $true
        }
        Start-Sleep -Milliseconds $PollMs
    }
    return $false
}


function Read-ContractValue {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$SummaryPath,

        [Parameter(Mandatory = $true)]
        [string]$Key,

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


function Write-TargetRunDir {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$RunDir
    )

    Write-Output ("TARGET_RUN_DIR=" + $RunDir)
}


function Get-IpcQaChildTargetRunDir {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$Stdout,

        [string]$Stderr,

        [ValidateSet( 'StdoutOnly', 'StdoutAndStderr' )]
        [string]$Source = 'StdoutOnly'
    )

    $text = if( $Source -eq 'StdoutAndStderr' )
    {
        $Stdout + "`n" + $Stderr
    }
    else
    {
        $Stdout
    }

    if( $text -match 'TARGET_RUN_DIR=([^\r\n]+)' )
    {
        return $Matches[1].Trim()
    }

    return "absent"
}


function Invoke-IpcQaChildScript {
    <#
    .SYNOPSIS
        Exécute un script QA enfant PowerShell et capture stdout/stderr/exit/TARGET_RUN_DIR (R89).
    .OUTPUTS
        PSCustomObject with ExitCode, StdOut, StdErr, Text, TargetRunDir, StdoutLog, StderrLog, SummaryPath.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$ScriptPath,

        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [string[]]$ExtraArgs,

        [string]$LogDir,

        [string]$LogBaseName,

        [ValidateSet( 'StdoutOnly', 'StdoutAndStderr' )]
        [string]$TargetRunDirSource = 'StdoutOnly',

        [string]$SummaryFileName
    )

    if( !(Test-Path -LiteralPath $ScriptPath) )
    {
        throw "Invoke-IpcQaChildScript: missing script $ScriptPath"
    }

    $argList = "-NoProfile -ExecutionPolicy Bypass -File `"$ScriptPath`" -BuildDir `"$BuildDir`""
    if( $ExtraArgs -and $ExtraArgs.Count -gt 0 )
    {
        $argList += " " + ( $ExtraArgs -join " " )
    }

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = "powershell.exe"
    $psi.Arguments = $argList
    $psi.UseShellExecute = $false
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError = $true
    $psi.CreateNoWindow = $true
    $p = New-Object System.Diagnostics.Process
    $p.StartInfo = $psi
    $null = $p.Start()
    $stdout = $p.StandardOutput.ReadToEnd()
    $stderr = $p.StandardError.ReadToEnd()
    $p.WaitForExit()
    $exitCode = $p.ExitCode
    $text = $stdout + "`n" + $stderr

    $stdoutLog = ""
    $stderrLog = ""
    if( -not [string]::IsNullOrWhiteSpace( $LogDir ) )
    {
        New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
        $logBase = if( [string]::IsNullOrWhiteSpace( $LogBaseName ) )
        {
            [System.IO.Path]::GetFileNameWithoutExtension( $ScriptPath )
        }
        else
        {
            $LogBaseName
        }
        $stdoutLog = Join-Path $LogDir ( $logBase + ".stdout.log" )
        $stderrLog = Join-Path $LogDir ( $logBase + ".stderr.log" )
        $stdout | Set-Content -Path $stdoutLog -Encoding UTF8
        $stderr | Set-Content -Path $stderrLog -Encoding UTF8
    }

    $targetRunDir = Get-IpcQaChildTargetRunDir -Stdout $stdout -Stderr $stderr -Source $TargetRunDirSource
    $summaryPath = ""
    if( -not [string]::IsNullOrWhiteSpace( $SummaryFileName ) -and $targetRunDir -ne "absent" )
    {
        $summaryPath = Join-Path $targetRunDir $SummaryFileName
    }

    return [pscustomobject]@{
        ExitCode = $exitCode
        StdOut = $stdout
        StdErr = $stderr
        Text = $text
        TargetRunDir = $targetRunDir
        StdoutLog = $stdoutLog
        StderrLog = $stderrLog
        SummaryPath = $summaryPath
    }
}


function Invoke-IpcPathIdempotenceGuard {
    <#
    .SYNOPSIS
        Garde-fou PATH idempotent (R88) — appelle test_ipc_qa_path_idempotence.ps1 (R85).
    .OUTPUTS
        PSCustomObject with Ok, TargetRunDir, ExitCode, SummaryPath, Green.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [Parameter(Mandatory = $true)]
        [string]$IpcToolsDir,

        [string]$LogDir,

        [switch]$NoThrow
    )

    $pathIdemScript = Join-Path $IpcToolsDir "test_ipc_qa_path_idempotence.ps1"
    $child = Invoke-IpcQaChildScript -ScriptPath $pathIdemScript -BuildDir $BuildDir `
        -LogDir $LogDir -LogBaseName "test_ipc_qa_path_idempotence" `
        -TargetRunDirSource StdoutOnly `
        -SummaryFileName "qa_ipc_path_idempotence_summary.txt"
    $exitCode = $child.ExitCode
    $exitCodeStr = [string]$exitCode
    $targetRunDir = $child.TargetRunDir
    $summaryPath = $child.SummaryPath

    $green = $null
    if( $summaryPath -and (Test-Path -LiteralPath $summaryPath) )
    {
        $greenLine = Get-Content -LiteralPath $summaryPath | Where-Object { $_ -like 'ipc_path_idempotence_green=*' } | Select-Object -First 1
        if( -not [string]::IsNullOrWhiteSpace( $greenLine ) )
        {
            $green = ( $greenLine -replace '^ipc_path_idempotence_green=', '' ).Trim()
        }
    }

    $ok = if( $green -eq "true" -and $exitCode -eq 0 ) { "true" } else { "false" }

    $result = [pscustomobject]@{
        Ok = $ok
        TargetRunDir = $targetRunDir
        ExitCode = $exitCodeStr
        SummaryPath = $summaryPath
        Green = $green
    }

    if( $ok -ne "true" -and -not $NoThrow )
    {
        $pathBlocker = "summary_absent"
        if( $summaryPath -and (Test-Path -LiteralPath $summaryPath) )
        {
            $blockerLine = Get-Content -LiteralPath $summaryPath | Where-Object { $_ -like 'exact_blocker_if_any=*' } | Select-Object -First 1
            if( -not [string]::IsNullOrWhiteSpace( $blockerLine ) )
            {
                $pathBlocker = ( $blockerLine -replace '^exact_blocker_if_any=', '' ).Trim()
            }
        }
        throw "test_ipc_qa_path_idempotence.ps1 not green (exit=$exitCodeStr exact_blocker_if_any=$pathBlocker)"
    }

    return $result
}
