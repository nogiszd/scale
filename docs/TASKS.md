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
| T-301 | Exception directory registration (x64 unwind/SEH) | Claude | READY | T-202 | M3 |
| T-601 | Vulkan compute plumbing harness | Claude | DONE | T-001 | M6 |
| T-701 | gfx1030 decoder (first subset) | Codex | DONE | T-001 | M7 |
| T-702 | Shader IR v0 | Codex | DONE | T-701, T-704 | M7 |
| T-703 | SPIR-V emitter + validation | Codex | DONE | T-702 | M7 |
| T-704 | Synthetic shader ABI v0 contract | Codex | DONE | T-000 | M7 |
| T-705 | Shader test-case library (JSON) | Codex | DONE | T-704 | M7 |
| T-706 | `scale-shader` CLI (decode / compile / validate) | Codex | READY (after T-709) | T-705 | M7 |
| T-707 | Shader ABI v1 contract: EXEC masks and control flow | Codex | DONE | T-704 | M7 |
| T-708 | Decoder for ABI v1 (decode.h v3) | Codex | DONE | T-707 | M7 |
| T-709 | IR + SPIR-V for ABI v1 | Codex | READY | T-708 | M7 |
| T-711 | RDNA2 reference interpreter (ABI v0) | Claude | DONE | T-701, T-704 | M7 |
| T-712 | Reference interpreter for ABI v1 | Claude | DONE | T-708 | M7 |
| T-720 | Differential runner: interpreter vs GPU (inline ABI v0 cases) | Claude | DONE | T-601, T-703, T-711 | M7 |
| T-721 | Differential runner on JSON test cases, ABI v1 GPU cases | Claude | IN_PROGRESS (JSON runner done; v1 GPU part waits for T-709) | T-705, T-720, T-709 | M7 |
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

### T-709 — IR + SPIR-V for ABI v1 (Codex)

**Paths owned:** `src/shader/` (IR and SPIR-V files), `tests/unit/shader/`,
`tests/shader/`.

Lower ABI v1 per `docs/contracts/shader_abi.md`: uniform EXEC/VCC/SCC/PC,
static branch-structure checks (forward only, the single store and end on
every path), a structured block dispatcher, per-lane guards, masked store,
and the workgroup-memory VCC exchange with barriers in uniform control flow.
No subgroup-size assumptions. spirv-val on every test program. The ABI v1
GPU cases already exist in `tests/shader/cases/` (`t707_*`,
`v1_scc_and_unconditional_branches`); add more there in the same format. The
case's `initial_exec` reaches the translated program without an extra
binding (contract), so document how the lowering API takes it. GPU
execution itself is T-721.

### T-706 — `scale-shader` CLI (Codex, after T-709)

**Paths owned:** `tools/scale_shader/`, its test block.

A standalone tool, independent of the native execution host (AGENTS.md):
`scale-shader decode <case.json|words>` prints the decoded instructions,
`scale-shader compile <case.json> -o <out.spv>` runs decode → IR → SPIR-V,
and `scale-shader validate <out.spv>` runs spirv-val if present (SKIPPED
otherwise). Uses `scale::shader_testcase`. Exit codes follow the `scale` CLI
convention. **Acceptance:** CLI tests on the T-704 and T-707 cases and on
malformed input. Natural home for the opt-in shader debug mode (decision
0005).

### T-721 — Differential runner on JSON test cases (Claude)

Done so far: `gpu_shader_differential` loads every `tests/shader/cases/*.json`
through `scale::shader_testcase`, one CTest test per case. ABI v0 cases run
on the interpreter and the GPU. ABI v1 cases run the interpreter checks and
SKIP the GPU part with the T-709 reason until the compiler lowers ABI v1.
Remaining: pass `initial_exec` to the lowering once T-709 defines it.

Original scope: replace the T-720 inline cases with JSON test cases loaded
through `scale::shader_testcase`. Run each case on `scale::shader_ref` (`run_abi_v0`
or `run_abi_v1`) and on the GPU, and compare both with the manual expected
values. After T-709: ABI v1 cases including partial initial EXEC. Records
device, driver, tools and seed; label `gpu`; software devices labelled
`software_gpu` (docs/contracts/gpu_targets.md).

### T-301 — Exception directory registration (Claude)

Register the mapped image's `.pdata` (`RtlAddFunctionTable` or the
documented equivalent) in the worker before entry, with bounds-checked
RUNTIME_FUNCTION entries and deregistration on unload. A self-authored test
executable raises and handles an SEH exception and a C++ exception (if the
CRT dependency is acceptable for the test contract; otherwise SEH only,
documented). Native Windows reference run for the same executable. A
malformed exception directory is rejected before entry.

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
- **T-720** First differential slice: interpreter and GPU agree with manual
  values on 4 inline ABI v0 cases (RX 9070 XT).
- **T-801** Portable `scale::common`, platform gating, portable test tool
  runner, no-OS-headers check.
