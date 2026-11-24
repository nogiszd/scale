# Roadmap

Milestone definitions and acceptance criteria are in `AGENTS.md`. This file
records scope decisions and the current track plan. Milestone numbers are
deliverables, not a strict serial order.

| Milestone | Scope | Tasks | State |
|---|---|---|---|
| M0 Foundation | CMake/presets, CLI skeleton, diagnostics contract, CTest | T-000 | done |
| M1 PE inspector | Read-only bounded PE32+ AMD64 inspection, `scale inspect` | T-101..T-103 | ready |
| M2 Native execution slice | Worker, custom-entry fixture, relocated fixture, `scale run`, missing-import diagnostic | T-201..T-204 | ready (T-201) |
| M3 Imports and Windows runtime fidelity | Imports/ordinals/forwarders, delay load, CRT startup, TLS, SEH/C++ EH, DLL init | — | not planned yet |
| M4 Adapters and filesystem | Adapter registry, title root + data root, path translation | backlog | — |
| M5 Integration and diagnostics | report.json completeness, synthetic test title | — | — |
| M6 Graphics slice | Vulkan compute/triangle via synthetic adapter | T-601 | ready after T-001 |
| M7 RDNA2 shader recompiler | ABI contract, decoder, IR, SPIR-V, interpreter, differential GPU tests | T-701..T-720 | ready (T-704, T-701) |
| M8 Graphics integration | Compiler → pipeline creation | — | — |
| M9 Audio, input, local data | Interfaces demanded by verified tests | — | — |
| M10 Retail readiness gate | See AGENTS.md | — | not started |

## Parallel tracks after M0

- **CPU/runtime:** Claude on M1 and M2 (inspector and worker first, then
  loader slices).
- **Shader:** Codex on the T-704 ABI contract and the T-701 decoder. Claude
  starts the T-711 interpreter once the decoder header is frozen.
- **Graphics:** Claude, T-601, as soon as the Vulkan SDK is installed.
