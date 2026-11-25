# Task registry

This registry tracks upstream SCALE assignments. The maintainer or their
delegate confirms claims and owner changes, reconciles this file and merges
to `integration`. Contributors update the **Status** of their own tasks in
their task branches. Read the current `integration` copy before requesting
a claim. Contributors can coordinate their own local work independently.

Statuses: `READY` (assigned, unblocked) · `IN_PROGRESS` · `REVIEW` (branch
pushed, full suite green) · `DONE` (merged to main) · `BLOCKED` (reason given)
· `WAITING` (depends on unfinished tasks)
· `PROPOSED` (unassigned and awaiting coordinator review).

## Board

| ID | Title | Owner | Status | Depends on | Milestone |
|---|---|---|---|---|---|
| T-000 | Foundation scaffold | Claude | DONE | — | M0 |
| T-001 | Replace in-tree test harness with GoogleTest | Claude | DONE | T-000 | M0 |
| T-101 | PE32+ inspector library | Claude | DONE | T-000 | M1 |
| T-102 | PE test executables + dumpbin comparison | Claude | DONE | T-101 | M1 |
| T-103 | `scale inspect` command | Claude | DONE | T-101 | M1 |
| T-201 | Worker process supervisor | Claude | DONE | T-000 | M2 |
| T-202 | First native test executable, reference run, loader decision | Claude | DONE | T-101, T-201 | M2 |
| T-203 | `scale run`, relocated-data test executable, report.json | Claude | DONE | T-202 | M2 |
| T-204 | Missing synthetic import diagnostic | Claude | DONE | T-203 | M2/M3 |
| T-301 | Exception directory registration (x64 unwind/SEH) | Claude | DONE | T-202 | M3 |
| T-302 | TLS directory: static TLS and callbacks | Claude | READY | T-301 | M3 |
| T-601 | Vulkan compute plumbing harness | Claude | DONE | T-001 | M6 |
| T-701 | gfx1030 decoder (first subset) | Codex | DONE | T-001 | M7 |
| T-702 | Shader IR v0 | Codex | DONE | T-701, T-704 | M7 |
| T-703 | SPIR-V emitter + validation | Codex | DONE | T-702 | M7 |
| T-704 | Synthetic shader ABI v0 contract | Codex | DONE | T-000 | M7 |
| T-705 | Shader test-case library (JSON) | Codex | DONE | T-704 | M7 |
| T-706 | `scale-shader` CLI (decode / compile / validate) | Codex | DONE | T-705 | M7 |
| T-707 | Shader ABI v1 contract: EXEC masks and control flow | Codex | DONE | T-704 | M7 |
| T-708 | Decoder for ABI v1 (decode.h v3) | Codex | DONE | T-707 | M7 |
| T-709 | IR + SPIR-V for ABI v1 | Codex | DONE | T-708 | M7 |
| T-730 | Shader ABI v2 contract: memory and resources | Codex | DONE | T-707 | M7 |
| T-731 | Decoder and test-case fields for ABI v2 | Codex | IN_PROGRESS (interfaces frozen) | T-730 | M7 |
| T-732 | IR + SPIR-V for ABI v2 | Codex | WAITING | T-731 | M7 |
| T-733 | Reference interpreter for ABI v2 | Claude | IN_PROGRESS | T-730, T-731 (interfaces) | M7 |
| T-734 | ABI v2 GPU differential cases | Claude | WAITING | T-732, T-733 | M7 |
| T-711 | RDNA2 reference interpreter (ABI v0) | Claude | DONE | T-701, T-704 | M7 |
| T-712 | Reference interpreter for ABI v1 | Claude | DONE | T-708 | M7 |
| T-720 | Differential runner: interpreter vs GPU (inline ABI v0 cases) | Claude | DONE | T-601, T-703, T-711 | M7 |
| T-721 | Differential runner on JSON test cases, ABI v1 GPU cases | Claude | DONE | T-705, T-720, T-709 | M7 |
| T-801 | Portability groundwork (decision 0004) | Claude | DONE | T-000 | infra |
| T-802 | Linux build/test environment and CI | unassigned | BLOCKED (no Linux environment) | T-801 | infra |
| T-303 | Bind normal imports through the IAT | unassigned | PROPOSED | T-204 | M3 |
| T-304 | Load a guest DLL from an explicit module root | unassigned | PROPOSED | T-302, T-303 | M3 |
| T-305 | Resolve forwarded exports | unassigned | PROPOSED | T-304 | M3 |
| T-306 | Run one ordinary CRT executable | unassigned | PROPOSED | T-302, T-303 | M3 |
| T-307 | Inspect and bind delay imports | unassigned | PROPOSED | T-304 | M3 |
| T-401 | Register SCALE-owned console module adapters | unassigned | PROPOSED | T-303 | M4 |
| T-402 | Inventory console imports and select the first module slice | unassigned | PROPOSED | T-103 | M4 |
| T-403 | Implement the first evidence-backed console export group | unassigned | PROPOSED | T-401, T-402 | M4 |
| T-404 | Separate read-only title assets from writable data | unassigned | PROPOSED | — | M4 |
| T-501 | Run a headless synthetic title across loader and runtime | unassigned | PROPOSED | T-304, T-306, T-401, T-404 | M5 |
| T-602 | Render a deterministic Vulkan triangle through a synthetic adapter | unassigned | PROPOSED | T-601 | M6 |
| T-735 | Translate a minimal vertex and pixel shader pair | unassigned | PROPOSED | T-734 | M7 |
| T-803 | Feed translated stage shaders into the graphics pipeline | unassigned | PROPOSED | T-602, T-735 | M8 |
| T-901 | Exercise local data and input with deterministic host doubles | unassigned | PROPOSED | T-401, T-404 | M9 |
| T-902 | Submit a synthetic PCM buffer through an audio adapter | unassigned | PROPOSED | T-401 | M9 |
| T-308 | [Look up guest modules and exports at runtime](research/TRANSLATION_GAPS.md#t-308--look-up-guest-modules-and-exports-at-runtime) | unassigned | PROPOSED | T-304, T-305 | M3 |
| T-405 | [Read local title identity and executable metadata](research/TRANSLATION_GAPS.md#t-405--read-local-title-identity-and-executable-metadata) | unassigned | PROPOSED | T-103, T-404 | M4 |
| T-406 | [Implement one documented GDK async queue mode](research/TRANSLATION_GAPS.md#t-406--implement-one-documented-gdk-async-queue-mode) | unassigned | PROPOSED | T-401 | M4 |
| T-407 | [Read assets through an uncompressed DirectStorage queue](research/TRANSLATION_GAPS.md#t-407--read-assets-through-an-uncompressed-directstorage-queue) | unassigned | PROPOSED | T-404, T-406 | M4 |
| T-408 | [Model one title memory allocation lifecycle](research/TRANSLATION_GAPS.md#t-408--model-one-title-memory-allocation-lifecycle) | unassigned | PROPOSED | T-401 | M4 |
| T-502 | [Record compatibility failures by stage and subsystem](research/TRANSLATION_GAPS.md#t-502--record-compatibility-failures-by-stage-and-subsystem) | unassigned | PROPOSED | T-204 | M5 |
| T-503 | [Verify the PE inspector with a public GDK PC sample](research/TRANSLATION_GAPS.md#t-503--verify-the-pe-inspector-with-a-public-gdk-pc-sample) | unassigned | PROPOSED | T-103 | M5 |
| T-504 | [Inspect a Scarlett GDK sample when a GDKX build is available](research/TRANSLATION_GAPS.md#t-504--inspect-a-scarlett-gdk-sample-when-a-gdkx-build-is-available) | unassigned | PROPOSED | T-103, T-402 | M5 |
| T-603 | [Draw a textured primitive with host shaders](research/TRANSLATION_GAPS.md#t-603--draw-a-textured-primitive-with-host-shaders) | unassigned | PROPOSED | T-602 | M6 |
| T-604 | [Submit dependent GPU work with correct resource states](research/TRANSLATION_GAPS.md#t-604--submit-dependent-gpu-work-with-correct-resource-states) | unassigned | PROPOSED | T-603 | M6 |
| T-605 | [Present and resize a host window](research/TRANSLATION_GAPS.md#t-605--present-and-resize-a-host-window) | unassigned | PROPOSED | T-604 | M6 |
| T-606 | [Check one placed-resource aliasing case](research/TRANSLATION_GAPS.md#t-606--check-one-placed-resource-aliasing-case) | unassigned | PROPOSED | T-604 | M6 |
| T-607 | [Show frame pacing in the host window](research/TRANSLATION_GAPS.md#t-607--show-frame-pacing-in-the-host-window) | unassigned | PROPOSED | T-605 | M6 |
| T-736 | [Identify one readable console shader container and its stage metadata](research/TRANSLATION_GAPS.md#t-736--identify-one-readable-console-shader-container-and-its-stage-metadata) | unassigned | PROPOSED | T-706 | M7 |
| T-737 | [Translate one texture sampling instruction family](research/TRANSLATION_GAPS.md#t-737--translate-one-texture-sampling-instruction-family) | unassigned | PROPOSED | T-734, T-603 | M7 |
| T-738 | [Model local data share and a workgroup barrier](research/TRANSLATION_GAPS.md#t-738--model-local-data-share-and-a-workgroup-barrier) | unassigned | PROPOSED | T-734 | M7 |
| T-739 | [Verify one wave64 operation across lane 31/32](research/TRANSLATION_GAPS.md#t-739--verify-one-wave64-operation-across-lane-3132) | unassigned | PROPOSED | T-734 | M7 |
| T-740 | [Verify one storage-buffer atomic operation](research/TRANSLATION_GAPS.md#t-740--verify-one-storage-buffer-atomic-operation) | unassigned | PROPOSED | T-734 | M7 |
| T-804 | [Identify the first console graphics ABI slice](research/TRANSLATION_GAPS.md#t-804--identify-the-first-console-graphics-abi-slice) | unassigned | PROPOSED | T-402 | M8 |
| T-805 | [Bind a guest resource to one translated shader pipeline](research/TRANSLATION_GAPS.md#t-805--bind-a-guest-resource-to-one-translated-shader-pipeline) | unassigned | PROPOSED | T-803, T-737, T-804 | M8 |
| T-806 | [Replay one evidenced recorded command sequence](research/TRANSLATION_GAPS.md#t-806--replay-one-evidenced-recorded-command-sequence) | unassigned | PROPOSED | T-604, T-804, T-805 | M8 |
| T-807 | [Cache verified shader translations with explicit invalidation](research/TRANSLATION_GAPS.md#t-807--cache-verified-shader-translations-with-explicit-invalidation) | unassigned | PROPOSED | T-803 | M8 |
| T-808 | [Untile and upload one evidenced texture layout](research/TRANSLATION_GAPS.md#t-808--untile-and-upload-one-evidenced-texture-layout) | unassigned | PROPOSED | T-603, T-804 | M8 |
| T-809 | [Show the game name and performance overlay](research/TRANSLATION_GAPS.md#t-809--show-the-game-name-and-performance-overlay) | unassigned | PROPOSED | T-405, T-607, T-803 | M8 |
| T-810 | [Warm up previously used Vulkan pipelines](research/TRANSLATION_GAPS.md#t-810--warm-up-previously-used-vulkan-pipelines) | unassigned | PROPOSED | T-807 | M8 |
| T-903 | [Connect the input adapter to one host controller](research/TRANSLATION_GAPS.md#t-903--connect-the-input-adapter-to-one-host-controller) | unassigned | PROPOSED | T-901 | M9 |
| T-904 | [Play one PCM buffer through a Windows audio backend](research/TRANSLATION_GAPS.md#t-904--play-one-pcm-buffer-through-a-windows-audio-backend) | unassigned | PROPOSED | T-902 | M9 |
| T-1001 | [Capture a repeatable early title compatibility result](research/TRANSLATION_GAPS.md#t-1001--capture-a-repeatable-early-title-compatibility-result) | unassigned | PROPOSED | T-103, T-502 | M10 |

See the [translation gap review](research/TRANSLATION_GAPS.md) for the new task
specifications, their sources and the GDK sample verification plan.

## Interface freeze points

Dependent work starts from a **frozen public interface** merged to
`integration`. The full implementation does not need to be finished first.

| Interface | Producer | Consumers | State |
|---|---|---|---|
| `src/shader/include/scale/shader/decode.h` | Codex | Claude (`shader_ref`), Codex (IR) | **Frozen** v4 (`755af8f`: SMEM `s_buffer_load_dword`, raw MUBUF/SMEM fields, `WaitCounters::simm16`); v3 implemented (T-708), v4 implementation in T-731 |
| `docs/contracts/shader_abi.md` | Codex | Claude (`shader_ref`, T-721, T-733), Codex (IR, SPIR-V) | **Frozen** v0 (`b629d89`) + v1 (`a5ef811`) + v2 (`05ba211`) |
| `src/shader/include/scale/shader/test_case.h` | Codex | Claude (T-721, T-734), Codex (T-706) | **Frozen** v2 (`e9e8e67`: `ShaderTestCase::v2` with `initial_state` + `constant_words`; v1 fields `e8ca799`) |

A change to a frozen interface needs a coordinator-approved note in this file.

## Shared-file protocol

The files below are shared. Make only the edits listed.

- `tools/scale/commands.cpp`: replace your command's `nullptr` handler with
  your function. Declare the handler in your own header and add your `.cpp`
  to `tools/scale/CMakeLists.txt`.
- Root `CMakeLists.txt`: approved exception for T-706 (Codex): the single line
  `scale_add_component(tools/scale_shader REQUIRED)` inside the
  `if(SCALE_ENABLE_SHADER)` block, after `src/shader_ref`.
- `tests/unit/CMakeLists.txt`, `tests/integration/CMakeLists.txt`: append
  your own clearly delimited block. Do not edit other blocks.
- `docs/TASKS.md`: change only the status of your own tasks.
- `docs/agents/<you>/HANDOFF.md`: yours alone.
- Everything else outside your listed paths, including the root
  `CMakeLists.txt`, `cmake/`, `CMakePresets.json`, `src/common`,
  `src/diagnostics` and `tests/support`, is coordinator-owned. Ask for
  changes in your HANDOFF.md (section "Requests to coordinator").

---

## Open tasks

### Proposed translation work

These proposals come from SCALE's code and contracts. See the
[reference cross-check](research/REFERENCES.md#cross-check-of-the-translation-backlog)
for related implementations and limits. The coordinator
confirms scope and assigns owned paths before implementation.

#### T-303 — Bind normal imports through the IAT

`import_resolver.cpp` currently reports every import as unresolved and `image_map_win32.cpp` has no IAT binding step. Carry resolved addresses into the mapper and validate every slot before applying page protections. A synthetic EXE must call a registered function and return the expected value. A missing required import or invalid IAT slot must fail before entry.

#### T-304 — Load a guest DLL from an explicit module root

The mapper currently rejects DLL images. Start with a self-built DLL from a caller-configured module root and a guest EXE that imports it by name and ordinal. Test export bounds, initialization ordering and unload behavior. A missing module or failed initialization must produce a named diagnostic.

#### T-305 — Resolve forwarded exports

Support forwarded exports in a pair of self-built DLLs from the same module root. Test forwarding by name and ordinal, and reject missing targets and cycles with diagnostics that identify the resolution chain.

#### T-306 — Run one ordinary CRT executable

The worker calls a custom zero-argument entry. Compare one standard MSVC console executable with a native Windows run, covering static initialization, ASCII arguments, stdout and exit code. Document which startup mechanisms are required and reject unsupported ones before entry.

#### T-307 — Inspect and bind delay imports

The mapper rejects the delay-import directory. Compare inspection with `dumpbin` for one self-built `/DELAYLOAD` executable. Verify its first call and repeated calls against the native run, and report a missing delay target when invoked.

#### T-401 — Register SCALE-owned console module adapters

Add a registry keyed by guest module and name or ordinal with an evidence record for each binding. Exercise provider precedence with a synthetic module and a conflicting guest DLL. Missing signatures must leave the import unresolved. Use the same interface for later SCALE implementations of console exports.

#### T-402 — Inventory console imports and select the first module slice

Start with public references and `scale inspect` reports from locally supplied files when available. Record module, export or ordinal, sample identity, evidence for signature and behavior, and confidence. Identify a small set of related exports with verifiable contracts. Record missing evidence under U-002 and keep raw binaries out of the repo.

#### T-403 — Implement the first evidence-backed console export group

Implement the smallest group selected by T-402 as a SCALE-owned provider. Write ABI and behavior tests covering arguments, return codes and resource lifetime where applicable. An unknown required export from the same module must still produce `unresolved_import`.

#### T-404 — Separate read-only title assets from writable data

The architecture lists `src/filesystem` as planned. Add path mapping for one read-only title mount and one writable data mount. Test file reads, writes, missing paths and rejected escapes, including Windows reparse points. Confirm that test writes leave the title files unchanged.

#### T-501 — Run a headless synthetic title across loader and runtime

Build one source-only executable that starts through the CRT, calls a synthetic adapter and uses both filesystem mounts. Assert per-stage diagnostics, final exit code and write location. Run a negative variant with a missing import and verify that no guest code runs.

#### T-602 — Render a deterministic Vulkan triangle through a synthetic adapter

`vulkan_compute.cpp` only dispatches compute. Add an offscreen graphics pipeline using known host shaders, a color target and readback. Assert sampled pixels inside and outside the triangle. Record the GPU and a capability-specific skip or error when the required format is unavailable.

#### T-735 — Translate a minimal vertex and pixel shader pair

The existing shader harness exercises compute. Define a synthetic stage ABI for position, one interpolant and color output. Check decoding against LLVM where supported, validate SPIR-V and test the pair on Vulkan against independently specified pixels.

#### T-803 — Feed translated stage shaders into the graphics pipeline

Use the triangle path with SCALE-translated vertex and pixel shaders, making stage interfaces, resource bindings and synchronization explicit. Compare readback with the host shader baseline and check that a mismatched stage interface fails before submission.

#### T-901 — Exercise local data and input with deterministic host doubles

Add a synthetic adapter test that reads scripted input and saves then reloads local state under the writable root. Verify the disconnected-device and unavailable-service results.

#### T-902 — Submit a synthetic PCM buffer through an audio adapter

Define a small synthetic audio adapter with explicit format, timing and ownership. Test submitted samples, completion order and unsupported formats with a fake sink before adding a host backend.

### T-730 — Shader ABI v2 contract: memory and resources (Codex)

**Paths owned:** `docs/contracts/shader_abi.md` (new v2 section).

Contract first, no code; same rules as T-707 (cite the RDNA2 ISA document
70648 and LLVM AMDGPUUsage, mark every rule as a synthetic ABI decision).
Scope:
- MUBUF beyond ABI v0: nonzero immediate offsets, an SGPR `soffset`, and
  per-lane addresses computed by the program instead of only `v1 = 4*lane`.
- Out-of-range buffer accesses: define the behavior only as far as the ISA
  documents it for the descriptor the ABI fixes (e.g. loads returning 0,
  stores dropped). Anything not documented is rejected before dispatch by
  the interpreter's pre-check, never assumed.
- Scalar memory: a third, read-only constant buffer, `s_load_dword` /
  `s_buffer_load_dword` (pick what the ISA and LLVM support for gfx1030 and
  justify it), and `s_waitcnt lgkmcnt(0)` with pending-scalar-load rules
  analogous to the v1 per-lane pending rule.
- How the extra buffer maps to Vulkan/SPIR-V, and the JSON test-case fields
  (constant buffer words, `abi_version: 2`).
**Acceptance:** Claude reviews and approves; then T-731..T-734 start.

### T-731 — Decoder and test-case fields for ABI v2 (Codex, WAITING on T-730)

Freeze `decode.h` v4 (SMEM encoding, MUBUF offset fields already present
where possible) and `test_case.h` v2 fields first, as separate interface
steps. Then implementation, LLVM cross-check for every new form, negative
tests.

### T-732 — IR + SPIR-V for ABI v2 (Codex, WAITING on T-731)

Lower ABI v2 per the contract, spirv-val on every case, new v2 cases in
`tests/shader/cases/`.
### T-302 — TLS directory: static TLS and callbacks (Claude)

Support the TLS directory in the worker for test executables: allocate the
image's TLS index and template per thread (`StartAddressOfRawData`..`End`,
`SizeOfZeroFill`), run TLS callbacks with `DLL_PROCESS_ATTACH` before the
entry and `DLL_THREAD_ATTACH`/`DETACH` for threads the test creates, using
documented mechanisms only. Test executables: static TLS read/write on two
threads, a callback with an observable effect. Native reference runs for
both. Malformed TLS directories are rejected before entry. If a documented
mechanism for a privately mapped image does not exist, record the gap in
docs/research/KNOWN_UNKNOWNS.md and a decision record before choosing an
approach.
### T-802 — Linux build/test environment and CI (unassigned, BLOCKED)

Needs a Linux machine or CI runner with Vulkan. Scope: GCC/Clang presets with
Ninja, build the portable components, run their tests and the Vulkan GPU
tests (SKIPPED without a device). Can't be marked done without real Linux
runs.

---

## Done tasks (summary)

Evidence, commands and results for each are in docs/PROGRESS.md.

- **T-000** Foundation: CMake + presets, `scale::common`, `scale::diagnostics`
  (JSON writer, deduplicating log, report.json v1), CLI skeleton.
- **T-001** GoogleTest 1.18.0 via FetchContent (URL + SHA-256);
  `GTEST_SKIP()` reported as SKIPPED (docs/contracts/testing.md).
- **T-101** Bounded PE32+ AMD64 inspector (`scale::loader`), every error code
  tested with synthetic images.
- **T-102** PE test executables, metadata compared with dumpbin.
- **T-103** `scale inspect <file> [--json]` (docs/contracts/inspect.md).
- **T-201** Worker supervisor: job object, timeout, bounded output capture,
  exception codes.
- **T-202** `test_ret42`, native reference run, decision 0002 (custom mapping
  in a worker, OS loader as reference).
- **T-203** `scale run`, relocated-data test executable, `--report`
  (docs/contracts/run.md).
- **T-204** Unresolved synthetic import blocks execution with a fatal
  `runtime/import_resolution/unresolved_import` diagnostic.
- **T-301** Exception directory validated (`exception_table.h`) and registered
  with `RtlAddFunctionTable`; `test_seh` handles an access violation through
  its own frame handler, natively and under SCALE (101).
- **T-601** Vulkan compute harness (`scale::graphics`), exact readback, label
  `gpu`.
- **T-701** gfx1030 decoder subset, LLVM cross-check.
- **T-702** Shader IR v0. **T-703** SPIR-V emitter + spirv-val.
- **T-704** Synthetic shader ABI v0. **T-707** Shader ABI v1.
- **T-705** Strict JSON test-case library (`scale::shader_testcase`).
- **T-708** Decoder for ABI v1 (VOPC, saveexec, exec_lo moves, branches),
  LLVM cross-check.
- **T-711** Reference interpreter (`scale::shader_ref`) for ABI v0.
- **T-712** Reference interpreter for ABI v1, cross-checked against the v3
  decoder.
- **T-706** `scale-shader` CLI: decode / compile / validate, v0 and v1.
- **T-709** IR + SPIR-V for ABI v1 (`lower_ir_v1(decoded, initial_exec)`,
  structured dispatcher, EXEC guards, workgroup VCC exchange).
- **T-720** First differential slice: interpreter and GPU agree with manual
  values on 4 inline ABI v0 cases (RX 9070 XT).
- **T-721** Differential runner on `tests/shader/cases/*.json`, one CTest
  test per case; all 10 cases (4 ABI v0, 6 ABI v1) agree with the interpreter and manual values on the GPU.
- **T-801** Portable `scale::common`, platform gating, portable test tool
  runner, no-OS-headers check.
