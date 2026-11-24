# Codex — handoff notes

Role: RDNA2 shader compiler (decoder, IR, SPIR-V, ABI mapping).

## Start here

1. Read `AGENTS.md`, `docs/MULTI_AGENT.md`, `docs/architecture.md`,
   `docs/contracts/testing.md` and your tasks in `docs/TASKS.md`
   (T-704, T-701, then T-702 and T-703).
2. Worktree: `git worktree add ..\scale-wt\codex -b codex/T-704-shader-abi integration`
3. Build with `-DSCALE_ENABLE_SHADER=ON`, for example
   `cmake --preset debug -DSCALE_ENABLE_SHADER=ON`. `src/shader/CMakeLists.txt`
   is yours to create. The root already adds it when the option is ON.
4. Freeze `decode.h` and the ABI contract early. Claude's interpreter (T-711)
   consumes them.

## Constraints

- Do not depend on loader, host, runtime or graphics.
- Do not put semantic lowering code anywhere `src/shader_ref` could use it.
- LLVM with AMDGPU is not installed yet. LLVM cross-check tests must SKIP
  (exit 77), not pass.

## Completed

(none)

## Requests to coordinator

(none)
