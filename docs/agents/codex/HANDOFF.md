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

### T-701 — REVIEW

Review branch `codex/T-701-decoder-impl`, based on `integration` at `b1fb7a7`.
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
cmake --preset debug -B out/build/shader-no-llvm -DSCALE_ENABLE_SHADER=ON `
    -DSCALE_SHADER_LLVM_MC=missing-llvm-mc `
    -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=out/build/debug/_deps/googletest-src
cmake --build out/build/shader-no-llvm --target integration_shader_llvm
ctest --test-dir out/build/shader-no-llvm -R '^integration_shader_llvm[.]' --output-on-failure
git diff --exit-code c4ce6f3 -- src/shader/include/scale/shader/decode.h
git diff --cached --check
```

- Debug and Release each passed 120/120 CTest cases with zero skips.
  There are 21 decoder unit cases, with hand-encoded positive coverage for
  every opcode and negative coverage for all four error codes.
- LLVM 23.1.6 `llvm-mc -triple=amdgcn -mcpu=gfx1030 -filetype=obj` assembled
  the T-704 fixture at configure time. `llvm-objdump --disassemble --mcpu=gfx1030`
  produced the reference listing. GoogleTest matched all five instruction
  texts, five offsets and seven dwords in both configurations.
- An isolated configure with a missing `llvm-mc` path reported the LLVM
  case as SKIPPED in CTest. The frozen header is byte-identical to `c4ce6f3`.

## Completed

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

After the T-701 implementation is merged and T-702 is ready on `integration`,
start the shader IR on its own branch. T-711 consumes the frozen decoder
interface and T-704 contract independently of the IR.

## Requests to coordinator

- Review the T-701 implementation branch for integration.
