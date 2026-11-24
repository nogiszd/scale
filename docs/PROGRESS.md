# Progress

Only recorded, executed results. Newest first.

## 2026-11-24 — T-204 Unresolved import diagnostics — M2 complete (Claude)

- `scale::runtime::resolve_imports` (`src/runtime`) applies the resolver policy
  in order: documented adapter → approved guest module → permitted Windows
  export → unresolved. All three sources are empty in this slice, so every
  import is unresolved. There is no search path and no stub. Each becomes a
  fatal `runtime/import_resolution/unresolved_import` event with `module`,
  `symbol` or `ordinal`, `importing_module` and `import_kind`.
- The worker resolves imports after inspection and before mapping. If any are
  unresolved it reports up to 64 events (then `unresolved_imports_omitted`)
  and exits 3. Nothing is mapped and the entry is never reached. Events use
  the new percent-encoded `event` line format
  (`scale/diagnostics/event_line.h`). The host parses it into report.json,
  and the first fatal event sets the outcome.
- Test executables: `test_platform.dll` (synthetic platform API: TestAllocate
  by name, TestFree @2 NONAME) and `test_import_missing.exe` (imports both,
  returns 40 + 2).
- Native reference: the Windows loader resolves `test_platform.dll` from the
  executable's directory and returns 42. SCALE run: `BLOCKED` at stage
  `inspected`, with two unresolved_import events (ordinal 2 and TestAllocate).
  This is the intended difference: SCALE never uses a DLL search path.
- Mutation check: a resolver that accepts every import fails 3 tests. The
  loader still refuses the import table, so nothing executes even then.
- Tests: `unit_diagnostics.EventLine/PercentCoding` (4), `unit_runtime` (2),
  `unit_host.ExecutableReport` (+3), `integration_native_execution` (+2) and
  the CLI test `cli_run_blocked_on_unresolved_imports`.
- **M2 acceptance (AGENTS.md):** first native test executable with a native
  reference (T-202), relocated data (T-203), worker capturing exit code,
  output, timeout and exceptions (T-201/T-203), unsupported directories
  rejected explicitly, and an unresolved synthetic import diagnostic (T-204).
  Limitations: only the custom `int test_entry(void)` contract is supported.
  CRT entry, imports with binding, TLS and exceptions are M3.

## 2026-11-24 — Terminology: "fixture" → "executable" (Claude)

User-facing wording now follows the vocabulary of comparable projects (XCL:
"executable", "game"/"title", "test title"). Entries below this one keep the
old names as historical record. Mapping:

| Old | New |
|---|---|
| `scale run <trusted-fixture.exe>` | `scale run <executable>` |
| worker mode `run-fixture` | worker mode `run` |
| report `subject.kind = "fixture"` | `"executable"` |
| CLI label `fixture:` | `executable:` (all labels re-aligned) |
| `tests/fixtures/{pe,native}` | `tests/executables/{pe,native}` |
| targets `fixture_*` (`fixture_ret42`, …) | `test_*` (`test_ret42`, …) |
| entry contract `int fixture_entry(void)` | `int test_entry(void)` |
| `scale_add_pe_fixture`, `SCALE_FIXTURE_*` | `scale_add_test_executable`, `SCALE_TEST_EXE_*` |
| `host::FixtureRunOptions` / `FixtureRun` / `run_fixture` / `make_fixture_report` | `RunOptions` / `ExecutableRun` / `run_executable` / `make_run_report` |
| `fixture_run.{h,cpp}`, `native_fixture_test.cpp` | `executable_run.{h,cpp}`, `native_execution_test.cpp` |
| synthetic `fixture_platform.dll!FixtureAllocate` | `test_platform.dll!TestAllocate` |
| loader messages "not supported by the fixture loader" | "not supported by the loader yet" |

No behavior changed; the trusted, self-authored scope and the custom entry
contract stay the same. Codex-owned shader documents still say "fixture" for
shader test cases. Codex will update them in its own branch.

Verification after a clean rebuild: `ctest --preset full` 157/157,
`release-full` 157/157, `-DSCALE_ENABLE_SHADER=ON` 180/180.

## 2026-11-24 — T-203 `scale run`, relocated data, report.json (Claude)

- `scale run <fixture> [--report <path>] [--timeout-ms <n>]` (contract
  `docs/contracts/run.md`) runs the fixture through `scale_worker`. The
  outcome is PASS (entry returned), BLOCKED (unsupported loader feature) or
  FAILED (malformed input, exception, timeout, unexpected exit). Guest
  arguments are rejected. `--report` writes report.json v1 with the
  subject's SHA-256 (BCrypt; `scale::sha256_hex`, verified against FIPS
  vectors and `Get-FileHash`).
- `scale::host::run_fixture` / `make_fixture_report`: worker report lines
  become stages and diagnostic events. Exceptions inside the image get
  `rva` and `module`. Worker error lines now carry
  `stage=<read|inspect|map>`, and the mapped line carries `size`.
- New fixtures: `fixture_reloc.exe` (two DIR64 pointers → 40 + 2),
  `fixture_crash.exe` (null write) and `fixture_hang.exe` (spin). Globals
  instead of stack locals keep crash/hang as leaf functions, without an
  exception directory, which the loader still rejects.
- Observed CLI results: ret42 and reloc → PASS, returned 42. crash →
  FAILED, `host/entry/unhandled_exception exception_code=0xc0000005
  rva=0x1007`. hang with `--timeout-ms 500` → FAILED after 507 ms.
  `fixture_exit.exe` → BLOCKED, `loader/map/unsupported_directory`.
- Native reference comparisons: reloc returns 42 both ways (worker: 2
  relocations, non-zero delta). The crash is 0xC0000005 natively (exit code)
  and in the worker (reported exception). The hang times out both ways.
- Mutation check: with relocation disabled, `AppliesDir64Relocations`,
  `RelocatedPointersMatchReference` and `cli_run_relocated` fail.
  `fixture_reloc` crashes at RVA 0x1028. Restored afterwards.
- Tests added: `unit_common.Sha256` (2), `unit_host.FixtureReport` (10),
  `integration_native_fixture` (+4), and 11 CLI tests, including report.json
  content checked by a dependent CTest fixture.
- `ctest --preset full` and `release-full`: see the commit. Limitations: the
  supervisor runs one fixture at a time, and stdout/stderr are captured but
  not printed by `scale run`.

## 2026-11-24 — T-202 First native execution slice (Claude)

- Decision 0002: custom mapping in the worker. The OS loader cannot meet
  the resolver policy or stop before an unresolved import. It stays the
  native reference.
- `scale::loader::map_image` copies headers and sections into private
  memory, applies DIR64 relocations (relocatable images always move), sets
  per-section protections (headers R, code RX, data RW, gaps NOACCESS) and
  returns the entry. Before allocating, it rejects DLLs, any directory except
  base relocations and debug, other relocation types, W+X sections, a
  non-executable or zero entry, and a taken preferred base without
  relocations. `read_file` is shared with `inspect_pe_file`.
- `fixture_ret42.exe` (`tests/fixtures/native`): entry contract
  `int fixture_entry(void)` returning the initialized global
  `fixture_result` (42). No CRT, no imports.
- Native reference: Windows exits the process with the entry's return value.
  42 in 30/30 runs of a scratch probe, and 42 under the supervisor in the
  test.
- Worker `run-fixture <path>` reports `stage start/inspected/mapped/entry/
  returned value=N` and exits with the entry's value. Loader errors exit 3
  with `error component=... code=... message=...`. Observed run:
  `stage mapped base=0x140000000 delta=0 relocations=0`,
  `stage returned value=42`, exit 42, 17 ms. `fixture_exit.exe`: `error
  component=loader code=unsupported_directory message=the import_table
  directory ...`, exit 3, entry never reached.
- Tests: `unit_image_map` (11, quick): mapping and protections checked with
  `VirtualQuery`, DIR64 relocation, release on destruction, and every
  rejection code. No code is executed in this process.
  `integration_native_fixture` (5): native reference = 42, worker = 42 and
  equal to the reference with all stages, imports/DLL/missing file rejected
  before entry.
- Limitations: one fixture with a custom entry. Relocated-data execution is
  T-203. There is no `scale run` CLI yet (T-203). Imports, TLS, exceptions and
  CRT entry points are unsupported and rejected.

## 2026-11-24 — T-201 Worker process supervisor (Claude)

- `scale::host::run_supervised` (`src/host`) creates the worker directly in a
  job object (`PROC_THREAD_ATTRIBUTE_JOB_LIST`) with kill-on-close, a
  committed-memory limit and `DIE_ON_UNHANDLED_EXCEPTION` (no WER UI). It
  inherits only an explicit handle list (`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`):
  stdin = NUL, stdout/stderr pipes and a report pipe. stdout/stderr are
  captured up to a limit (the rest is drained and discarded). A wall-clock
  timeout kills the job. The job is also terminated after the worker exits,
  which kills any descendants.
- Report channel: the worker gets `--scale-report-handle=<hex>`. Its
  top-level exception filter writes `exception code=0x... address=0x...` to
  that pipe and terminates with the exception code. Guest stdout/stderr
  cannot fake a crash report.
- `tools/scale_worker`: self-test modes (exit, access-violation, hang, flood,
  echo-args, read-stdin, alloc, report, spawn-orphan). Guest execution is
  refused with "not implemented yet (task T-202)".
- Tests: `unit_host` (4, quick): command-line quoting round-trips through
  `CommandLineToArgvW` (spaces, quotes, trailing backslashes, empty, UNC,
  non-ASCII) and report-line parsing. `integration_host_supervisor` (13):
  exit code; access violation reported as 0xC0000005 with an address (20 ms,
  no dialog); a 500 ms timeout (0.51 s observed); 4 MiB per stream truncated
  to 64 KiB without blocking; exact arguments; empty stdin; a 64 MiB job
  limit rejecting a 256 MiB commit; report lines; a fake crash line on
  stdout ignored; descendant killed; relative and missing executables rejected.
- `ctest --preset full` and `release-full`: 115/115 passed, zero warnings.
  `scale_worker.exe` has Dynamic base, NX, CFG and CET.
- Limitations: the worker uses the caller's unelevated token (no restricted
  token or AppContainer). It is not a security boundary. Crashes that bypass
  the filter (fail-fast, stack overflow) appear only as
  `abnormal_exit_status`.

## 2026-11-24 — T-103 `scale inspect` — M1 complete (Claude)

- `scale inspect <file> [--json]` (`tools/scale/inspect_command.cpp`) prints a
  human-readable summary or the JSON schema v1 from `docs/contracts/inspect.md`.
  A rejected file gives exit 1 with `component/code: message` and, with
  `--json`, an error document. Usage errors exit 2.
- 9 new CLI tests (label integration): text and JSON output on the T-102
  fixtures, a malformed file, a missing file and usage errors. The old
  not-implemented test now targets `run` (T-203).
- Observed: shell tools in this environment halve backslashes, so several
  regex escapes were silently lost and some patterns matched too loosely.
  Fixed by writing the file directly. A deliberately wrong expectation was
  confirmed to fail the check script.
- `ctest --preset full` and `release-full`: 98/98 passed, zero warnings.
- **M1 acceptance:** bounded PE32+ AMD64 inspector with a negative test per
  error code (T-101), fixture metadata compared against dumpbin (T-102),
  `scale inspect` with `--json` (T-103). Delay imports, exports and unwind
  data are reported as present only and not parsed. Nothing is executed.

## 2026-11-24 — T-102 PE fixtures and dumpbin comparison (Claude)

- `tests/fixtures/pe`: three no-CRT fixtures with an explicit `/ENTRY`,
  `/DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT`, `/GS` kept and `/RTC1` removed:
  `fixture_exit.exe` (KERNEL32!ExitProcess by name, plus a DIR64 relocation
  from an absolute data pointer), `fixture_lib.dll` (exports FixtureAdd
  @7 NONAME and FixtureMul by name), and `fixture_ordinal.exe` (imports
  fixture_lib by ordinal 7 and by name).
- `integration_pe_dumpbin` (6 cases, label integration): it parses
  `dumpbin /headers /imports /relocations` (MSVC 14.52.37025 dumpbin) on its
  own and compares machine, characteristics, entry point, image base,
  alignments, image/header sizes, subsystem, DLL characteristics, all 16
  directories, every section field, imports (module, IAT VA, names with
  hints, ordinals) and relocation block/type counts with the inspector. It
  also checks each fixture's documented contract. If dumpbin is missing, the
  comparison is SKIPPED.
- Observed: when a `.def` export has an explicit ordinal and no NONAME, the
  import library makes importers bind by ordinal. `FixtureMul` therefore has
  no ordinal in the `.def`.
- Observed: dumpbin lists a module's ordinal imports in a different order
  than the import table, so entries are compared as sorted sets.
- `ctest --preset full` and `release-full`: 89/89 passed, zero warnings.
- The fixtures are inspected only. Running them is T-202.

## 2026-11-24 — T-101 PE32+ inspector library (Claude)

- `scale::loader` (`src/loader`): `inspect_pe(ByteView)` and
  `inspect_pe_file(path, max_size)`. It parses and validates the DOS, COFF
  and PE32+ optional headers, alignment, sections, all 16 data directories
  (bounds), imports (by name with hint, by ordinal, IAT fallback), the
  base-relocation summary by type, TLS (with callback RVAs) and the
  exception directory (RUNTIME_FUNCTION count). Delay-import, export, load
  config and the remaining directories are reported as present only. Limits
  and the 30 error codes are documented in `pe_image.h`.
- `unit_loader` (48 cases, label quick): a test-side PE builder
  (`tests/unit/loader/pe_builder.h`) builds valid images; every error code
  has a negative case. Also includes a read-only inspection of
  `%SystemRoot%\System32\kernel32.dll`.
- Mutation check: with the relocation block-size validation disabled, the
  suite aborts. Restored afterwards; all cases pass.
- `ctest --preset full` and `release-full`: 83/83 passed, zero warnings.
- Not yet compared against dumpbin (T-102). Unwind data, exports and delay
  imports are not parsed.

## 2026-11-24 — T-001 GoogleTest migration (Claude)

- GoogleTest **1.18.0** via `FetchContent`, from
  `https://github.com/google/googletest/releases/download/v1.18.0/googletest-1.18.0.tar.gz`
  with `URL_HASH SHA256=6e3191c1455468b3fc35a417fb565c1c5071aee1b7e7f85e30cf48a98d37d8b5`.
  gMock off, shared CRT (/MD).
- The in-tree harness (`tests/support/scale_test.h`, its main and the skip
  probe) was removed. All unit cases were ported with unchanged meaning.
  `scale_add_test` now calls `gtest_discover_tests` (PRE_TEST discovery) with
  labels, timeout and `SKIP_REGULAR_EXPRESSION "\[  SKIPPED \]"`.
- Self-tests (`selftest_gtest_*`, label quick) check GoogleTest's skip, pass
  and failure output and exit codes.
- One-off end-to-end check: a scratch project registered the probe through
  `scale_add_test`. CTest reported `Probe.Passing` Passed,
  `Probe.Skipped` ***Skipped and `Probe.Failing` ***Failed (exit 8).
- `ctest --preset full`: 100% passed, 35/35 (30 quick, 5 integration).
  `ctest --preset release-full`: 35/35. Zero compiler warnings in both
  configurations.

## 2026-11-24 — Toolchain: Vulkan SDK and LLVM AMDGPU (Claude)

- Vulkan SDK 1.4.363.0 installed by the user. `scripts/vsenv.ps1` now reloads
  machine and user environment variables from the registry, so tools
  installed after the shell started are found.
- The official LLVM 23.1.6 Windows release (`clang+llvm-23.1.6-x86_64-pc-windows-msvc`)
  was checked and **does not register AMDGPU** (targets: AArch64, ARM, BPF,
  NVPTX, RISC-V, WebAssembly, x86). The winget package lacks `llvm-mc`.
  The prebuilt copy was removed.
- Added `scripts/build-llvm-amdgpu.ps1`. It verifies the pinned source SHA-256,
  builds only `llvm-mc` and `llvm-objdump` with `LLVM_TARGETS_TO_BUILD=AMDGPU`
  (1061 build steps, MSVC 19.52), installs them and checks a gfx1030 encoding.
  Ran it end to end with `-AddToUserPath -Clean`. Its output is recorded in
  the environment table below.
- `.\scripts\check-env.ps1`: every CPU, graphics and shader tool is present,
  and the AMDGPU target is registered.

## 2026-11-24 — T-000 Foundation scaffold (Claude)

### Environment (verified)

| Item | Value |
|---|---|
| OS | Windows 11 Pro 10.0.26200 x64 |
| Visual Studio | Community 2026 (18.12), `C:\Program Files\Microsoft Visual Studio\18\Community` |
| MSVC | 19.52.37029 (toolset 14.52.37025), host x64 → target x64 |
| Windows SDK | 10.0.26100.0 |
| CMake | 4.3.1-msvc1 (VS-bundled; preferred by `scripts/vsenv.ps1`) |
| Ninja | 1.13.2 (VS-bundled) |
| Git | 2.50.1.windows.1 |
| GPU | AMD Radeon RX 9070 XT, AMD proprietary driver, Vulkan API 1.4.359 (driver 2.0.416), from `vulkaninfo --summary` |
| Vulkan SDK | 1.4.363.0 at `C:\VulkanSDK\1.4.363.0`; `spirv-val` SPIRV-Tools v2026.4, `glslangValidator` 16.6.0 (installed by the user, 2026-11-24) |
| LLVM AMDGPU | 23.1.6, `llvm-mc` + `llvm-objdump` only, targets amdgcn/amdgpu/r600, built from source by `scripts/build-llvm-amdgpu.ps1` into `%LOCALAPPDATA%\Programs\LLVM-AMDGPU\23.1.6\bin` (user PATH). Check: `v_add_nc_u32_e32 v0, v1, v2` -> `[0x01,0x05,0x00,0x4a]`, `s_endpgm` -> `[0x00,0x00,0x81,0xbf]` (gfx1030) |
| Python | 3.13 (optional, unused) |

Note: Strawberry Perl puts CMake 3.29.2 and Ninja 1.12.0 on the user PATH
first. `scripts/vsenv.ps1` moves the VS-bundled tools ahead of them for the
current session only. Both CMake versions satisfy the 3.28 minimum.

### Commands and results

```powershell
. .\scripts\vsenv.ps1
.\scripts\check-env.ps1          # "All CPU track tools present." exit 0
cmake --preset debug             # configured, no warnings
cmake --build --preset debug     # 0 warnings under /W4 /WX
ctest --preset full              # 100% tests passed, 0 failed out of 9
ctest --preset quick             # 4/4 passed
ctest --preset integration       # 5/5 passed
cmake --preset release; cmake --build --preset release
ctest --preset release-full      # 9/9 passed
ctest --preset gpu               # "No tests were found" -> error (intended; no GPU tests exist)
dumpbin /headers out\build\debug\bin\scale.exe
    # High Entropy VA, Dynamic base, NX compatible, Control Flow Guard, CET compatible
```

### Delivered

- Root CMake with component registry. Presets: `debug`, `release`; test
  presets `quick`, `integration`, `gpu`, `full`, `release-full`.
- `scale::build_options` with hardening flags.
- `scale::common`: `Error`/`Result`, checked arithmetic, `ByteView`, UTF-8 ↔
  UTF-16.
- `scale::diagnostics`: JSON writer (UTF-8 sanitizing), deduplicating
  bounded `DiagnosticLog`, `report.json` schema v1 with a golden test.
- CLI `scale` with `help` and `version`. `inspect` and `run` are registered
  as not implemented (exit 3).
- Test harness with skip semantics (exit 77 → CTest SKIPPED), verified by
  self-tests. `scale_add_command_test` checks exact exit codes.
- Docs: architecture, contracts, task registry, multi-agent workflow.

### Limitations

- No PE parsing, execution, graphics or shader code yet. This is
  infrastructure only.
- The `gpu` preset has no tests.

### Next

T-001 (GoogleTest migration) first, then parallel tasks in `docs/TASKS.md`:
T-101 and T-201 (Claude), T-704 and
T-701 (Codex). T-601 and the LLVM cross-check in T-701 wait on tool
installation.
