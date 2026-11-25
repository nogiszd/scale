# SCALE — agent instructions

## Goal and boundaries

Build an independent Windows x64 compatibility layer for Xbox Series X|S software. Execute compatible AMD64 code on the host CPU, provide platform runtime adapters, and translate graphics and RDNA2 shaders. CPU/runtime, graphics and shader translation are all required. For retail testing guidance, see the [roadmap](docs/ROADMAP.md#m10--retail-compatibility-baseline-triage-and-iteration).

Use public documentation and test material you create or obtain yourself. Do not copy, vendor or mechanically translate XCL or other projects' code. Do not distribute proprietary SDK files or games, bypass DRM or package encryption, spoof entitlements or online services, or invent unknown console ABI details. Keep all code, comments, documentation and CLI output in English.

## Start each task

1. Read [current state](docs/PROGRESS.md), the current [task registry](docs/TASKS.md) and only the relevant contract or decision record. Inspect the actual code and toolchain before relying on a status claim. Treat the dated progress heading as a pointer, not a promise that it is current.
2. Work on a small, testable vertical slice. Use the assigned task, ownership and branch rules in [multi-agent coordination](docs/MULTI_AGENT.md); contributions and review steps are in [CONTRIBUTING.md](CONTRIBUTING.md). In ordinary manual sessions, take the user's requested task rather than selecting a new one.
3. Run relevant tests and record the actual commands, results, skips, limitations and blockers. A skipped test is not a pass. Update the relevant state documents before reporting completion.

## Engineering rules

- Use C++23, MSVC, CMake and Ninja on Windows first. Keep portable code free of OS headers; see [architecture](docs/architecture.md) and [portability decision](docs/decisions/0004-platform-portability.md). Create modules and add dependencies only for a demonstrated need.
- Use RAII, checked arithmetic, bounded parsing, explicit ownership and structured errors. Preserve ASLR, DEP and other applicable mitigations.
- The native worker isolates crashes, but is not a security sandbox. Never alter original title files; run repository-built tests without administrator privileges. Use a disposable VM for experiments with unfamiliar binaries.
- Reject unsupported required imports and behavior explicitly. No guessed signatures, blanket stubs, silent fallbacks or fabricated success. Record console-specific gaps in [known unknowns](docs/research/KNOWN_UNKNOWNS.md).
- Use GoogleTest via CTest; follow the [testing contract](docs/contracts/testing.md). Keep shader compiler semantics independent from the reference interpreter.
- Research needed for a feature using [recorded sources](docs/research/REFERENCES.md), public specifications, public reference projects and reproducible observations. Distinguish documented facts, inference and unknowns; verify approaches from other projects with an independent source or test before implementing them. Do not copy their code.
- Do not install software or change machine-wide settings automatically. Do not overwrite unrelated work. The maintainer controls pushes. In manual sessions, commits are optional unless requested; for a task handoff, follow the repository's branch and review process.

## Where information lives

| Need | Authoritative document |
|---|---|
| Current implementation and verified results | [PROGRESS.md](docs/PROGRESS.md) |
| Milestones, acceptance gates and compatibility loop | [ROADMAP.md](docs/ROADMAP.md) |
| Task ownership, dependencies and shared-file rules | [TASKS.md](docs/TASKS.md) |
| Component boundaries | [architecture.md](docs/architecture.md) |
| Build setup | [BUILD.md](docs/BUILD.md) |
| Testing details | [testing.md](docs/contracts/testing.md) |
| Evidence, sources and open unknowns | [research](docs/research/REFERENCES.md) |
| Agent handoffs | `docs/agents/<agent>/HANDOFF.md` |

A feature is complete when its contract is documented, it builds, appropriate positive and negative tests have run, and limitations are explicit. A milestone requires its acceptance evidence in the roadmap; never mark unexecuted tests as passed.
