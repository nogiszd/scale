# References

Record the URL, the revision or date accessed, and what was used from each
source.

| Source | Used for | Accessed / revision |
|---|---|---|
| https://learn.microsoft.com/en-us/windows/win32/debug/pe-format | PE/COFF structures and directories (M1, M2: T-101..T-204) | accessed 2026-11-24 |
| https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention | x64 ABI (M3) | not yet used |
| https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64 | RUNTIME_FUNCTION, UNWIND_INFO, chained unwind info, handler flags (T-301) | accessed 2026-11-24 |
| https://gpuopen.com/machine-readable-isa/ | AMD RDNA 2 ISA reference, document 70648: encodings and semantics (T-701, T-704, T-707, T-708, T-711, T-712) | accessed 2026-11-24 |
| https://llvm.org/docs/AMDGPUUsage.html | Processors, buffer resource, initial kernel state (T-704); llvm-mc / llvm-objdump 23.1.6 encodings for every decoder and interpreter test | accessed 2026-11-24; LLVM 23.1.6 |
| https://registry.khronos.org/vulkan/ | Vulkan compute, SPIR-V unified spec (T-601, T-703, T-707) | accessed 2026-11-24; SDK 1.4.363.0 |
| https://github.com/KhronosGroup/SPIRV-Tools | spirv-val / spirv-dis (T-703, T-720) | SPIRV-Tools v2026.4 (Vulkan SDK 1.4.363.0) |
| https://github.com/microsoft/GDK, https://github.com/microsoft/Xbox-GDK-Samples | Public GDK context only; not console ABI evidence | — |
| https://github.com/xcl-dev/xcl | Public implementation reference for native loading, console adapters and graphics | `568e7db7729c` |

## Cross-check of the translation backlog

The work items were derived from SCALE's code and milestone criteria. The
sources below provide examples of similar capabilities in other projects.
XCL's runtime results are reported by its maintainers.

| SCALE tasks | Reference and what it shows |
|---|---|
| T-303–T-305, T-307 | [XCL loader](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/loader.rs) maps executables and DLLs and routes imports to different providers. Its [README](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/README.md) reports name, ordinal and delay-load support. Verify forwarding and startup behavior with SCALE's own Windows reference tests. |
| T-401–T-403 | [XCL’s XGameRuntime](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/xgameruntime/mod.rs) exposes offline GDK service adapters. Its export choices help frame research questions. Verify each ABI and its behavior before binding it in SCALE. |
| T-404, T-901, T-902 | [XCL’s README](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/README.md) describes separate game and writable storage. Its [GameInput](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/gameinput.rs) and [XAudio2](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/xaudio2.rs) modules show host adapters for input and audio. SCALE's synthetic cases provide independent test oracles. |
| T-602, T-735, T-803 | [XCL’s graphics adapter](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/d3d12x/real.rs) and [DXIL emitter](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/gpu/dxil/mod.rs) illustrate console graphics and shader integration on Windows. [vkd3d-proton](https://github.com/HansKristian-Work/vkd3d-proton) is a separate reference for mapping PC D3D12 concepts onto Vulkan. SCALE's RDNA2-to-SPIR-V path and the console graphics ABI still need their own validation. |
| T-306, T-501 | The CRT and headless title tests follow from SCALE's custom entry point and the need to verify the complete integration path. Compare with ordinary Windows execution and explicit test expectations. |

This cross-check covers the proposed slices. Full title translation also
requires evidence for console shader containers and ISA differences, graphics
resource binding and synchronization, and the platform services each title
actually calls. Track those gaps in [KNOWN_UNKNOWNS.md](KNOWN_UNKNOWNS.md)
and prioritize them as integration tests expose them.

See the [translation gap review](TRANSLATION_GAPS.md) for the additional
source comparisons, proposed tests and GDK sample matrix.
