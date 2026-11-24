# Claude — handoff notes

Role: architect and integration coordinator (sole merger to `integration`
and `main`), CPU/runtime, reference interpreter, Vulkan backend,
differential GPU verification.

Last updated: 2026-11-24, after the T-730 merge (ABI v2 contract frozen at
`05ba211`). Pushes need the maintainer's explicit request.

## State of the project

Current state per track: the table at the top of docs/PROGRESS.md. Tasks
and owners: docs/TASKS.md. Milestone states: docs/ROADMAP.md.

Summary: M0–M2 done. M3: T-301 (exception directory registration) done.
M7: ABI v0 and v1 verified on the RX 9070 XT (10 JSON cases), `scale-shader`
CLI merged. Suites: `full`/`release-full` 189/189,
`full-all`/`release-full-all` 334/334, `gpu` 14/14, no skips.

## Where to resume

**Waiting for Codex:** T-731, the ABI v2 decoder and test-case fields. Its
first step is an interface freeze (`decode.h` v4, `test_case.h` v2) as a
separate REVIEW. When the maintainer says it is in review: check the new
operand/encoding forms keep raw `encoded` fields (the interpreter re-derives
them), merge the freeze, then start T-733. Do not start T-302 until the
maintainer says so.
## My next tasks, in order

1. **T-733** — reference interpreter for ABI v2 (contract approved; starts
   at the T-731 interface freeze). Model the worked example and its three
   rejection variants (lane 29 straddle, missing scalar wait, empty-EXEC
   pending `s12` at the store), store collisions and scalar pending state.
2. **T-734** — ABI v2 GPU differential cases (after T-732).
3. **T-302** — TLS directory (static TLS, callbacks), spec in TASKS.md;
   paused until the maintainer says so.
4. Remaining M3 features one at a time (import binding through adapters,
   CRT startup, DLL init), each with its own test executable.
5. Graphics triangle slice (M6).

Review queue: Codex T-731 (interface freeze first).

## Decisions to remember

- 0001 layout. 0002 custom mapping in the worker, OS loader as reference;
  the exception directory is now validated and registered (T-301). 0003
  **Vulkan + SPIR-V** (maintainer choice; revisit at M8 only with verified
  D3D12.X evidence). 0004 Windows first, Linux-ready: no OS headers in
  portable components, OS code in `_win32.cpp`, no Linux claims without
  Linux runs. 0005 translate shaders on first use, disk cache keyed by
  binary/ABI/translator/backend, runtime diagnostics + opt-in debug mode.
- Resolver policy: adapter → approved guest module → permitted Windows
  export → unresolved. **Never** auto-stub unknown imports or fake success.
- Terminology: "executable" / "test executable", never "fixture". **SCALE**
  in prose, identifiers lowercase. No "games" in README.
- Commit, merge and PR messages: task-id subject, no AI or self attribution.
- Retail material: only authorized, lawfully obtained executables, and only
  after the M10 gate. A leaked retail folder was proposed once and declined;
  never use it.

## Coordinator workflow I follow

- Rules in `.claude/rules/`: docs up to date after larger blocks, push only
  on explicit request, a Codex prompt after every review.
- Every task gets a branch `claude/<task>` from `integration`. Stage explicit
  paths, never `git add -A`.
- Review Codex: inspect the branch (`git log`, `git diff --stat`,
  `git merge-tree --write-tree`), read the diff, `git merge --no-ff
--no-commit` on `integration`, build `debug`, `release`, `debug-all`,
  `release-all`, run `full`, `release-full`, `full-all`, `release-full-all`,
  `gpu`, plus independent checks (llvm-mc/objdump, spirv-val, GPU
  differential runner) and mutation checks. An uncaught mutation means a
  missing test: add one. Then commit the merge, fast-forward `main`, delete
  the merged branch once Codex has switched away from it.
- Shared-file conflicts (`tests/*/CMakeLists.txt`): keep both blocks.
  `docs/TASKS.md` conflicts: keep ours, reapply statuses.
- Approved exception: Codex's single root `CMakeLists.txt` line for
  `tools/scale_shader` (T-706, recorded in TASKS.md).

## Environment notes

- Always `. .\scripts\vsenv.ps1`, which reloads PATH from the registry and
  puts VS CMake/Ninja ahead of Strawberry Perl's.
- Tools: MSVC 19.52, SDK 10.0.26100, CMake 4.3.1, Ninja 1.13.2, Vulkan SDK
  1.4.363.0, LLVM 23.1.6 AMDGPU (`scripts/build-llvm-amdgpu.ps1`, installed in
  `%LOCALAPPDATA%\Programs\LLVM-AMDGPU\23.1.6\bin`). GPU: RX 9070 XT,
  Vulkan 1.4.359, AMD 26.10.1, subgroup 64 (range 32–64).
- Pushes can be blocked by the permission check; then ask the maintainer to
  run `! git push origin main integration`.
- Shell pitfalls: Bash heredocs halve backslashes (use Write/Edit or
  PowerShell `[IO.File]`); in PowerShell nested arrays flatten (use an
  ordered hashtable for mutation lists); here-strings can drop the trailing
  newline (check joins); keep LF line endings.
- Windows `RtlAddFunctionTable` keeps a pointer to the caller's table (no
  copy); a stale registration reads freed memory. Covered by the
  `unit_image_map` reuse test.

## Open items

- `.claude/rules/push-only-on-request.md` and
  `.claude/rules/codex-prompt-after-review.md` are written but not
  committed (commit with the next task if the maintainer wants them in the
  repo).
- This handoff update is not committed yet.
- Codex's worktree is on the merged `codex/T-706-shader-cli`; delete that
  branch once Codex switches to `codex/T-730-shader-abi-v2`.

## Requests to coordinator

(none — this is the coordinator)
