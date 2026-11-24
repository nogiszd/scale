# Progress

Only recorded, executed results. Newest first.

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
