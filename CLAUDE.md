# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build

KiCad uses CMake (C++20) with Ninja. Minimum CMake 3.22 (3.28.1 for MSVC).

The build directory in this repo is configured with:

```bash
cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DKICAD_USE_CMAKE_FINDPROTOBUF=ON ../
ninja
```

Key CMake options:
- `KICAD_BUILD_QA_TESTS=ON` (default) — build C++ unit tests and CLI tests
- `KICAD_BUILD_PNS_DEBUG_TOOL=ON` — enable router debugging (Debug builds)
- `KICAD_SANITIZE_ADDRESS=ON` / `KICAD_SANITIZE_THREADS=ON` — sanitizers
- `KICAD_IPC_API=ON` (default) — build the protobuf-based API

See `CMakePresets.json.sample` for ready-made configurations for Linux, macOS, and MSVC.

## Tests

**C++ unit tests** use Boost.Test, located in `qa/tests/` organized by subsystem:

```bash
cd build
ctest -V                          # all tests
ctest -R "pattern" -V             # tests matching name pattern
```

**Python CLI tests** use pytest, located in `qa/tests/cli/`:

```bash
cd qa/tests/cli
KICAD_RUN_FROM_BUILD_DIR=1 KICAD_CLI=<build>/kicad/kicad-cli pytest
KICAD_RUN_FROM_BUILD_DIR=1 KICAD_CLI=<build>/kicad/kicad-cli pytest test_pcb.py::test_name -v
```

Python test dependencies: `pytest`, `cairosvg`, `pytest-image-diff`, `pydantic`, `numpy`.

## Code Style

- **Formatter:** `clang-format` using `_clang-format` at repo root (120-col LLVM style, Allman braces, 4-space indent)
- **Static analysis:** `.clang-tidy` at repo root
- Apply formatting: `clang-format -i <file>`
- Do **not** alphabetize `#include` directives in existing files — follow the file's existing order.
- `clang-format` is a guide, not law; when it produces wrong output (e.g. lambda formatting), follow existing file conventions instead.

Naming:
- Classes: `UPPER_CASE` or `CamelCase` (e.g. `PCB_TRACK`, `SCH_SHEET`)
- Methods: `camelCase`
- Member variables: `m_name` prefix
- Constants: `ALL_CAPS`

## Architecture

KiCad is a suite of EDA tools sharing a single process via a **KIWAY/KIFACE DSO architecture** (documented in `include/kiway.h`):

- **KIWAY** — central message router and module loader; one per project
- **KIFACE** — interface each major editor (eeschema, pcbnew, etc.) exposes when loaded as a DSO
- **KIWAY_PLAYER** — a wxFrame that holds a reference to its KIWAY, enabling cross-module event broadcasting

The project manager (`kicad/`) bootstraps the process and loads editor modules on demand. Editors communicate via KIWAY-routed wxEvents and (for the IPC API) protobuf messages.

### Major subsystems

| Directory | Role |
|-----------|------|
| `libs/` | Lowest-level libraries with no KiCad EDA deps: `core` (UTF-8, base64), `kimath` (geometry, polygons), `kiplatform` (wxWidgets extensions), `sexpr` (S-expression parser for `.kicad_*` files) |
| `common/` | Shared EDA utilities, GAL (Graphics Abstraction Layer), netlist reader, reusable dialogs/widgets |
| `eeschema/` | Schematic editor; I/O plugins in `sch_io/`; ngspice simulation in `sim/` |
| `pcbnew/` | PCB editor; PNS push-and-shove router in `router/`; connectivity solver in `connectivity/`; DRC in `drc/`; I/O plugins in `pcb_io/` |
| `3d-viewer/` | OpenGL 3D board renderer |
| `kicad/` | Project manager, job runner (`jobs_runner.cpp`), and `kicad-cli` entry point |
| `api/` | Protobuf-based IPC API — `.proto` definitions in `api/proto/`, generated code used by plugins and headless CLI jobs |
| `cvpcb/` | Component-to-footprint assignment, bridges schematic netlist with PCB |
| `qa/` | All tests: `qa/tests/` (unit + CLI), `qa/qa_utils/` (helpers), `qa/mocks/` (mock objects) |

### File format

KiCad's native format is S-expressions (`.kicad_sch`, `.kicad_pcb`, `.kicad_mod`, etc.), parsed by `libs/sexpr/` and written by component-specific I/O plugins.

### Graphics

The **GAL** (Graphics Abstraction Layer, `common/gal/`) abstracts OpenGL and Cairo backends. All 2D rendering in editors goes through GAL. The 3D viewer (`3d-viewer/`) uses its own OpenGL pipeline.

## Key Headers for Orientation

- `include/kiway.h` — KIWAY/KIFACE/KIWAY_PLAYER architecture description
- `include/pgm_base.h` — application singleton and global state
- `include/gal/graphics_abstraction_layer.h` — rendering interface
- `include/tool/tool_manager.h` — tool dispatch system used by all editors

