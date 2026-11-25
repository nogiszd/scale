# 0002 — Native image loading: custom mapping in the worker

Status: accepted (2026-11-24, T-202)

## Context

M2 needs trusted PE32+ test executables to run on the host CPU. AGENTS.md says to
prefer documented OS loader facilities and to use custom mapping only when
there is a demonstrated need.

Options considered:

1. **OS loader** (`CreateProcess` on the image, or `LoadLibraryExW`). The OS
   resolves every import itself, through the standard DLL search order, before
   any SCALE code can intervene.
2. **Custom mapping** in the worker. SCALE copies headers and sections into
   private memory, applies base relocations, sets page protections and
   resolves imports itself.

## Decision

Use custom mapping (`scale::loader::map_image`), run only inside the
supervised worker process (T-201).

## Rationale (demonstrated requirements the OS loader cannot meet)

- **Resolver policy ([roadmap M3](../ROADMAP.md#m3--import-resolution-and-windows-runtime-fidelity)):** documented adapter → guest DLL from a configured module root → explicitly permitted Windows export → unresolved error. The OS
  loader cannot substitute an adapter for a synthetic module such as
  `test_platform.dll` without a real DLL of that name. It also follows
  the standard DLL search order.
- **No execution past an unresolved required import:** with the OS loader
  the process either fails with a loader dialog or status code, which can't
  be mapped to a structured diagnostic, or it runs. Custom mapping reports
  `unresolved_import` before any guest code runs (T-204).
- **Console images:** console executables will eventually need
  evidence-driven loading behavior (KNOWN_UNKNOWNS U-001). The mapping code
  is where that will live.

The OS loader is still the **reference**: every test executable whose contract allows
it also runs natively, and the observable results must match.

## Scope of the first slice (T-202)

Supported: EXE images with SectionAlignment ≥ 4 KiB, DIR64 base
relocations (relocatable images are always moved to an OS-chosen address),
the debug directory (ignored), the exception directory (validated and
registered with `RtlAddFunctionTable`, T-301), and an entry inside an
executable section, called directly as `int test_entry(void)`.

Rejected with named errors before any memory is made executable: DLLs, every
other present directory (imports, TLS, load config, delay imports, exports,
CLR, …), other relocation types, W+X sections, and fixed
images whose preferred base is taken.

## Consequences

- Each new loader feature (imports, TLS, SEH/unwind registration, DLL init)
  is a separate, tested addition to `map_image`'s allowlist (M3).
- Mapped images live in private memory. Debuggers and
  `GetModuleHandle` do not see them as modules. Exception dispatch and
  unwinding find guest functions through the table registered with
  `RtlAddFunctionTable` (T-301), deregistered before the memory is freed.
- Success with `test_entry` says nothing about CRT entry points
  (`mainCRTStartup`). These are separate M3 features.
- Executable pages come from `VirtualProtect(PAGE_EXECUTE_READ)` on private
  memory, so CFG marks the whole region as valid call targets. This is
  acceptable for trusted test executables. Revisit before untrusted inputs.
