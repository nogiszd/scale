<div align="center">
  <img src="docs/logo-scale.svg" alt="Scale logo" width="132" />
  <h1>Scale</h1>
  <p><strong>Scarlett Compatibility and Layered Execution</strong></p>
  <p>An independent Windows x64 compatibility layer for Xbox Series X|S software.</p>
  <p>
    <img src="https://img.shields.io/badge/platform-Windows%2011%20x64-454545?style=flat-square" alt="Windows 11 x64" />
    <img src="https://img.shields.io/badge/C%2B%2B-23-454545?style=flat-square" alt="C++23" />
    <img src="https://img.shields.io/badge/backend-Vulkan%20(planned)-454545?style=flat-square" alt="Planned Vulkan backend" />
    <img src="https://img.shields.io/badge/status-early%20development-df3853?style=flat-square" alt="Early development" />
  </p>
  <p><a href="#development-status">Status</a> · <a href="#getting-started">Getting started</a> · <a href="#command-line-interface">CLI</a> · <a href="#documentation">Documentation</a></p>
</div>

---

## Overview

Scale aims to execute compatible AMD64 machine code natively on Windows, provide the required platform runtime, translate graphics operations and recompile RDNA2 shaders to SPIR-V.

The project is developed incrementally, with an emphasis on correctness, reproducible tests and evidence-backed compatibility. Native execution, runtime fidelity, graphics translation and shader recompilation are core parts of the long-term architecture.

> [!IMPORTANT]
> Scale is in early development. No retail software is currently supported or tested. The capabilities listed as planned below are not implemented.

## Development status

| Milestone | Component         | Status       | Current scope                                                                            |
| :-------- | :---------------- | :----------- | :--------------------------------------------------------------------------------------- |
| M0        | Foundation        | **Complete** | Build system, CLI skeleton, diagnostics contract and CTest infrastructure.               |
| M1        | PE inspector      | **Complete** | `scale::loader`, read-only PE32+ inspection and fixtures compared against `dumpbin`.     |
| M2        | Native execution  | In progress  | Supervised worker, fixture loader and `scale run`; missing-import diagnostic next (T-204). |
| M6        | Graphics          | Planned      | Vulkan graphics harness; requires Vulkan SDK (T-601).                                    |
| M7        | Shader recompiler | Planned      | RDNA2 decoding, IR and SPIR-V output; requires LLVM AMDGPU and Vulkan SDK (T-701–T-705). |

For verified results, see [Development progress](docs/PROGRESS.md). For the complete milestone scope and work registry, see the [Roadmap](docs/ROADMAP.md) and [Tasks](docs/TASKS.md).

## Getting started

### Prerequisites

| Requirement                                 | Purpose                                |
| :------------------------------------------ | :------------------------------------- |
| Windows 11 x64                              | Development and execution environment  |
| Visual Studio 2022 or newer, or Build Tools | MSVC x64 toolset and Windows SDK       |
| Desktop development with C++ workload       | C++ toolchain, bundled CMake and Ninja |
| Git                                         | Version control                        |

Graphics and shader development additionally requires:

- [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) — Vulkan headers, libraries and SPIR-V validation tools.
- LLVM tools (`llvm-mc` and `llvm-objdump`) with AMDGPU support. The standard Windows distribution does not provide the required complete toolset. The repository provides a script to build the tools from pinned source without administrator privileges:

  ```powershell
  .\scripts\build-llvm-amdgpu.ps1 -AddToUserPath
  ```

  The build typically takes 10–30 minutes, depending on the host.

### Configure the environment

Open PowerShell in the repository root and initialize the MSVC x64 environment:

```powershell
. .\scripts\vsenv.ps1
.\scripts\check-env.ps1
```

The first script configures the current session only. The second reports the availability of required tools.

### Build

Configure and build the Debug preset:

```powershell
cmake --preset debug
cmake --build --preset debug
```

For a Release build:

```powershell
cmake --preset release
cmake --build --preset release
```

Build directories are located in `out/build/<preset>/`, with executables in `out/build/<preset>/bin/`.

Optional component flags:

| CMake option            | Purpose                       |
| :---------------------- | :---------------------------- |
| `SCALE_ENABLE_SHADER`   | Enable the shader component   |
| `SCALE_ENABLE_GRAPHICS` | Enable the graphics component |

Pass optional flags during configuration using `-DSCALE_ENABLE_SHADER=ON` or `-DSCALE_ENABLE_GRAPHICS=ON`. These flags do not imply that the respective components are already implemented.

### Run tests

```powershell
ctest --preset quick
ctest --preset integration
ctest --preset full
```

For Release tests, build the Release preset first, then run `ctest --preset release-full`.

| Test preset    | Scope                         |
| :------------- | :---------------------------- |
| `quick`        | Small CPU tests               |
| `integration`  | CLI, worker and fixture tests |
| `full`         | Full Debug suite              |
| `release-full` | Full Release suite            |
| `gpu`          | Hardware GPU tests            |

> [!NOTE]
> `ctest --preset gpu` intentionally fails with `No tests were found` while no GPU tests exist. An empty GPU test run is never considered a pass.

## Command-line interface

```text
scale help
scale version
scale inspect <file> [--json]
scale run <trusted-fixture.exe> [--report <path>] [--timeout-ms <n>]
```

| Command   | Description                                                  | Availability |
| :-------- | :----------------------------------------------------------- | :----------- |
| `help`    | Display available commands                                   | Available    |
| `version` | Display the project version                                  | Available    |
| `inspect` | Inspect a PE32+ executable without executing it              | Available    |
| `run`     | Execute a trusted self-authored fixture in a supervised worker | Available    |

The PE inspection interface is documented in [docs/contracts/inspect.md](docs/contracts/inspect.md), and fixture execution and `report.json` are documented in [docs/contracts/run.md](docs/contracts/run.md). `scale run` accepts only trusted fixtures with the custom entry contract `int fixture_entry(void)`; it is not a general executable runner.

**Exit codes**

| Code | Meaning                 |
| :--: | :---------------------- |
| `0`  | Success                 |
| `1`  | Operation failed        |
| `2`  | Invalid command usage   |
| `3`  | Command not implemented |

## Documentation

| Document                                    | Description                                        |
| :------------------------------------------ | :------------------------------------------------- |
| [Architecture](docs/architecture.md)        | Components, dependencies and ownership             |
| [Roadmap](docs/ROADMAP.md)                  | Milestones and implementation scope                |
| [Development progress](docs/PROGRESS.md)    | Verified results and commands                      |
| [Task registry](docs/TASKS.md)              | Work items for parallel development                |
| [Multi-agent workflow](docs/MULTI_AGENT.md) | Coordination and integration rules                 |
| [Contracts](docs/contracts/)                | Shared interfaces, diagnostics and testing         |
| [Research](docs/research/)                  | Known unknowns and technical references            |
| [AGENTS.md](AGENTS.md)                      | Mission, development rules and acceptance criteria |

## Legal scope

Scale is an independent project and is not affiliated with Microsoft or Xbox.

The project uses public documentation and self-authored test material only. It does not implement package decryption, DRM bypass, entitlement spoofing or online service impersonation, and it ships no proprietary SDK files or games.

---

<div align="center">
  <sub>Scale · Scarlett Compatibility and Layered Execution</sub>
</div>
