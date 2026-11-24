# Task registry

The shared source of truth for who works on what. Only the coordinator
(Claude) assigns tasks, changes owners or merges. Agents update the **Status**
of their own tasks in their task branch. The coordinator reconciles this file
on `integration`. Read the copy on `integration`, not a stale local copy,
before claiming anything.

Statuses: `READY` (assigned, unblocked) · `IN_PROGRESS` · `REVIEW` (branch
pushed, full suite green) · `DONE` (merged to main) · `BLOCKED` (reason given)
· `WAITING` (depends on unfinished tasks).

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
| T-730 | Shader ABI v2 contract: memory and resources | Codex | READY | T-707 | M7 |
| T-731 | Decoder and test-case fields for ABI v2 | Codex | WAITING | T-730 | M7 |
| T-732 | IR + SPIR-V for ABI v2 | Codex | WAITING | T-731 | M7 |
| T-733 | Reference interpreter for ABI v2 | Claude | WAITING | T-730, T-731 (interfaces) | M7 |
| T-734 | ABI v2 GPU differential cases | Claude | WAITING | T-732, T-733 | M7 |
| T-711 | RDNA2 reference interpreter (ABI v0) | Claude | DONE | T-701, T-704 | M7 |
| T-712 | Reference interpreter for ABI v1 | Claude | DONE | T-708 | M7 |
| T-720 | Differential runner: interpreter vs GPU (inline ABI v0 cases) | Claude | DONE | T-601, T-703, T-711 | M7 |
| T-721 | Differential runner on JSON test cases, ABI v1 GPU cases | Claude | DONE | T-705, T-720, T-709 | M7 |
| T-801 | Portability groundwork (decision 0004) | Claude | DONE | T-000 | infra |
| T-802 | Linux build/test environment and CI | unassigned | BLOCKED (no Linux environment) | T-801 | infra |

Unassigned backlog: title filesystem / path translation (M4), adapter
registry (M4), graphics triangle slice (M6).

## Interface freeze points

Dependent work starts from a **frozen public interface** merged to
`integration`. The full implementation does not need to be finished first.

| Interface | Producer | Consumers | State |
|---|---|---|---|
| `src/shader/include/scale/shader/decode.h` | Codex | Claude (`shader_ref`), Codex (IR) | **Frozen** v3 (`9ebc332`), implemented (T-708) |
| `docs/contracts/shader_abi.md` | Codex | Claude (`shader_ref`, T-721), Codex (IR, SPIR-V) | **Frozen** v0 (`b629d89`) + v1 (`a5ef811`) |
| `src/shader/include/scale/shader/test_case.h` | Codex | Claude (T-721), Codex (T-706) | **Frozen** (`e8ca799`, v0 + v1 fields) |

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
