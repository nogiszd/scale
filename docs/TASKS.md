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
| T-601 | Vulkan compute plumbing harness | Claude | DONE | T-001 | M6 |
| T-801 | Portability groundwork (decision 0004) | Claude | DONE | T-000 | infra |
| T-802 | Linux build/test environment and CI | unassigned | BLOCKED (no Linux environment) | T-801 | infra |
| T-704 | Synthetic shader ABI v0 contract | Codex | DONE | T-000 | M7 |
| T-701 | gfx1030 decoder (first subset) | Codex | DONE | T-001 | M7 |
| T-702 | Shader IR v0 | Codex | DONE | T-701, T-704 | M7 |
| T-703 | SPIR-V emitter + validation | Codex | DONE | T-702 | M7 |
| T-705 | Shader test-case library (T-704 JSON) | Codex | DONE | T-704 | M7 |
| T-706 | `scale-shader` CLI (decode / compile / validate) | Codex | READY | T-705 | M7 |
| T-707 | Shader ABI v1 contract: EXEC masks and control flow | Codex | DONE | T-704 | M7 |
| T-708 | Decoder for ABI v1 (decode.h v3: vcc_lo/exec_lo, VOPC, branches) | Codex | IN_PROGRESS (interface v3 frozen) | T-707 | M7 |
| T-709 | IR + SPIR-V for ABI v1 (EXEC masks, structured branches, VCC exchange) | Codex | WAITING | T-708 | M7 |
| T-711 | RDNA2 reference interpreter | Claude | DONE | T-701 (interface), T-704 | M7 |
| T-712 | Reference interpreter for ABI v1 | Claude | READY | T-708 (interface) | M7 |
| T-720 | Differential runner: interpreter vs GPU | Claude | DONE (inline cases; JSON via T-705 next) | T-601, T-703, T-711 | M7 |

Unassigned backlog: title filesystem / path translation (M4), adapter
registry (M4).

## Interface freeze points

Dependent work starts from a **frozen public header** that has been merged to
`integration`. The full implementation does not need to be finished first.

| Interface | Producer | Consumer | Freeze deliverable |
|---|---|---|---|
| `src/shader/include/scale/shader/decode.h` | Codex (T-701, T-708) | Claude (T-711, T-712) | **Frozen** v3 (T-708: vcc_lo/exec_lo, VOPC, branches; additive over v2 `c4ce6f3`) |
| `docs/contracts/shader_abi.md` | Codex (T-704) | Claude (T-711, T-720) | **Frozen** at `d4886a5` (ABI v0) |
| `src/shader/include/scale/shader/test_case.h` | Codex (T-705) | Claude (T-720), Codex (T-706) | **Frozen** (v0 + v1 fields) |

A change to a frozen interface after the freeze needs a coordinator-approved
note in this file.

## Shared-file protocol

The files below are shared. Make only the edits listed.

- `tools/scale/commands.cpp`: replace your command's `nullptr` handler with
  your function and clear its task id. Declare the handler in your own header
  (e.g. `tools/scale/inspect_command.h`) and add your `.cpp` to
  `tools/scale/CMakeLists.txt`.
- `tests/unit/CMakeLists.txt`, `tests/integration/CMakeLists.txt`: append
  your own clearly delimited block. Do not edit other blocks, except to remove
  `cli_unimplemented_command_reports_task` when T-103 lands.
- `docs/TASKS.md`: change only the status of your own tasks.
- `docs/agents/<you>/HANDOFF.md`: yours alone.
- Everything else outside your listed paths, including the root
  `CMakeLists.txt`, `cmake/`, `CMakePresets.json`, `src/common`,
  `src/diagnostics` and `tests/support`, is coordinator-owned. Ask for
  changes in your HANDOFF.md (section "Requests to coordinator").

---

## T-000 — Foundation scaffold (Claude, DONE)

CMake + presets, `scale::build_options`, `scale::common` (Error/Result,
checked arithmetic, ByteView, UTF conversion), `scale::diagnostics` (JSON
writer, deduplicating DiagnosticLog, report.json v1), CLI skeleton, CTest
helpers and harness, environment scripts, documentation. Evidence:
docs/PROGRESS.md.

---

## T-001 — Replace in-tree test harness with GoogleTest (Claude, DONE)

Done: GoogleTest 1.18.0 via FetchContent (URL + SHA-256), see
docs/PROGRESS.md and docs/contracts/testing.md. Original scope:

Required by AGENTS.md (testing policy). Must land before any other task adds
tests, so new tests are written against GoogleTest from the start.

- Obtain a pinned GoogleTest release reproducibly. The method (FetchContent
  with a fixed tag and hash, or a vcpkg manifest) is decided once the user
  finishes installing dependencies. Record the version in docs/PROGRESS.md.
- Delete `tests/support/scale_test.h`, `scale_test_main.cpp`, the skip
  probe and `scale_add_test`'s custom main. `scale_add_test` becomes a thin
  wrapper: link `GTest::gtest_main`, call `gtest_discover_tests` with labels
  and timeout.
- Port the existing `unit_common` and `unit_diagnostics` cases unchanged in
  meaning.
- Prove that `GTEST_SKIP()` is reported as CTest SKIPPED (not PASS) and that a
  failing case fails CTest, with a self-test.
- Keep `scale_add_command_test` for CLI and process-level checks.
- Update `docs/contracts/testing.md`.

**Acceptance:** `ctest --preset full` and `release-full` are green, with the
same coverage as before and per-case CTest entries.

## T-101 — PE32+ inspector library (Claude)

**Paths owned:** `src/loader/` (inspection files: `pe_image.*`, `pe_*`),
`tests/unit/loader/`, the `unit_loader` block in `tests/unit/CMakeLists.txt`.

**Scope:** Bounded, read-only PE32+ AMD64 inspection from documented PE/COFF
structures (https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
No execution, no mapping into memory, no `windows.h` image structures in the
public header. Define your own fixed-layout parsing with `scale::ByteView`.

1. Library `scale::loader` via `scale_add_library(loader SOURCES ... DEPS
   scale::common scale::diagnostics)`.
2. Public API:
   `scale::Result<PeImage> inspect_pe(scale::ByteView file)` and
   `scale::Result<PeImage> inspect_pe_file(const std::filesystem::path&,
   std::uint64_t max_size)`. Reading the file uses a size cap and opens
   read-only.
3. Report: machine, PE32+ magic, characteristics, DLL characteristics,
   subsystem, image base, entry point RVA, section/file alignment, size of
   image/headers, sections (name, VA, virtual size, raw offset/size,
   characteristics), data directories (present/absent, RVA, size), imports
   (DLL name, by-name with hint, or by-ordinal), base relocation summary
   (block count, entry count by type), TLS directory presence (+ callback
   array RVA when present), exception directory presence (+ entry count),
   delay-import directory presence.
4. Validate: DOS header and `e_lfanew` bounds, PE signature, `Machine ==
   IMAGE_FILE_MACHINE_AMD64`, optional header magic `0x20B`, size of optional
   header, number of sections bound, section raw ranges within file, section
   alignment rules, RVA→file-offset mapping with overflow checks, directory
   bounds, import descriptor and thunk walks bounded by limits you declare
   (constants in the header), NUL-terminated names within bounds.
5. Errors: `component = "loader"`, `stage = "pe_parse"`, documented codes
   (e.g. `truncated_dos_header`, `bad_dos_magic`, `bad_pe_signature`,
   `unsupported_machine`, `unsupported_optional_header`, `section_out_of_file`,
   `rva_out_of_image`, `import_table_malformed`, `limit_exceeded`). List them
   in the header.

**Acceptance:** `unit_loader` (label `quick`) builds synthetic images in
memory, using a small test-side PE builder, and covers every success field
and every error code. This includes truncation at each header boundary,
overflowing RVAs/sizes, a PE32 (0x10B) image, an i386 machine, an
unterminated import name and an import descriptor list that exceeds the limit.
`ctest --preset full` is green in Debug and Release.

## T-102 — PE test executables + dumpbin comparison (Claude)

**Paths owned:** `tests/executables/pe/`, the `pe test executables` block in
`tests/integration/CMakeLists.txt`.

Build 2 or 3 tiny self-authored images with CMake: an EXE that imports
`kernel32!ExitProcess` by name, a DLL with exports, and an EXE that imports
that DLL by ordinal. Use `/NODEFAULTLIB` plus an explicit `/ENTRY` where no
CRT is wanted, and keep `/DYNAMICBASE /NXCOMPAT`. Add an integration test that
runs `dumpbin /headers /imports /relocations`, found next to `CMAKE_LINKER`,
and compares entry point RVA, image base, section names/RVAs/sizes and
imported names/ordinals with the inspector output (via a small test
executable that links `scale::loader`). If dumpbin is not found, the test is
SKIPPED, not PASS. **Acceptance:** comparison test passes on this machine.

## T-103 — `scale inspect <file> [--json]` (Claude)

**Paths owned:** `tools/scale/inspect_command.{h,cpp}`, the handler line in
`commands.cpp`, the `inspect` block in `tests/integration/CMakeLists.txt`.

Human-readable output by default. `--json` emits a document via
`scale::diagnostics::JsonWriter` (top-level `schema_version`; document the
schema in `docs/contracts/inspect.md`). On parse failure, print the error
`component/code: message` and exit `1`. Bad usage exits `2`. **Acceptance:**
integration tests run inspect on the T-102 test executables (text and JSON) and on a
malformed file. `cli_unimplemented_command_reports_task` is removed.

---

## T-201 — Worker process supervisor (Claude)

**Paths:** `src/host/`, `tools/scale_worker/`, `tests/unit/host/`, host
block in integration tests. Spawn an unprivileged worker
(`CreateProcessW`, no handle inheritance except the pipes), assign it to a
job object with kill-on-close and a memory limit, enforce a timeout, capture
stdout/stderr (bounded), the exit code and unhandled exception codes. A
worker self-test mode covers exit N, access violation, infinite loop
(timeout) and large output (truncation). **Acceptance:** each mode is tested
and reported as a structured result with diagnostics.

## T-202 — First native test executable, reference run, loader decision (Claude)

Self-authored `test_ret42` with an explicit custom entry contract
(documented), initialized data, no CRT. A native Windows reference run
returns 42. A decision record covers OS loader facilities vs. custom mapping.
Minimum image loading in the worker: section permissions, bounds, entry
invocation. Unsupported directories are rejected explicitly. Consumes the
T-101 inspector.

## T-203 — `scale run`, relocated data, report.json (Claude)

`scale run <executable>` through the worker. A second test executable forces relocation
(image base conflict or `/DYNAMICBASE` with relocated pointers in `.data`).
`--report <path>` writes report.json v1. Comparison against the native
reference.

## T-204 — Missing synthetic import diagnostic (Claude)

A test executable imports `test_platform.dll!TestAllocate` and no resolver
accepts it. Execution must not start. The report shows `outcome = BLOCKED`
and a fatal `runtime/import_resolution/unresolved_import` with `module`,
`symbol`, `importing_module` and `import_kind`. Negative test only; no adapter
is implemented.

## T-601 — Vulkan compute plumbing harness (Claude)

Uses the Vulkan SDK 1.4.363.0 (headers, loader import library, `spirv-val`,
`glslangValidator`). `src/graphics` minimal
instance/device/compute pipeline. Run a known GLSL→SPIR-V compute shader built
at configure time, read it back and verify exact values. Label `gpu`. Report
the device, driver and SDK version. A missing device is SKIPPED.

---

## T-704 — Synthetic shader ABI v0 contract (Codex)

**Paths owned:** `docs/contracts/shader_abi.md`, and later
`src/shader/include/scale/shader/abi.h`.

Define the explicit, synthetic (not Xbox) compute ABI for the first slice:
wave size (state 32 or 64 explicitly), workgroup dimensions, initial
SGPR/VGPR contents (e.g. lane/thread IDs, buffer addressing scheme), how
buffers are bound and addressed (buffer descriptors vs. raw 64-bit addresses,
and how that maps to SPIR-V/Vulkan), output mapping, and the machine-readable
test-case format used by the differential runner (input buffers, seed,
expected outputs, float tolerance rule). Cite the AMD RDNA2 ISA doc and
LLVM AMDGPUUsage sections you rely on. Mark every choice "synthetic ABI
decision", not console behavior. **Acceptance:** Claude reviews and
approves; merged to `integration` before T-702 and T-711 start.

## T-701 — gfx1030 decoder, first subset (Codex)

**Paths owned:** `src/shader/` (decoder files), `tests/unit/shader/`,
`tests/shader/` (test vectors as text), shader blocks in test CMake lists.

Library `scale::shader_decode`, built only with `-DSCALE_ENABLE_SHADER=ON`.
Decode the minimum set needed for a first arithmetic kernel: SOP1/SOP2/SOPK/
SOPP (incl. `s_endpgm`, `s_waitcnt`), VOP1/VOP2/VOP3 integer and f32 adds,
muls and moves, literal constants, inline constants, and the global or buffer
load/store chosen in T-704. Unknown encodings and unsupported fields produce
`shader/decode/unsupported_encoding` (or a more precise code) with the dword
offset and raw words. Never skip an instruction. Freeze `decode.h` first.

**Acceptance:** hand-encoded vectors from the public RDNA2 ISA document
pass. An LLVM cross-check test compares decoder text with `llvm-mc
-triple=amdgcn -mcpu=gfx1030 -show-encoding` / `llvm-objdump`. That test is
SKIPPED when LLVM with AMDGPU is unavailable, and milestone acceptance stays
incomplete until it runs. Record the LLVM version.

## T-702 — Shader IR v0 (Codex)

Small typed IR covering what T-701 decodes. It models EXEC explicitly, plus
SCC/VCC and lane-uniform vs. per-lane values. Lowering errors are explicit.
Unit tests cover IR construction from decoded programs. No SPIR-V yet.

## T-703 — SPIR-V emitter + validation (Codex)

Direct SPIR-V binary emission for the T-702 IR under the T-704 ABI. Validate
with `spirv-val`, which requires the Vulkan SDK. SKIPPED when absent.
Validation is not correctness. GPU execution is T-720.

## T-705 — Shader test-case library (Codex)

**Paths owned:** `src/shader/` (test-case files), `tests/unit/shader/`.

Library `scale::shader_testcase`: loads the T-704 JSON test-case format
strictly. It rejects duplicate, missing or unknown fields, bad values,
unsupported versions and out-of-contract dispatch, with named error codes
(component `shader`, stage `testcase`). Output: a typed struct with code
words, input words, expected words, output type and seed, plus the ABI v1
fields `initial_exec` and `initial_output_words` (T-707). Do not depend on
`shader_ir`, `shader_spirv` or `shader_ref`. T-720 (Claude) and T-706 consume
it. If you want a JSON library instead of a small strict parser, propose it
in your HANDOFF first (AGENTS.md: pinned, justified dependency).
**Acceptance:** the T-704 example loads; every rejection rule has a negative
test. Freeze the public header first (interface freeze), as with `decode.h`.

## T-706 — `scale-shader` CLI (Codex)

**Paths owned:** `tools/scale_shader/`, its test block.

A standalone tool, independent of the native execution host (AGENTS.md):
`scale-shader decode <case.json|words>` prints the decoded instructions,
`scale-shader compile <case.json> -o <out.spv>` runs decode → IR → SPIR-V,
and `scale-shader validate <out.spv>` runs spirv-val if present (SKIPPED
otherwise). Exit codes follow the `scale` CLI convention. **Acceptance:** CLI
tests on the T-704 example and on malformed input.

## T-707 — Shader ABI v1 contract: EXEC masks and control flow (Codex)

**Paths owned:** `docs/contracts/shader_abi.md` (new v1 section).

Contract first, no code. Extend the synthetic ABI to partial EXEC masks
(`v_cmp_*` → VCC, `s_and_saveexec_b32`, `s_mov_b32 exec_lo`), scalar
branches (`s_cbranch_scc0/1`, `s_cbranch_execz`, `s_branch`) and the matching
test-case fields (e.g. initial EXEC or per-case dispatch options). Define
inactive-lane store semantics and how SPIR-V represents divergence. Cite
the RDNA2 ISA and LLVM. Mark every rule as a synthetic ABI decision.
**Acceptance:** Claude reviews it, because T-711 must model the same
semantics. Implementation tasks follow after the freeze.
## T-801 — Portability groundwork (Claude)

Decision 0004. Make `scale::common` portable: a platform-independent UTF-8 ↔
UTF-16 implementation checked against the Win32 conversion and SHA-256
checked against BCrypt and FIPS vectors, or an explicit `_win32.cpp` split
behind the same headers. Add CMake platform gating per component instead of
a root-level Windows-only error, a compiler-flag block for GCC/Clang, and a
shared test helper for starting tools (replacing `_wpopen`/`_wspawn` in
tests). Windows behavior and test results must not change. **Acceptance:**
the Windows suites are unchanged, and portable components contain no OS
headers (checked by a grep-based test).

## T-802 — Linux build/test environment and CI (unassigned, BLOCKED)

Needs a Linux machine or CI runner with Vulkan. Scope: configure presets for
GCC/Clang plus Ninja, build the portable components, run their tests and
the Vulkan GPU tests (SKIPPED without a device). This can't be marked done
without real Linux runs.
## T-708 — Decoder for ABI v1 (Codex)

`decode.h` is frozen, so this starts with an **interface v3 proposal** as a
separate REVIEW step: operand kinds (or an equivalent) for `vcc_lo` (106)
and `exec_lo` (126), `Encoding::vopc`, the new opcodes (`v_cmp_eq_u32`,
`v_cmp_lt_u32`, `s_and_saveexec_b32`, `s_cbranch_scc0/1`, `s_cbranch_execz`,
`s_branch`), and a branch operand that carries the raw SIMM16 and the
computed target dword. Keep `encoded` raw fields so T-712 can check
independently. After the freeze: decoder implementation, LLVM cross-check
coverage for every new form, and negative tests. Branch-target structure
is a program check for the IR and interpreter, not the decoder.

## T-709 — IR + SPIR-V for ABI v1 (Codex)

Lower ABI v1 per `docs/contracts/shader_abi.md`: uniform EXEC/VCC/SCC/PC,
static branch-structure checks (forward only, the single store and end on
every path), a structured block dispatcher, per-lane guards, masked store,
and the workgroup-memory VCC exchange with barriers in uniform control flow.
spirv-val for every test program. GPU execution is T-720.

## T-712 — Reference interpreter for ABI v1 (Claude)

Extend `scale::shader_ref`: `initial_exec`, `initial_output_words`, per-lane
pending loads, the empty-mask rule (a vector instruction with EXEC == 0
reads no sources), VOPC compares writing full VCC, `s_and_saveexec_b32`,
`exec_lo` moves, the four branches with a wave-uniform PC, and the same
static path rules. Hand-computed tests, including both T-707 worked examples.
## T-711 — RDNA2 reference interpreter (Claude)

`src/shader_ref/`, `scale::shader_ref`. Directly interprets decoded
instructions over a modeled wave (SGPRs, VGPRs, EXEC, VCC, SCC, memory)
under the T-704 ABI. It does not depend on IR or emitter code. Hand-computed
expected values are kept per instruction. Inactive-lane behavior and overflow
are tested.

## T-720 — Differential runner (Claude)

One data-driven runner. For each test case it executes the T-704 test case
on the interpreter and on the GPU (SPIR-V from T-703) and compares against
the manual expected values. Exact integer values; float tolerance per the
contract. The device, driver, tools and seed are recorded. Label `gpu`.
