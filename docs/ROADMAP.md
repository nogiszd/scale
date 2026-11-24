# Roadmap

Milestone definitions and acceptance criteria are in `AGENTS.md`. This file
records each milestone's scope and current state. Milestone numbers are
deliverables, not a strict serial order. Tasks and owners are in
[TASKS.md](TASKS.md). Verified results are in [PROGRESS.md](PROGRESS.md).

| Milestone | Scope | Tasks | State |
|---|---|---|---|
| M0 Foundation | CMake/presets, CLI, diagnostics contract, GoogleTest + CTest | T-000, T-001 | done |
| M1 PE inspector | Read-only bounded PE32+ AMD64 inspection, dumpbin comparison, `scale inspect` | T-101..T-103 | done |
| M2 Native execution slice | Supervised worker, image mapping, `scale run` + report.json, unresolved-import diagnostics | T-201..T-204 | done |
| M3 Imports and Windows runtime fidelity | Exception tables / unwind, TLS, import binding through adapters, delay load, CRT startup, DLL init | T-301, T-302 | in progress: exception directory done (T-301), TLS next (T-302) |
| M4 Adapters and filesystem | Adapter registry, title root + writable data root, path translation | — | backlog |
| M5 Integration and diagnostics | report.json completeness, synthetic test title | — | not started |
| M6 Graphics slice | Vulkan compute harness (done), then a verified triangle through a synthetic adapter | T-601 | compute done, triangle not started |
| M7 RDNA2 shader recompiler | ABI contracts, decoder, IR, SPIR-V, reference interpreter, GPU differential tests | T-701..T-734 | ABI v0 and v1 verified on GPU, `scale-shader` CLI done; ABI v2 (memory and resources, T-730..T-734) next |
| M8 Graphics integration | Translated shaders → pipeline creation, translation cache (decision 0005) | — | not started |
| M9 Audio, input, local data | Interfaces demanded by verified tests | — | not started |
| M10 Retail readiness gate | See AGENTS.md | — | not started |
| infra Portability | Windows first, Linux-ready (decision 0004) | T-801, T-802 | groundwork done; Linux CI blocked |

## Current tracks

- **Shaders (Codex):** T-730 ABI v2 contract (memory and resources), then
  decoder (T-731) and IR/SPIR-V (T-732). Claude: interpreter (T-733) and GPU
  cases (T-734).
- **CPU/runtime (Claude):** T-302 TLS directory, then the remaining M3
  features one at a time (import binding through adapters, CRT startup, DLL
  init, C++ exceptions).
- **Graphics (Claude):** the triangle slice after T-721.
