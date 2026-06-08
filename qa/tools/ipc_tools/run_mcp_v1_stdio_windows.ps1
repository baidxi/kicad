param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,

    [Parameter(Mandatory = $false)]
    [string]$Preset = "win64-release",

    [Parameter(Mandatory = $false)]
    [switch]$WithProject,

    [Parameter(Mandatory = $false)]
    [switch]$NoBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$BuildDir = (Resolve-Path $BuildDir).Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $BuildDir)
$exe = Join-Path $BuildDir "kicad\\kicad-mcp.exe"
if (!(Test-Path $exe)) { throw "Missing kicad-mcp.exe at $exe" }

if (!$NoBuild) {
    & cmake --build --preset $Preset --target kicad-mcp
    if ($LASTEXITCODE -ne 0) { throw "Build failed (kicad-mcp)." }
}

$py = Join-Path $BuildDir "vcpkg_installed\\x64-windows\\tools\\python3\\python.exe"
if (!(Test-Path $py)) { throw "Missing python at $py" }

$script = Join-Path $repoRoot "qa\\tools\\ipc_tools\\mcp_stdio_smoke.py"
if (!(Test-Path $script)) { throw "Missing smoke script at $script" }

$args = @("--builddir", $BuildDir)
if ($WithProject) { $args += "--with-project" }

& $py $script @args
if ($LASTEXITCODE -ne 0) { throw "mcp_stdio_smoke.py failed." }

