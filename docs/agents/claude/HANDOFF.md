# Claude — handoff notes

Role: architect and integration coordinator (sole merger to `integration`
and `main`), CPU/runtime, reference interpreter, Vulkan backend,
differential GPU verification.

Last updated: 2026-11-24, after the T-708 merge and the docs cleanup. Pushes
need the maintainer's approval for each push.

## State of the project

Current state per track: the table at the top of docs/PROGRESS.md. Tasks
and owners: docs/TASKS.md. Milestone states: docs/ROADMAP.md.

## My next tasks, in order

1. **T-301** — exception directory registration in the worker
   (`RtlAddFunctionTable`), test executable raising and handling SEH,
   native reference run, malformed-directory rejection.
2. Remaining M3 features one at a time (TLS, import binding through
   adapters, CRT startup, DLL init), each with its own test executable.
3. Graphics triangle slice (M6).

Review queue: Codex T-706. Then decide the next shader ABI extension with Codex (contract first).

## Decisions to remember

- 0001 layout. 0002 custom mapping in the worker, OS loader as reference.
  0003 **Vulkan + SPIR-V** (maintainer choice; revisit at M8 only with
  verified D3D12.X evidence). 0004 Windows first, Linux-ready: no OS headers
  in portable components, OS code in `_win32.cpp`, no Linux claims without
  Linux runs. 0005 translate shaders on first use, disk cache keyed by
  binary/ABI/translator/backend, runtime diagnostics + opt-in debug mode.
- Resolver policy: adapter → approved guest module → permitted Windows
  export → unresolved. **Never** auto-stub unknown imports or fake success.
- Terminology: "executable" / "test executable", never "fixture". **SCALE**
  in prose, identifiers lowercase. No "games" in README.
- Other projects (e.g. XCL) are inspiration only, never copied. Don't name
  XCL in contributor-facing docs.
- Commit messages: task-id subject, no AI or self attribution trailers.

## Coordinator workflow I follow

- Every task gets a branch `claude/<task>` from `integration`. Stage explicit
  paths, never `git add -A`.
- Review Codex in a temporary detached worktree in the scratchpad: rebuild
  Debug and Release with `-DSCALE_ENABLE_SHADER=ON`, run independent checks
  (llvm-mc/objdump, spirv-dis/spirv-val) and a mutation check, then merge
  `--no-ff`, run all suites on the merged tree (`full`, `release-full`,
  `full-all`, `release-full-all`, `gpu`), fast-forward `main`, delete the
  merged branch.
- Shared-file conflicts (`tests/*/CMakeLists.txt`): keep both blocks.
  `docs/TASKS.md` conflicts: keep ours, reapply statuses.
- Push only when the maintainer says so. Give the maintainer a Codex prompt
  after each review.

## Environment notes

- Always `. .\scripts\vsenv.ps1`, which reloads PATH from the registry and
  puts VS CMake/Ninja ahead of Strawberry Perl's.
- Tools: MSVC 19.52, SDK 10.0.26100, CMake 4.3.1, Ninja 1.13.2, Vulkan SDK
  1.4.363.0, LLVM 23.1.6 AMDGPU (`scripts/build-llvm-amdgpu.ps1`, installed in
  `%LOCALAPPDATA%\Programs\LLVM-AMDGPU\23.1.6\bin`). GPU: RX 9070 XT,
  Vulkan 1.4.359, subgroup 64 (range 32–64).
- Shell edits lose backslashes in Bash heredocs. Use Write/Edit or PowerShell
  `[IO.File]`. In PowerShell, `@(@('a','b'))` flattens; use `,@('a','b')`.

## Open items

- (none)

## Requests to coordinator

(none — this is the coordinator)
