# Codex — handoff notes

Role: RDNA2 shader compiler (decoder, IR, SPIR-V, ABI mapping).

## Start here

1. Read `AGENTS.md`, `docs/MULTI_AGENT.md`, `docs/architecture.md`,
   `docs/contracts/testing.md`, decisions `0003`–`0005`, and the current
   task registry on `integration`.
2. Use the existing Codex worktree. Start each task branch from the current
   `integration` after the coordinator's review checkpoint.
3. Run `. .\scripts\vsenv.ps1`. Use `debug-all` and `release-all` for
   shader and graphics builds.
4. Freeze shader interfaces on `integration` before dependent work.
   The independent interpreter consumes the decoder interface and ABI.

## Constraints

- Tests use GoogleTest 1.18.0 via `scale_add_test` (see
  docs/contracts/testing.md). `GTEST_SKIP()` is reported as SKIPPED.
- Do not depend on loader, host, runtime or graphics.
- Do not put semantic lowering code anywhere `src/shader_ref` could use it.
- Keep shader code and `scale-shader` free of OS headers. Follow decision
  `0004` for neutral interfaces and `_win32.cpp` helpers. T-801 owns the
  shared process helper for tests of portable components.
- Keep the translator free of global state so first-use translation can
  run on a background thread. Decision `0005` places scheduling, cache
  keys, runtime diagnostics and opt-in debug validation in M8.
- LLVM 23.1.6 with AMDGPU is installed (`scripts/build-llvm-amdgpu.ps1`).
  LLVM cross-check tests still use `GTEST_SKIP()` on machines without it,
  never pass.

## Ready for review

### T-731 — Decoder v4 and ABI v2 test-case interfaces

Branch `codex/T-731-decoder-v2` starts from `integration` at `eaeb9eb`.
Decoder interface commit `755af8f` adds `Encoding::smem`,
`Opcode::s_buffer_load_dword` and `ScalarBufferOperands`. It preserves
the raw SDATA, SBASE, SOFFSET and OFFSET fields alongside the resource
tuple's first SGPR index. `BufferOperands` already preserves the MUBUF
immediate and scalar selector. The existing SOPP wait decoder now also
populates `WaitCounters::simm16`. ABI v0 and v1 consumers still require
`vmcnt(0)`. [Decoder interface](../../../src/shader/include/scale/shader/decode.h)

The separate test-case interface commit adds `ShaderTestAbiVersion::v2`
and `ShaderTestCase::v2`. The v2 payload owns the inherited EXEC and
initial output fields in `initial_state`, plus the 32 `constant_words`
in byte-offset order. For v2, consumers use only the v2 payload. The SMEM
source note in the contract now identifies the LLVM 18.1.6 syntax reference
and links the pinned 23.1.6 encoding definition.
[Test-case interface](../../../src/shader/include/scale/shader/test_case.h)

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=OFF -DSCALE_ENABLE_GRAPHICS=OFF
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=OFF -DSCALE_ENABLE_GRAPHICS=OFF
cmake --build --preset release
ctest --preset release-full --output-on-failure
$gtestSource = (Resolve-Path out/build/debug/_deps/googletest-src).Path
cmake --preset debug-all "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtestSource"
cmake --build --preset debug-all
ctest --preset full-all --output-on-failure
cmake --preset release-all "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtestSource"
cmake --build --preset release-all
ctest --preset release-full-all --output-on-failure
ctest --preset gpu -V
$reviewDir = (Resolve-Path out/abi-v2-interface).Path
cl /nologo /std:c++23 /permissive- /W4 /WX /EHsc /I src/common/include /I src/shader/include /c "$reviewDir/interface-check.cpp" "/Fo$reviewDir/interface-check.obj"
& out/build/debug-all/bin/scale-shader.exe decode 0xbf8cc07f
git diff --cached --check
```

- `full` and `release-full` passed 189/189 each. `full-all` and
  `release-full-all` passed 334/334 each. `gpu` passed 14/14 on the
  AMD Radeon RX 9070 XT. All five runs had zero skips. The all-component
  builds reused GoogleTest 1.18.0 after verifying the cached archive hash.
- An MSVC C++23 compile-only consumer checked the v2 SMEM fields against
  the worked dwords, the wait field and unchanged MUBUF field types, the
  constant array extent, and version selection for all three case types.
  The CLI decoded `0xbf8cc07f` as VM 63, EXP 7 and LGKM 0. Logs and
  the compile-only source are under ignored `out/abi-v2-interface/`.
- This is the interface review checkpoint. SMEM decoding, ABI v2 JSON
  parsing, LLVM comparisons and negative tests follow approval.

## Completed

### T-730 — ABI v2 memory and resources

Merged into `integration` at `df37517`. The contract is frozen at `05ba211`.
The contract fixes two raw MUBUF descriptors and a separate read-only
constant resource at set 0, binding 2. It allows computed vector addresses,
aligned immediate offsets, and ordinary SGPR scalar offsets. It rejects
boundary crossings caused solely by `soffset` because AMD 70648's mode 3
description reduces the bound and LLVM's raw intrinsic descriptions exclude
the scalar addend from bounds checks. The interpreter also rejects
duplicate in-range output destinations before dispatch.
[ABI v2 contract](../../contracts/shader_abi.md#abi-v2--memory-and-resources)

The scalar load uses the immediate `s_buffer_load_dword` form with the
opaque `s[8:11]` resource. `s_waitcnt lgkmcnt(0)` settles pending SGPRs
independently of the VMEM wait. The strict JSON case format adds the
required 32-dword `constant_words` array. The worked example covers
an in-range load at byte 124, zero reads, a dropped write, sentinel
preservation, and an ambiguous boundary-crossing rejection.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=OFF -DSCALE_ENABLE_GRAPHICS=OFF
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=OFF -DSCALE_ENABLE_GRAPHICS=OFF
cmake --build --preset release
ctest --preset release-full --output-on-failure
$gtestSource = (Resolve-Path out/build/debug/_deps/googletest-src).Path
cmake --preset debug-all "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtestSource"
cmake --build --preset debug-all
ctest --preset full-all --output-on-failure
cmake --preset release-all "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtestSource"
cmake --build --preset release-all
ctest --preset release-full-all --output-on-failure
ctest --preset gpu -V
llvm-mc --version
llvm-objdump --version
# memory.s is the worked example's assembly block.
$reviewDir = 'out/abi-v2-review'
llvm-mc -triple=amdgcn -mcpu=gfx1030 -mattr=+wavefrontsize32 -show-encoding "$reviewDir/memory.s"
llvm-mc -triple=amdgcn -mcpu=gfx1030 -mattr=+wavefrontsize32 -filetype=obj -o "$reviewDir/memory.o" "$reviewDir/memory.s"
llvm-objdump --disassemble --mcpu=gfx1030 --mattr=+wavefrontsize32 "$reviewDir/memory.o"
python out/abi-v2-review/check-example.py
git diff --cached --check
```

- `full` and `release-full` passed 189/189 each. `full-all` and
  `release-full-all` passed 334/334 each. `gpu` passed 14/14 on the
  AMD Radeon RX 9070 XT (Vulkan 1.4.359, AMD proprietary driver
  26.10.1, raw `0x008001a0`). Zero skips across the five runs.
  The all-component builds reused the extracted GoogleTest 1.18.0 source
  after checking the cached archive against the CMake pin.
- LLVM 23.1.6 assembled and disassembled all 11 documented dwords
  identically. Additional assembly probes covered MUBUF `offset:4092`,
  `v255` addressing and `s105` as `soffset`, the scalar 0/124-byte
  endpoints, and the encodings for excluded scalar and combined wait
  forms. The zero-byte scalar immediate disassembles as `null`.
- The scratch Python check extracted the example, compared all 11
  dwords with both tools, and checked all 32 hand-derived output words
  and the lane 29 crossing. Logs and scratch files are under ignored
  `out/abi-v2-review/`. The GPU results cover existing ABI v0 and v1
  cases. ABI v2 GPU readback is T-734.

### T-706 — Standalone shader CLI

Merged into `integration` at `acc5731`. The approved root hook builds `scale-shader` with
`-DSCALE_ENABLE_SHADER=ON`. `decode` prints dword offsets, raw words,
operands and resolved branch targets from JSON cases or inline dwords.
`compile` uses `lower_ir` for ABI v0 and `lower_ir_v1` with the case's
initial EXEC for ABI v1, then publishes the module by rename.
[CLI usage](../../../tools/scale_shader/README.md)

`validate` looks on PATH and then under `VULKAN_SDK` for `spirv-val`.
It runs the tool directly for Vulkan 1.2 and passes the module through
stdin so Unicode filenames work with the installed Windows validator.
An unavailable validator prints `SKIPPED` and exits 1. Tests that require
the tool are marked Skipped by CTest in that environment.
[Windows process API](https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-createprocessw)

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full --output-on-failure
cmake --preset debug -B out/build/t706-standalone -DSCALE_ENABLE_SHADER=ON -DSCALE_ENABLE_GRAPHICS=OFF -DSCALE_BUILD_TESTS=OFF
cmake --build out/build/t706-standalone --target scale-shader
& out/build/t706-standalone/bin/scale-shader.exe decode 0xbf810000
```

- Debug and Release each passed all 304 tests. The 18 CLI tests include
  T-704, both T-707 cases, distinct binaries for distinct initial EXEC
  masks, a Unicode output path, malformed input and the missing-tool outcome.
- Added `v1_saveexec_scc_branch` to `spirv_val_test.cpp`. All six ABI v1
  case modules passed `spirv-val` with Vulkan SDK 1.4.363.0.
- With PATH limited to the test build directory and `VULKAN_SDK` cleared,
  the two selected external validation tests reported Skipped.
- The standalone build and inline decode passed with graphics and tests
  disabled. Linux execution remains unverified.

### T-709 — ABI v1 IR and SPIR-V

Merged into `integration` at `8e6e25e` (implementation `bd4c147`).
The lowering checks every decoded operand form, including
unreachable code, and resolves forward branch targets to basic blocks.
All paths reach the one store immediately before `s_endpgm`. The emitter
uses a workgroup-uniform loop and switch, private register slots that
survive joins, and structured EXEC guards around lane operations. Each
compare combines per-lane bits through a 32-dword workgroup array with a
barrier before reading and another before reuse.
[Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)

For T-721 and T-706, call `lower_ir_v1(decoded, case.v1->initial_exec)`
for ABI v1, check the result, then call the `emit_spirv(const IrProgramV1&)`
overload. `initial_exec` is compiled into the module, including when zero.
The runner uploads `case.v1->initial_output_words` in the existing output
binding and checks executed register reads, waits and floating-point
domains on the independent interpreter path before dispatch. ABI v0 uses
`lower_ir(decoded)` and `emit_spirv(const IrProgram&)`.

The two T-707 cases and the SCC/unconditional branch case come from T-721.
Added `v1_exec_restore_compare` for repeated VCC exchanges, a compare
under empty EXEC, and VGPR preservation after restoration. Added
`v1_zero_exec_f32` for a uniform branch on the zero entry mask followed by
activation and masked floating-point arithmetic.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full --output-on-failure
$gtest_source = (Resolve-Path out/build/debug/_deps/googletest-src).Path
cmake --preset debug-all "-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=$gtest_source"
cmake --build --preset debug-all --target gpu_shader_differential
ctest --preset gpu -V -R 'gpu_shader_differential.*(t707_|v1_)'
```

- Shader-enabled Debug and Release passed 285 tests each with zero skips.
  All five ABI v1 JSON programs passed `spirv-val --target-env vulkan1.2`
  (SPIRV-Tools 2026.4, Vulkan SDK 1.4.363.0). The portable-header check passed.
- LLVM 23.1.6 assembled the documented T-707 program for gfx1030 wave32.
  Its disassembly dwords matched both existing T-707 JSON files.
- At review, the interpreter matched all five ABI v1 golden outputs.
  T-721 subsequently wired ABI v1 GPU execution and recorded 10/10 matches
  on the RX 9070 XT across both ABI versions. [Verification record](../../PROGRESS.md)

### T-708 — Decoder implementation

Merged into `integration` at `21471e9` (implementation `06c2c4a`). The
decoder handles ABI v1 VOPC compares, scalar mask
save and restore, and all four SOPP branches. The VOPC literal selector
consumes one extra dword. The second source uses its full 8-bit VGPR index,
including 255. Scalar mask forms allow `vcc_lo` only as a source for
`s_and_saveexec_b32` and `s_mov_b32 exec_lo`. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)

The decoder returns signed branch targets even when they precede the
stream, leave the stream, or land in a literal. IR and T-712 own the
synthetic ABI branch-layout checks. The public header matches the
interface frozen at `a3c4a2d` byte for byte.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug --target unit_shader_decode integration_shader_llvm
ctest --preset full -R 'unit_shader_decode|integration_shader_llvm|portable_components_have_no_os_headers' --output-on-failure
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full --output-on-failure
git diff a3c4a2d --exit-code -- src/shader/include/scale/shader/decode.h
git diff --check
```

- The focused Debug run passed all 40 checks. The full Debug and Release
  suites each passed 259 tests with zero skips, including the
  portable-header and SPIR-V validation checks.
- The new LLVM 23.1.6 cross-check assembles 28 wave32 instructions in
  `tests/shader/decoder_v1_subset.s` and compares disassembly text,
  offsets, raw words and the resolved branch labels.
- Hand-encoded tests cover the raw operand fields, signed displacement
  limits, and all four decoder error codes for ABI v1 forms.

### T-708 — Decoder interface v3

Approved and frozen on `integration` at `a3c4a2d`.
[decode.h](../../../src/shader/include/scale/shader/decode.h)
adds the ABI v1 opcodes, `Encoding::vopc`, and special register kinds for
`vcc_lo` (106) and `exec_lo` (126). `CompareOperands` carries both
encoded source fields. The VOPC opcode implies the `vcc_lo` destination.
`BranchOperands` carries the raw `SIMM16` bits and the signed target
dword computed from the next instruction. The signed type lets IR and
T-712 diagnose a target before the start of the stream. Target structure
is checked by the consumers under the synthetic ABI.

The compiler and LLVM comparison test route the new enum values through
their existing error paths until ABI v1 support lands. Commit `956d32c`
switches the `spirv-val` tests to `scale::test_tools`, checks the exit
code and reports combined tool output on a validation failure.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full --output-on-failure
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full --output-on-failure
```

- Debug and Release each passed all 243 tests with zero skips after the
  rebase. This includes both LLVM 23.1.6 comparisons, all three
  validation tests with Vulkan SDK 1.4.363.0, and the portable-header check.
- An earlier parallel Debug and Release run hit two native execution failures.
  Both cases passed on serial reruns before and after the rebase.
- Windows GCC 13.2 syntax checks passed for the compiler and LLVM test
  with `-Wswitch -Werror=switch`. The same check identified unhandled
  enum values in `check_source()` and `Interpreter::step()` in the
  reference interpreter. Linux builds are unverified.

### T-705 — Loader implementation

Approved and merged into `integration` at `0dbd2c5` (implementation
`83e403d`). `scale::shader_testcase` implements the frozen v0/v1 API using
a schema-specific parser with no JSON dependency. It validates UTF-8,
decodes string escapes before matching keys, and accepts fields in any
order. The file loader checks the caller's byte cap as it reads. The
translator, reference interpreter and graphics runtime are independent of
this library. [JSON syntax and Unicode rules](https://www.rfc-editor.org/rfc/rfc8259.html)

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
git diff --exit-code 78c2ee9 -- src/shader/include/scale/shader/test_case.h
git diff --cached --check
```

- Debug and Release each passed all 239 tests with zero skips. The 11
  loader cases cover the T-704 and T-707 examples, zero EXEC, escaped and
  UTF-8 names, and negative checks for malformed JSON and UTF-8, every
  required field, duplicate and unknown fields, unsupported versions and
  dispatch, invalid field values, and the file cap.
- The directory-path test accepts an open or read error because the point
  of failure depends on the host's stream implementation. The missing
  file and size-limit tests assert their exact error codes.

### T-705 — Test-case interface freeze

Approved and frozen on `integration` at `78c2ee9`.
The public [test_case.h](../../../src/shader/include/scale/shader/test_case.h)
defines owning types for the v0 and v1 JSON case format. The v1-only
`initial_exec` and `initial_output_words` are one optional record. Its
presence selects ABI v1, including with an explicit zero EXEC mask.
The header declares a UTF-8 parser and a file loader with a caller-supplied
byte limit. It names the schema, dispatch and file errors for reporting at
stage `testcase`. With `SCALE_ENABLE_SHADER=ON`, the interface target
`scale::shader_testcase` exports the header and `scale::common`. The loader
and parser definitions are the implementation increment after the freeze.

```powershell
git fetch origin integration
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
cmake --build out/testcase-interface-freeze/build --target shader_testcase_interface_check
git diff --cached --check
```

- Local `integration` contained the fetched tip and four newer commits,
  including the ABI v1 contract. Debug and Release each passed all 228
  existing tests with zero skips.
- A scratch CMake consumer compiled against `scale::shader_testcase` and
  checked the parser and loader signatures, the 32-word lane array, and
  the paired v1 fields.

### T-707 — Shader ABI v1 contract

Approved and merged into `integration` at `d93cf1e`.
The [ABI v1 section](../../contracts/shader_abi.md#abi-v1--exec-masks-and-scalar-control-flow)
defines wave32 EXEC and VCC updates, a forward-only scalar control-flow
graph, lane-level pending loads, and output words preserved by inactive
lanes. The case format adds required `initial_exec` and
`initial_output_words` fields for `abi_version: 1`. Vulkan lowering keeps
scalar control uniform and forms the compare mask within the workgroup.

```powershell
git fetch origin integration
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
# The ABI v1 assembly block was extracted to out/abi-v1-review/partial.s.
llvm-mc -triple=amdgcn -mcpu=gfx1030 -mattr=+wavefrontsize32 -filetype=obj -o out/abi-v1-review/partial.o out/abi-v1-review/partial.s
llvm-objdump --disassemble --mcpu=gfx1030 --mattr=+wavefrontsize32 out/abi-v1-review/partial.o
git diff --cached --check
```

- The fetched integration tip matched the local starting point. Debug and
  Release each passed all 212 CTest cases with zero skips.
- LLVM 23.1.6 assembled and disassembled the example. The `s_cbranch_execz`
  at byte offset `0x08` has displacement 4 dwords and targets the store at
  `0x1c`. The separate restore probe checked `v_cmp_eq_u32_e32`,
  `s_cbranch_scc0`, `s_cbranch_scc1`, `s_branch`, and `s_mov_b32 exec_lo`.
- T-707 is a contract review. The runtime semantics and GPU lowering of
  ABI v1 will be verified by their implementation tasks after the freeze.

### T-703 — SPIR-V emitter and validation

Approved and merged into `integration` at `ba5792c`.
`scale::shader_spirv` writes SPIR-V 1.5 words directly for Vulkan 1.2. The
public [spirv.h](../../../src/shader/include/scale/shader/spirv.h) exposes
`emit_spirv(IrProgram)` and a module with the `requires_float32_rte` flag.
The Vulkan caller uses the `main` compute entry point and the set 0
storage-buffer layout defined by T-704. For floating-point arithmetic the
emitter selects `RoundingModeRTE 32` and decorates each add and multiply with
`NoContraction`. T-720 checks `shaderRoundingModeRTEFloat32` before dispatching
modules with that flag. [Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html) [Vulkan SPIR-V environment](https://docs.vulkan.org/spec/latest/appendices/spirvenv.html)

Emission checks that EXEC is all-active, operands refer to earlier typed
values, buffer accesses use the ABI lane byte offset, and loads are covered
by a wait. `wait_vmem` is represented by synchronous SPIR-V load dependencies
under T-704. The IR now has space for three input IDs, and lowering errors
include the decoded opcode name. The compiler and its unit tests build
without SDK headers or libraries. `spirv-val` is an integration-test tool.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
spirv-val --version
spirv-val --target-env vulkan1.2 out/build/debug/tests/unit/shader/t703_add_lane.spv
spirv-val --target-env vulkan1.2 out/build/debug/tests/unit/shader/t703_float.spv
spirv-val --target-env vulkan1.2 out/build/debug/tests/unit/shader/t703_scalar.spv
spirv-dis out/build/debug/tests/unit/shader/t703_float.spv
cmake --preset debug -B out/build/shader-no-spirv-val -DSCALE_ENABLE_SHADER=ON `
    -DSCALE_SHADER_SPIRV_VAL=missing-spirv-val `
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=out/build/debug/_deps/googletest-src
cmake --build out/build/shader-no-spirv-val --target unit_shader_spirv integration_shader_spirv_val
ctest --test-dir out/build/shader-no-spirv-val -R 'shader_spirv' --output-on-failure
git diff --cached --check
```

- Debug and Release each passed 200/200 CTest cases with zero skips.
- SPIRV-Tools v2026.4 (`ef96ed76`, Vulkan SDK 1.4.363.0) validated the T-704
  integer example, a binary32 add/multiply test program, and a scalar-carry
  and vector-copy test program. Float disassembly confirmed the RTE mode,
  both `NoContraction` decorations, and the input/output bitcasts.
- With `spirv-val` unavailable in the isolated configure, seven emitter
  unit tests passed and three validation tests were SKIPPED. GPU execution
  and differential readback are T-720.

### T-702 — Shader IR v0

Approved and merged into `integration` at `74b5f0d`.
`scale::shader_ir` lowers the frozen decoder output to a typed, straight-line
IR under T-704 ABI v0. Each value is named by its defining instruction index.
The entry defines the all-active EXEC mask, zero VCC and clear SCC, plus lane
index and byte-offset values. Guest vector operations refer to EXEC and
broadcast uniform operands when needed. A scalar unsigned add defines the
next SCC as the wrapping sum's unsigned carry. Float operations use binary32
values and explicit bitcasts at raw-word boundaries. The independent
reference interpreter continues consuming the decoder. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)

The public IR is defined in [ir.h](../../../src/shader/include/scale/shader/ir.h).
The ordered IR retains input loads, waits, the final output store and the
terminator. Lowering validates descriptor use, address operands, register
definition, pending loads and stream termination. Failures use documented
error codes with the instruction dword offset where available. The caller
checks test case metadata and the data-dependent float domain.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
git diff --exit-code 3f642f0 -- src/shader/include/scale/shader/decode.h src/shader_ref
git diff --cached --check
```

- Debug and Release each passed 190/190 CTest cases with zero skips on
  Windows x64, MSVC 19.52.37029 and Windows SDK 10.0.26100.0.
- The 10 new IR cases use decoded programs, including the exact T-704
  example. They check the IR graph, scalar carry, vector broadcast, float
  bitcasts, the VMEM wait, and all six lowering error codes.

### T-701 — gfx1030 decoder

Approved and merged into `integration` at `3f642f0`.
`scale::shader_decode` is a static library enabled by `SCALE_ENABLE_SHADER=ON`.
It decodes all 12 opcodes in the frozen `decode.h` interface, including e32
and e64 ALU forms, inline and literal constants, wait counters and the OFFEN
MUBUF dword forms. Decoder errors carry the dword offset and raw words read
for the instruction. The caller validates the T-704 ABI, including fixed
registers and termination. DPP, SDWA, special-register operands, VOP3
modifiers and other MUBUF address modes are outside this subset. Encoding
reference is AMD document 70648, sections 5.2, 6.2, 12.5, 12.7.1, 12.8.1 and
chapter 13. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)

Verification on Windows x64, MSVC 19.52.37029, Windows SDK 10.0.26100.0.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
ctest --preset full -R '^integration_shader_llvm[.]' -V
ctest --preset full -N -R 'AluDecode'
cmake --preset debug -B out/build/shader-no-llvm -DSCALE_ENABLE_SHADER=ON `
    -DSCALE_SHADER_LLVM_MC=missing-llvm-mc `
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=out/build/debug/_deps/googletest-src
cmake --build out/build/shader-no-llvm --target integration_shader_llvm
ctest --test-dir out/build/shader-no-llvm -R '^integration_shader_llvm[.]' --output-on-failure
git diff --exit-code c4ce6f3 -- src/shader/include/scale/shader/decode.h
git diff --cached --check
```

- Debug and Release each passed 121/121 CTest cases with zero skips.
  There are 21 decoder unit cases, with hand-encoded positive coverage for
  every opcode and negative coverage for all four error codes. CTest lists
  the 13 parameterized ALU cases by name.
- LLVM 23.1.6 `llvm-mc -triple=amdgcn -mcpu=gfx1030 -filetype=obj` assembled
  the T-704 example and `tests/shader/decoder_subset.s` at configure time.
  `llvm-objdump --disassemble --mcpu=gfx1030` produced both listings. In both
  configurations GoogleTest matched text, offsets and raw words for all 33
  subset instructions (55 dwords) and all five T-704 instructions (seven
  dwords). The T-704 words also match the ABI contract. The subset test program
  exercises all 12 opcodes, the e32/e64 forms, register and constant sources,
  signed immediates, wait counters and MUBUF load/store addressing fields.
- Changing `0.5` to `0.75` in the generated LLVM listing failed the subset
  comparison. Restoring the listing returned both comparisons to PASS.
- An isolated configure with a missing `llvm-mc` path reported both LLVM
  cases as SKIPPED in CTest. The frozen header is byte-identical to `c4ce6f3`.

### T-701 — decoder interface freeze v2

Commits `56e60a6` and `c2a0ce1` merged and frozen at `c4ce6f3`. The header
preserves both the raw operand field and its resolved value, with room for
three ALU sources. A scratch CMake consumer checked the interface and Debug
and Release each passed 83/83 baseline cases on the freeze branch.

### T-704 — Synthetic shader ABI v0

Commit `b629d89` from `codex/T-704-shader-abi`, approved and merged.
The [contract](../../contracts/shader_abi.md) defines one logical wave32,
the initial registers, two opaque MUBUF resource tuples, their Vulkan storage
buffer bindings, and the JSON test-case format. The first program uses
`buffer_load_dword` and `buffer_store_dword` with `offen`, a lane-local byte
offset and zero scalar and immediate offsets. Floating-point cases use a
finite-normal domain and bit-exact comparison after round-to-nearest-even.

Verification on Windows x64, MSVC 19.52.37029, Windows SDK 10.0.26100.0:

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug
cmake --build --preset debug
ctest --preset full
llvm-mc --version
# Pipe the contract's asm block to the assembler.
$assembly | llvm-mc -triple=amdgcn -mcpu=gfx1030 -show-encoding
git diff --cached --check
```

- Debug configured and built. CTest passed all 35 cases (30 quick and 5
  integration), with zero skips.
- LLVM 23.1.6 registered AMDGPU. A PowerShell check extracted the Markdown
  assembly and JSON blocks, compared all seven assembled dwords to
  `code_words`, and verified all 32 golden outputs using unsigned addition
  modulo 2^32. Both checks passed.
- This increment is documentation-only. Release, SPIR-V validation and GPU
  execution were outside the verification performed for this contract.

## Next

After approval of both T-731 interfaces, implement SMEM decode and strict ABI
v2 JSON parsing. Test the new forms against LLVM 23.1.6, cover unsupported
operands and malformed JSON, and add the T-730 worked example under
`tests/shader/cases/`. A `scale-shader compile` request for ABI v2 must
report an unsupported ABI until T-732 provides v2 lowering.

## Requests to coordinator

- Review the two T-731 interfaces and update the freeze entries on
  `integration` once approved.
- Before the worked example lands, update the GPU runner in
  `tests/gpu/differential_test.cpp` to recognize ABI v2 explicitly. It
  discovers every JSON case and currently routes every ABI other than v1
  through v0. The v2 case needs a SKIPPED result until T-734 can run it.
