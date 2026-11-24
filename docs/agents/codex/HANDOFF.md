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

### T-701 — interface freeze v2 ready

Review branch `codex/T-701-decoder`, based on `integration` at `016d6fd`.
The public [decoder interface](../../../src/shader/include/scale/shader/decode.h)
declares `decode(ByteView)` and the owned instruction representation. It
preserves encoding, raw dwords and stream offsets, with typed payloads for
ALU operands, wait counters and MUBUF operands. `AluOperands` has room for
three sources and retains `source_count`. `Operand::encoded` preserves the
raw field alongside the resolved value so the interpreter can independently
verify inline constants. The interpreter and compiler can consume the same
decoded stream and validate the synthetic ABI themselves.
With `SCALE_ENABLE_SHADER=ON`, `scale::shader_decode` is a CMake interface
target that exports the header and `scale::common`. The implementation will
turn it into a static library with the same target name. `decode()` is a
declaration at this checkpoint.

Verification on Windows x64, MSVC 19.52.37029, Windows SDK 10.0.26100.0.

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug -DSCALE_ENABLE_SHADER=ON
cmake --build --preset debug
ctest --preset full
cmake --preset release -DSCALE_ENABLE_SHADER=ON
cmake --build --preset release
ctest --preset release-full
cmake --build out/interface-freeze/build --target shader_decode_interface_check
git diff --cached --check
```

- Debug and Release each passed all 83 existing CTest cases with zero skips.
- The scratch CMake consumer under ignored `out/interface-freeze/` compiled
  with `scale::shader_decode`. It checked the `decode()` signature, the
  three-source array and count, and the raw field's type and ability to
  retain 9-bit and 16-bit values alongside resolved values.
- `llvm-mc -triple=amdgcn -mcpu=gfx1030 -show-encoding` accepted the proposed
  opcode forms, including e32/e64 moves and adds, `v_mul_lo_u32`, and both
  T-704 MUBUF instructions. Decoder behavior and the LLVM disassembly
  comparison belong to the implementation increment.

## Completed

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

After the coordinator merges the v2 interface, start the implementation on
a new branch from `integration`. Implement the first subset and GoogleTest
cases, including hand-encoded vectors and a cross-check of the T-704 example
using both `llvm-mc` and `llvm-objdump`. T-711 can start from the frozen
header and the merged ABI contract.

## Requests to coordinator

- Review and freeze `decode.h` on `integration` for T-711.
