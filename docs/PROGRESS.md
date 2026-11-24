# Progress

Only recorded, executed results. Newest first.

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
| Vulkan SDK | **absent**: no `spirv-val`, `glslangValidator`, `VULKAN_SDK` |
| LLVM AMDGPU | **absent**: no `llvm-mc`, `llvm-objdump`. VS-bundled LLVM has only clang-format/clang-tidy |
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

Parallel tasks in `docs/TASKS.md`: T-101 (Gemini), T-201 (Claude), T-704 and
T-701 (Codex). T-601 and the LLVM cross-check in T-701 wait on tool
installation.
