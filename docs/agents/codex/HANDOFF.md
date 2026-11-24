# Codex — handoff notes

Role: RDNA2 shader compiler (decoder, IR, SPIR-V, ABI mapping).

## Start here

1. Read `AGENTS.md`, `docs/MULTI_AGENT.md`, `docs/architecture.md`,
   `docs/contracts/testing.md` and your tasks in `docs/TASKS.md`
   (T-704, T-701, then T-702 and T-703).
2. Use the existing Codex worktree. Start each task branch from the current
   `integration` after the coordinator's review checkpoint.
3. Build with `-DSCALE_ENABLE_SHADER=ON`, for example
   `cmake --preset debug -DSCALE_ENABLE_SHADER=ON`. `src/shader/CMakeLists.txt`
   is yours to create. The root already adds it when the option is ON.
4. Freeze `decode.h` and the ABI contract early. Claude's interpreter (T-711)
   consumes them.

## Constraints

- Tests use GoogleTest 1.18.0 via `scale_add_test` (see
  docs/contracts/testing.md). `GTEST_SKIP()` is reported as SKIPPED.
- Do not depend on loader, host, runtime or graphics.
- Do not put semantic lowering code anywhere `src/shader_ref` could use it.
- LLVM 23.1.6 with AMDGPU is installed (`scripts/build-llvm-amdgpu.ps1`).
  LLVM cross-check tests still use `GTEST_SKIP()` on machines without it,
  never pass.

## Ready for review

### T-703 — REVIEW

Review branch `codex/T-703-spirv`, based on `integration` at `e912520`.
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

## Completed

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

T-720 will use `emit_spirv` for the Vulkan run and compare measured outputs
with the T-711 interpreter and manual expectations. T-711 consumes the
frozen decoder interface and T-704 contract independently of the compiler.

## Requests to coordinator

- Review the T-703 branch and confirm `spirv.h` for the T-720 runner.
