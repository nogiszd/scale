# SCALE — agent instructions

## Goal and boundaries

Build an independent Windows x64 compatibility layer for Xbox Series X|S software. Execute compatible AMD64 code on the host CPU, provide platform runtime adapters, and translate graphics and RDNA2 shaders. CPU/runtime, graphics and shader translation are all required. For retail testing guidance, see the [roadmap](docs/ROADMAP.md#m10--retail-compatibility-baseline-triage-and-iteration).

Use public documentation and test material you create or obtain yourself. Study public implementations and independently implement guest-facing console module behavior where the ABI is evidenced. Evaluate external libraries for a demonstrated need and document their license and integration. Do not distribute proprietary SDK files or games, bypass DRM or package encryption, spoof entitlements or online services, or invent unknown console ABI details. Keep all code, comments, documentation and CLI output in English.

## Start each task

1. Read [current state](docs/PROGRESS.md), the current [task registry](docs/TASKS.md) and only the relevant contract or decision record. Inspect the actual code and toolchain before relying on a status claim. Treat the dated progress heading as a pointer, not a promise that it is current.
2. Work on a small, testable vertical slice. Follow the task, ownership and branch rules in [CONTRIBUTING.md](CONTRIBUTING.md). A free pick-up task can be claimed and worked on solo as described there. See [multi-agent coordination](docs/MULTI_AGENT.md) for work shared with other agents. In ordinary manual sessions, take the user's requested task rather than selecting a new one.
3. Run relevant tests and record the actual commands, results, skips, limitations and blockers. A skipped test is not a pass. Update the relevant state documents before reporting completion.

## Engineering rules

- Use C++23, MSVC, CMake and Ninja on Windows first. Keep portable code free of OS headers; see [architecture](docs/architecture.md) and [portability decision](docs/decisions/0004-platform-portability.md). Create modules and add dependencies only for a demonstrated need.
- Use RAII, checked arithmetic, bounded parsing, explicit ownership and structured errors. Preserve ASLR, DEP and other applicable mitigations.
- The native worker isolates crashes, but is not a security sandbox. Never alter original title files; run repository-built tests without administrator privileges. Use a disposable VM for experiments with unfamiliar binaries.
- Reject unsupported required imports and behavior explicitly. No guessed signatures, blanket stubs, silent fallbacks or fabricated success. Record console-specific gaps in [known unknowns](docs/research/KNOWN_UNKNOWNS.md).
- Use GoogleTest via CTest; follow the [testing contract](docs/contracts/testing.md). Keep shader compiler semantics independent from the reference interpreter.
- Research needed for a feature using [recorded sources](docs/research/REFERENCES.md), public specifications, public reference projects and reproducible observations. Distinguish documented facts, inference and unknowns; verify approaches from other projects with an independent source or test before implementing them. Check license compatibility before reusing code.
- Do not install software or change machine-wide settings automatically. Do not overwrite unrelated work. The maintainer controls pushes. In manual sessions, commits are optional unless requested; for a task handoff, follow the repository's branch and review process.

## C++ quality and performance

- Keep control flow direct and make ownership and lifetimes explicit. Use RAII
  for resources and non-owning views for borrowed data whose owner outlives the
  view. Prefer existing types and helpers. Share logic when multiple callers
  must enforce the same invariant or implement the same behavior. Keep one-off
  code local and preserve independent implementations used as test oracles.
- Use `scale::ByteView` for untrusted binary data and the
  [checked arithmetic helpers](src/common/include/scale/common/checked.h) for
  offset, size, count, alignment and narrowing operations derived from external
  inputs. Validate arithmetic and bounds before forming a pointer, indexing,
  allocating or narrowing. Check signed inputs before converting offsets or
  sizes to unsigned types. Match integer widths at guest ABI and file
  boundaries. When guest semantics require wraparound, implement it explicitly
  at the defined width and test the boundary values.
- In per-frame and other high-frequency paths, reuse scratch storage, keep
  queues and histories bounded, and defer I/O and diagnostic formatting where
  practical. Avoid recurring heap allocations, redundant copies, contended
  locks and CPU waits on GPU work. Document the reason for costs required by
  correctness. Keep validation, defined behavior and resource lifetimes intact
  when optimizing.
- Profile the affected workload and verify performance claims with a
  [repeatable before/after measurement](docs/contracts/testing.md#performance-measurement).
  Consider SIMD, custom allocators or profile-guided optimization when
  measurements identify a bottleneck they can address.

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
