<div align="center">
  <img src="docs/logo-scale.svg" alt="SCALE logo" width="132" />
  <h1>SCALE</h1>
  <p><strong>Scarlett Compatibility and Layered Execution</strong></p>
  <p>An independent Windows x64 compatibility layer for Xbox Series X|S software.</p>
  <p>
    <img src="https://img.shields.io/badge/platform-Windows%2011%20x64-454545?style=flat-square" alt="Windows 11 x64" />
    <img src="https://img.shields.io/badge/C%2B%2B-23-454545?style=flat-square" alt="C++23" />
    <img src="https://img.shields.io/badge/backend-Vulkan-454545?style=flat-square" alt="Vulkan backend" />
    <img src="https://img.shields.io/badge/status-early%20development-df3853?style=flat-square" alt="Early development" />
  </p>
  <p><a href="#development-status">Status</a> · <a href="#roadmap">Roadmap</a> · <a href="#getting-started">Getting started</a> · <a href="#command-line-interface">CLI</a> · <a href="#documentation">Documentation</a></p>
</div>

---

## Overview

SCALE aims to execute compatible AMD64 machine code natively on Windows, provide the required platform runtime, translate graphics operations and recompile RDNA2 shaders to SPIR-V.

The project is developed incrementally, with an emphasis on correctness, reproducible tests and evidence-backed compatibility. Native execution, runtime fidelity, graphics translation and shader recompilation are core parts of the long-term architecture.

> [!IMPORTANT]
> SCALE is in early development. No retail software is currently supported or tested. The capabilities listed as planned below are not implemented.

## Development status

| Milestone | Component         | Status       | Current scope                                                                            |
| :-------- | :---------------- | :----------- | :--------------------------------------------------------------------------------------- |
| M0        | Foundation        | **Complete** | Build system, CLI skeleton, diagnostics contract and CTest infrastructure.               |
| M1        | PE inspector      | **Complete** | `scale::loader`, read-only PE32+ inspection and test executables compared against `dumpbin`.     |
| M2        | Native execution  | **Complete** | Supervised worker, image loader, `scale run` with `report.json`, unresolved-import diagnostics. |

For verified results, see [Development progress](docs/PROGRESS.md). For the complete milestone scope and work registry, see the [Roadmap](docs/ROADMAP.md) and [Tasks](docs/TASKS.md).

## Roadmap

SCALE is being built as a complete translation layer that runs Xbox Series X|S executables on Windows. The executable's AMD64 code runs natively on the host CPU. Everything else it expects from the console is supplied by SCALE: system services, graphics, shaders, audio and input. Every pillar is required, and an executable is only considered working once all of them do their part.

| Pillar | Milestones | Status | Scope |
| :----- | :--------- | :----- | :---- |
| **CPU execution and runtime** | M3–M5 | **In progress** | Done: exception-table registration, so x64 exception dispatch works in mapped executables. Next: TLS, then import binding through documented adapters, standard CRT startup, TLS, x64 unwinding/SEH and C++ exceptions, DLL initialization, system adapters with verified signatures, title filesystem with a writable data root, and a synthetic test title that combines them. |
| **Shaders** | M7 | **In progress** | RDNA2 (gfx1030) recompiler to SPIR-V. **First compute slice verified on real hardware:** decoder, typed IR and SPIR-V, checked by an independent reference interpreter and on the GPU against hand-computed results. EXEC masks and scalar control flow (ABI v1) are verified on the GPU too. Then wave operations, memory/resources and vertex/pixel stages. |
| **Graphics** | M6, M8 | **In progress** | Vulkan host backend ([decision 0003](docs/decisions/0003-gpu-backend.md)): the compute harness runs on every Vulkan device and records its capabilities ([GPU targets](docs/contracts/gpu_targets.md)). Next: a verified triangle slice, then the console graphics API mapped onto Vulkan, with resources, descriptors, synchronization, presentation and pipelines built from translated shaders. |
| **Audio, input and local data** | M9 | Planned | Interfaces demanded by verified test titles, built on public host APIs. Offline behavior is accurate, and unavailable services report documented errors. Nothing impersonates online services. |
| **Running executables** | M10 | Gated | Authorized retail testing starts only after the readiness baseline in [AGENTS.md](AGENTS.md) is met. After that, compatibility improves blocker by blocker from observed evidence. Each executable is tracked per stage (inspected, loaded, initialized, menu, rendering, interactive). |

Nothing on this roadmap is implemented beyond what the status tables show. Broad compatibility is the goal, not a promise.

## Getting started

### Prerequisites

| Requirement                                 | Purpose                                |
| :------------------------------------------ | :------------------------------------- |
| Windows 11 x64                              | Development and execution environment  |
| Visual Studio 2022 or newer, or Build Tools | MSVC x64 toolset and Windows SDK       |
| Desktop development with C++ workload       | C++ toolchain, bundled CMake and Ninja |
| Git                                         | Version control                        |

Windows 11 x64 is the first supported platform. Linux is planned: the Vulkan backend and the shader, diagnostics and PE inspection components are kept platform-independent ([decision 0004](docs/decisions/0004-platform-portability.md)). Linux builds are not supported or tested yet.

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
| `integration`  | CLI, worker and test executable tests |
| `full`         | Full Debug suite              |
| `release-full` | Full Release suite            |
| `gpu`          | GPU tests on every Vulkan device (uses `debug-all`) |
| `full-all`     | Full Debug suite with shader and graphics (`debug-all`) |

> [!NOTE]
> GPU tests need the `debug-all` preset (`cmake --preset debug-all`, which enables the shader and graphics components and needs the Vulkan SDK and LLVM AMDGPU tools). Every GPU test runs on every Vulkan device present and prints its capability fingerprint. Without a usable device it is SKIPPED, never PASS. See [docs/contracts/gpu_targets.md](docs/contracts/gpu_targets.md).

## Command-line interface

```text
scale help
scale version
scale inspect <file> [--json]
scale run <executable> [--report <path>] [--timeout-ms <n>]
```

| Command   | Description                                                  | Availability |
| :-------- | :----------------------------------------------------------- | :----------- |
| `help`    | Display available commands                                   | Available    |
| `version` | Display the project version                                  | Available    |
| `inspect` | Inspect a PE32+ executable without executing it              | Available    |
| `run`     | Execute a trusted self-authored test executable in a supervised worker | Available    |

The PE inspection interface is documented in [docs/contracts/inspect.md](docs/contracts/inspect.md), and test executable execution and `report.json` are documented in [docs/contracts/run.md](docs/contracts/run.md). `scale run` accepts only trusted test executables with the custom entry contract `int test_entry(void)`; it is not a general executable runner.

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
| [Contributing](CONTRIBUTING.md)             | How to join the pipeline: tasks, branches, review  |
| [Multi-agent workflow](docs/MULTI_AGENT.md) | Coordination and integration rules                 |
| [Contracts](docs/contracts/)                | Shared interfaces, diagnostics and testing         |
| [Research](docs/research/)                  | Known unknowns and technical references            |
| [AGENTS.md](AGENTS.md)                      | Mission, development rules and acceptance criteria |

## Legal scope

SCALE is an independent project and is not affiliated with Microsoft or Xbox.

The project uses public documentation and self-authored test material only. It does not implement package decryption, DRM bypass, entitlement spoofing or online service impersonation, and it ships no proprietary SDK files or third-party executables.

---

<div align="center">
  <sub>SCALE · Scarlett Compatibility and Layered Execution</sub>
</div>
