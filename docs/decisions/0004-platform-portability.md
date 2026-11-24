# 0004 — Platform portability: Windows first, Linux-ready

Status: accepted (2026-11-24, maintainer request)

## Context

SCALE targets Windows 11 x64 first (AGENTS.md). The graphics backend is
Vulkan (decision 0003), which also runs on Linux, and the maintainer wants
the repository to be able to continue on Linux later. No Linux environment is
available yet, so Linux support stays **unverified** until it builds and its
tests run there.

## Decision

Keep every component that doesn't need the host OS platform-independent
now, and isolate the OS-specific parts behind small, named boundaries so a
Linux implementation can be added beside the Windows one later.

| Component | Portability class | Notes |
|---|---|---|
| `scale::diagnostics` | portable | std only |
| `scale::common` | portable API | `text.cpp` (UTF-16 conversion) and `sha256.cpp` (BCrypt) are Windows implementations to be split or replaced (T-801) |
| `scale::loader` inspection (`pe_image`) | portable | parses bytes only |
| `scale::loader` mapping (`image_map`) | OS-specific | `VirtualAlloc`/`VirtualProtect` → `mmap`/`mprotect` on Linux |
| `scale::runtime` resolver policy | portable logic | adapters may be OS-specific |
| `scale::host`, `scale_worker` | OS-specific | job object, `CreateProcessW` and handle lists → process groups, rlimits, pidfd/pipes on Linux |
| `scale::shader_*`, `scale::shader_ref`, `scale-shader` | portable | must never include OS headers |
| `scale::graphics` (Vulkan) | portable core | OS-specific surface/presentation code isolated |
| CLI `scale` | thin OS shim | `wmain` and UTF-16 arguments on Windows, `main` on Linux |

## Rules (apply to all new code)

1. No OS headers (`windows.h`, `unistd.h`, …) in public headers, and none in
   portable components at all.
2. OS-specific code goes in files suffixed `_win32.cpp` (later
   `_linux.cpp`/`_posix.cpp`) inside the component that owns the boundary,
   behind a platform-neutral header. Don't scatter `#ifdef _WIN32`.
3. Paths are `std::filesystem::path`. Text is UTF-8 `std::string`. UTF-16
   appears only at Win32 calls.
4. Compiler flags live in `cmake/ScaleOptions.cmake` per compiler (MSVC now,
   GCC/Clang block later). Components gate themselves on platform. The root
   build should not refuse a whole platform once T-801 lands.
5. Tests of portable components must not use Win32 APIs. Starting tools such
   as `spirv-val` goes through a shared helper, not direct `_wspawn` calls.
6. Vulkan code uses only the loader and Khronos APIs. Platform surface
   extensions stay in the OS-specific layer.

## Consequences and limits

- Shaders, diagnostics, PE inspection and the Vulkan backend can move to
  Linux at low cost.
- **Running Xbox titles on Linux is a much larger step.** The title code is
  native Windows x64 code that expects the Windows x64 ABI, SEH/unwind
  semantics, TLS and Win32 behavior. Under the resolver policy, "permitted
  Windows exports" don't exist on Linux. A Linux execution host needs its
  own decision record when it is attempted. Until then the execution track
  is Windows-only by design.
- Follow-up tasks: T-801 (portable `common`, CMake platform gating, shared
  process helper for tests) and T-802 (Linux build/test environment and CI).
  T-802 can't be marked done without real Linux runs.
