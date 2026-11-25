# Build and test SCALE

## Get the source

```powershell
git clone https://github.com/nogiszd/scale.git
cd scale
```

## Prerequisites

| Requirement                                 | Purpose                                |
| :------------------------------------------ | :------------------------------------- |
| Windows 11 x64                              | Development and execution environment  |
| Visual Studio 2022 or newer, or Build Tools | MSVC x64 toolset and Windows SDK       |
| Desktop development with C++ workload       | C++ toolchain, bundled CMake and Ninja |
| Git                                         | Version control                        |

Windows 11 x64 is the first supported platform. Linux is planned: the Vulkan backend and the shader, diagnostics and PE inspection components are kept platform-independent ([decision 0004](decisions/0004-platform-portability.md)). Linux builds are not supported or tested yet.

Graphics and shader development additionally requires:

- [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows) — Vulkan headers, libraries and SPIR-V validation tools.
- LLVM tools (`llvm-mc` and `llvm-objdump`) with AMDGPU support. The standard Windows distribution does not provide the required complete toolset. The repository provides a script to build the tools from pinned source without administrator privileges:

  ```powershell
  .\scripts\build-llvm-amdgpu.ps1 -AddToUserPath
  ```

  The build typically takes 10–30 minutes, depending on the host.

## Configure the environment

Open PowerShell in the repository root and initialize the MSVC x64 environment:

```powershell
. .\scripts\vsenv.ps1
.\scripts\check-env.ps1
```

The first script configures the current session only. The second reports the availability of required tools.

Check `git --version`, `cmake --version`, `ninja --version`, `Get-Command cl, link, dumpbin` and the compiler/SDK architecture. Verify used C++23 features with the installed compiler. Do not auto-install dependencies or change machine-wide settings and report missing tools and exact installation steps.

For graphics work, verify `vulkaninfo --summary` and `spirv-val --version` and for shader decoding, verify `llvm-mc --version`, `llvm-objdump` and AMDGPU target registration. Pin and record tool versions. Public PC GDK does not establish console GDKX compatibility.

## Build

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

The first configure downloads the pinned, hash-checked GoogleTest release.

Build directories are located in `out/build/<preset>/`, with executables in `out/build/<preset>/bin/`.

Optional component flags:

| CMake option            | Purpose                       |
| :---------------------- | :---------------------------- |
| `SCALE_ENABLE_SHADER`   | Enable the shader component   |
| `SCALE_ENABLE_GRAPHICS` | Enable the graphics component |

Pass optional flags during configuration using `-DSCALE_ENABLE_SHADER=ON` or `-DSCALE_ENABLE_GRAPHICS=ON`. These flags do not imply that the respective components are already implemented.

## Run tests

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
> GPU tests need the `debug-all` preset (`cmake --preset debug-all`, which enables the shader and graphics components and needs the Vulkan SDK and LLVM AMDGPU tools). Every GPU test runs on every Vulkan device present and prints its capability fingerprint. Without a usable device it is SKIPPED, never PASS. See [GPU target contract](contracts/gpu_targets.md).
