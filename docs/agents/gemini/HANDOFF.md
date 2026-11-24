# Gemini — handoff notes

Role: PE inspection (M1), PE fixtures, `scale inspect`.

## Start here

1. Read `AGENTS.md`, `docs/MULTI_AGENT.md`, `docs/architecture.md`,
   `docs/contracts/diagnostics.md`, `docs/contracts/testing.md` and your
   tasks in `docs/TASKS.md` (T-101, then T-102 and T-103).
2. Worktree: `git worktree add ..\scale-wt\gemini -b gemini/T-101-pe-inspector integration`
3. Build: `. .\scripts\vsenv.ps1; cmake --preset debug; cmake --build --preset debug; ctest --preset full`
4. Create `src/loader/CMakeLists.txt` using `scale_add_library(loader ...)`.
   The root already adds `src/loader` once it exists.
5. Freeze `scale/loader/pe_image.h` first, as a small reviewable commit.
   Claude's native loader (T-202) consumes it.

## Constraints

- Parse only through `scale::ByteView` and `scale/common/checked.h`.
  Do not cast raw pointers to Windows image structs.
- Never execute or map the inspected file.
- Every error code needs a negative test.

## Completed

(none)

## Requests to coordinator

(none)
