# Scale — Scarlett Compatibility and Layered Execution

Scale is an independent Windows x64 compatibility layer for Xbox Series X|S
software. It is being built in milestones. The table below shows what exists
today. Nothing beyond what it lists is implemented.

| Track | Status |
|---|---|
| Foundation (M0) | Build, CLI skeleton, diagnostics contract, CTest infrastructure |
| PE inspector (M1) | Not implemented (T-101..T-103) |
| Native execution (M2) | Not implemented (T-201..T-204) |
| Graphics (M6) | Not implemented; needs Vulkan SDK (T-601) |
| Shader recompiler (M7) | Not implemented; needs LLVM AMDGPU + Vulkan SDK (T-701..T-705) |

No retail software is supported or tested. See `docs/PROGRESS.md` for verified
results and `docs/TASKS.md` for the work registry.

## Requirements

- Windows 11 x64
- Visual Studio 2022+ or Build Tools with *Desktop development with C++*
  (MSVC x64 toolset, Windows SDK, bundled CMake and Ninja)
- Git

Graphics and shader tracks will additionally need the LunarG Vulkan SDK and an
LLVM distribution with the AMDGPU target. `scripts/check-env.ps1` reports what
is present.

## Build and test

From a normal PowerShell in the repository root:

```powershell
. .\scripts\vsenv.ps1          # import MSVC x64 environment (session only)
.\scripts\check-env.ps1        # report toolchain availability
cmake --preset debug           # or: release
cmake --build --preset debug
ctest --preset quick           # small CPU tests
ctest --preset integration     # CLI / worker / fixture tests
ctest --preset full            # everything (debug)
ctest --preset release-full    # everything (release)
```

`ctest --preset gpu` runs hardware GPU tests. While no GPU tests exist it fails
with "No tests were found", on purpose: an empty GPU run is never a pass.

Build trees go to `out/build/<preset>/`. Binaries go to `out/build/<preset>/bin/`.

Optional components: `-DSCALE_ENABLE_SHADER=ON`, `-DSCALE_ENABLE_GRAPHICS=ON`.

## CLI

```text
scale help
scale version
scale inspect <file> [--json]           (not implemented, T-103)
scale run <trusted-fixture.exe> [-- args]  (not implemented, T-203)
```

Exit codes: `0` success, `1` operation failed, `2` usage error, `3` command
not implemented yet.

## Documentation

- `AGENTS.md` — mission, rules and milestone acceptance criteria
- `docs/architecture.md` — components, dependencies, ownership
- `docs/contracts/` — shared interfaces (diagnostics, testing)
- `docs/TASKS.md` — task registry for parallel agents
- `docs/MULTI_AGENT.md` — multi-agent workflow
- `docs/PROGRESS.md` — verified results and commands
- `docs/ROADMAP.md` — milestone scope
- `docs/research/` — known unknowns and references

## Legal scope

Scale uses public documentation and self-authored test material only. It does
not implement package decryption, DRM bypass, entitlement spoofing or online
service impersonation, and it ships no proprietary SDK files or games.
