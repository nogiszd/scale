# Claude — handoff notes

Role: architect, integration coordinator, CPU/runtime, reference
interpreter, Vulkan backend.

## Completed

- T-000 foundation scaffold (see docs/PROGRESS.md 2026-11-24).

## Current / next

- T-001 GoogleTest migration (waiting on dependency install), then T-101
  PE inspector and T-201 worker supervisor.
- Review gates: T-701 `decode.h` freeze and
  T-704 ABI contract (Codex).
- T-601 is ready (Vulkan SDK 1.4.363.0 installed).

## Notes

- Strawberry Perl's CMake and Ninja come first on PATH. Always dot-source
  `scripts/vsenv.ps1`.
- `ctest --preset gpu` fails while no GPU tests exist, on purpose.

## Requests to coordinator

(none — this is the coordinator)
