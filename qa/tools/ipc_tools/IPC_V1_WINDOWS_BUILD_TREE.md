## Goal

Reproduce the **V1 IPC** validation on Windows when running KiCad from a **build tree** (not installed):

- `Ping`
- `GetVersion`
- `GetProjectInfo` (no project → `loaded=false`)
- `GetProjectInfo` (project passed on startup → `loaded=true`)

This assumes `KICAD_IPC_API` is enabled in the build.

## Prerequisites

- A configured build directory, e.g. `C:\dev\kicad-master\build\msvc-win64-release`
- Built artifacts:
  - `kicad\kicad.exe`
  - `qa\tools\ipc_tools\qa_ipc_smoke.exe`
  - `pcbnew\_pcbnew.dll` (build the `pcbnew` target at least once)
  - API schemas copied into `build\...\schemas` (build target `api_schema_build_copy`)

## Minimal runtime environment (PowerShell)

Set these environment variables for the session:

- `KICAD_RUN_FROM_BUILD_DIR=1`
- `KICAD_USE_EXTERNAL_PYTHONHOME=1`
- `PYTHONHOME=<build>\vcpkg_installed\x64-windows\tools\python3`
- `KICAD_CONFIG_HOME=<some isolated directory>`

### Minimal PATH prefix

To allow KiCad to lazy-load kifaces from the build tree, prepend the following directories to `PATH`:

- `<build>\kicad`
- `<build>\eeschema`
- `<build>\pcbnew`
- `<build>\common`
- `<build>\common\gal`
- `<build>\api`
- `<build>\vcpkg_installed\x64-windows\bin`
- `<build>\vcpkg_installed\x64-windows\tools\python3`

If `_eeschema.dll` or `_pcbnew.dll` fails to load with Win32 error 126, the missing piece is typically:
- the module itself was not built yet (build the target), or
- the directory containing its dependent DLLs isn’t present in `PATH`.

## Isolated config for enabling the API server

KiCad reads `kicad_common.json` from `KICAD_CONFIG_HOME\<major.minor>\`.

For V1 validation, ensure:

- `api.enable_server=true`
- `api.interpreter_path` points to the vcpkg python exe (optional, but helps in build-tree runs)

## Running the checks

1. Build / refresh schemas:

```powershell
cmake --build --preset win64-release --target api_schema_build_copy
```

2. Ensure the PCB kiface exists:

```powershell
cmake --build --preset win64-release --target pcbnew
```

3. Start KiCad without a project and run smoke client against the URL from `api.log`.

4. Start KiCad with a project file argument (a minimal `{}` `.kicad_pro` is sufficient) and run the same smoke checks.

The `qa_ipc_smoke` tool expects:

```text
qa_ipc_smoke <ipc_url> ping|get_version|get_project_info
```

