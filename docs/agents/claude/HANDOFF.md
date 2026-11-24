# Claude — handoff notes

Role: architect, integration coordinator, CPU/runtime, reference
interpreter, Vulkan backend.

## Completed

- T-000 foundation scaffold (see docs/PROGRESS.md 2026-11-24).

## Current / next

- T-201 worker process supervisor (READY).
- Review gates: T-101 header freeze (Gemini), T-701 `decode.h` freeze and
  T-704 ABI contract (Codex).
- T-601 is blocked until the Vulkan SDK is installed.

## Notes

- Strawberry Perl's CMake and Ninja come first on PATH. Always dot-source
  `scripts/vsenv.ps1`.
- `ctest --preset gpu` fails while no GPU tests exist, on purpose.

## Requests to coordinator

(none — this is the coordinator)
