# Claude — handoff notes

Role: architect and integration coordinator (sole merger to `integration`
and `main`), CPU/runtime, reference interpreter, Vulkan backend,
differential GPU verification.

Last updated: 2026-11-24, at `integration` = `main` = `f0d2927` (local; 13 commits ahead of `origin`).
`origin` is at `e912520`, and the maintainer authorizes each push.

## State of the project

| Milestone | State |
|---|---|
| M0 Foundation | done (T-000, T-001 GoogleTest 1.18.0) |
| M1 PE inspector | done (T-101 library, T-102 dumpbin comparison, T-103 `scale inspect`) |
| M2 Native execution | done (T-201 worker supervisor, T-202 loader + first native run, T-203 `scale run` + report.json, T-204 unresolved-import diagnostics) |
| M7 Shader recompiler | in progress: T-704 ABI v0, T-701 decoder, T-702 IR and T-703 SPIR-V are done (Codex). **T-711 and T-720 are mine.** |
| M6 Graphics | T-601 ready (mine) |

Test totals (unchanged since the T-703 merge, `da29a41`): `ctest --preset full` 169/169, `release-full`
169/169, and with `-DSCALE_ENABLE_SHADER=ON` (`out/build/debug-shader`)
212/212.

## My next tasks, in order

1. **T-711 — RDNA2 reference interpreter** (`src/shader_ref`,
   `scale::shader_ref`). It interprets `scale::shader::DecodedInstruction`
   directly (frozen `decode.h`, `c4ce6f3`) over a modeled wave32: SGPRs,
   VGPRs, EXEC, VCC, SCC, and the two T-704 buffers. It must not use
   `shader_ir` or `shader_spirv`. Hand-computed expected values per
   instruction, plus inactive-lane and overflow tests. It must check inline
   constants on its own, using `Operand::encoded`, not only the decoder's
   `value`.
2. **T-601 — Vulkan compute harness** (`src/graphics`,
   `SCALE_ENABLE_GRAPHICS`). Instance, device and queue, two storage buffers,
   dispatch `(1,1,1)`, readback. Verify first with a known GLSL→SPIR-V
   compute shader (glslangValidator). Label `gpu`. If no device is present,
   the test is SKIPPED. Record device, driver and SDK. The device is an
   RX 9070 XT, Vulkan 1.4.359, `shaderRoundingModeRTEFloat32 = true`,
   subgroup 64 (range 32–64).
3. **T-720 — differential runner**: T-704 JSON test cases → decode → (a)
   T-711 interpreter, (b) IR → SPIR-V → T-601 GPU run. Compare both
   against the manual expected values and each other: exact u32, f32 at
   0 ULP in the normal domain. This closes the first M7 slice.
4. **T-801 — portability groundwork** (decision 0004): portable `common` (UTF conversion, SHA-256), per-component CMake platform gating, a GCC/Clang flag block, and a shared test process helper. Windows results must not change. T-802 (Linux CI) is BLOCKED until a Linux environment exists.
5. Then M3, CPU/runtime: binding imports through documented adapters,
   exception directory registration (`RtlAddFunctionTable`), TLS, CRT
   entry. Each feature gets its own test executable.

## Decisions to remember

- 0004: Windows first, Linux-ready. No OS headers in portable components. OS code goes in `_win32.cpp` behind neutral headers. Linux support is never claimed without Linux runs.
- 0001: build and component layout. 0002: custom mapping in the worker; the
  OS loader is the reference. 0003: **Vulkan + SPIR-V** (maintainer
  choice). Revisit at M8 only with verified D3D12.X evidence.
- Resolver policy: adapter → approved guest module → permitted Windows
  export → unresolved. **Never** auto-stub unknown imports or fake success.
  Other projects do this. SCALE deliberately does not.
- Terminology: "executable" / "test executable", never "fixture". The
  project name is **SCALE** in prose. Identifiers stay lowercase (`scale`
  command, `scale::`, targets, `tool.name`).
- Other projects (e.g. XCL) are inspiration only, never copied. Don't
  name XCL in contributor-facing docs.
- Commit messages: task-id subject, no AI or self attribution trailers.

## Coordinator workflow I follow

- Every task gets a branch `claude/<task>` from `integration`. Stage explicit
  paths, never `git add -A`, so the maintainer's own edits (e.g. README)
  stay out of my commits.
- Review Codex in a temporary detached worktree in the scratchpad. Rebuild
  Debug and Release with `-DSCALE_ENABLE_SHADER=ON`, run independent
  checks (llvm-mc/objdump, spirv-dis/spirv-val) and a mutation check, then
  merge `--no-ff`, run all suites on the merged tree, fast-forward `main`,
  and delete the merged branch (`git branch -d`). A branch still checked out
  in Codex's worktree is deleted later.
- Shared-file conflicts (`tests/*/CMakeLists.txt`): keep both blocks.
- Push only when the maintainer says so.

## Environment notes

- Always `. .\scripts\vsenv.ps1`, which reloads PATH from the registry and
  puts VS CMake/Ninja ahead of Strawberry Perl's. A shell without it can't
  find `llvm-mc`, `spirv-val` or `spirv-dis`.
- Tools: MSVC 19.52.37029, SDK 10.0.26100, CMake 4.3.1, Ninja 1.13.2,
  Vulkan SDK 1.4.363.0, LLVM 23.1.6 AMDGPU (built by
  `scripts/build-llvm-amdgpu.ps1` into
  `%LOCALAPPDATA%\Programs\LLVM-AMDGPU\23.1.6\bin`).
- The shell tools lose backslashes in heredoc/Python edits. Write regexes
  and Windows paths with the Write/Edit tools or PowerShell `[IO.File]`.
  In PowerShell, a one-element nested array `@(@('a','b'))` flattens. Use
  `,@('a','b')`.
- `ctest --preset gpu` fails with "No tests were found" until T-601 adds GPU
  tests. This is intentional.

## Open items

- Codex branch `codex/T-703-spirv` is merged but still checked out in
  Codex's worktree. Delete it once Codex switches.
- Codex-owned shader docs still say "Scale" in a few places. Codex will
  update them in its next branch (included in its prompt).

## Requests to coordinator

(none — this is the coordinator)
