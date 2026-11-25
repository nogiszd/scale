# Roadmap

This file defines milestone acceptance criteria and records their state. Milestone numbers are
deliverables, not a strict serial order. Tasks and owners are in
[TASKS.md](TASKS.md). Verified results are in [PROGRESS.md](PROGRESS.md).

| Milestone | Scope | Tasks | State |
|---|---|---|---|
| M0 Foundation | CMake/presets, CLI, diagnostics contract, GoogleTest + CTest | T-000, T-001 | done |
| M1 PE inspector | Read-only bounded PE32+ AMD64 inspection, dumpbin comparison, `scale inspect` | T-101..T-103 | done |
| M2 Native execution slice | Supervised worker, image mapping, `scale run` + report.json, unresolved-import diagnostics | T-201..T-204 | done |
| M3 Imports and Windows runtime fidelity | Exception tables / unwind, TLS, import binding through adapters, delay load, CRT startup, DLL init | T-301–T-308 | in progress: exception directory done (T-301), TLS next (T-302) |
| M4 Adapters and filesystem | Adapter registry, title root + writable data root, path translation, title metadata | T-401–T-408 | backlog |
| M5 Integration and diagnostics | report.json completeness, synthetic test title | T-501–T-504 | not started |
| M6 Graphics slice | Vulkan compute harness (done), verified triangle, host presentation and performance overlay | T-601–T-607 | compute done, triangle not started |
| M7 RDNA2 shader recompiler | ABI contracts, decoder, IR, SPIR-V, reference interpreter, GPU differential tests | T-701..T-740 | ABI v0 and v1 verified on GPU, `scale-shader` CLI done; ABI v2 contract frozen (T-730), decoder/IR/interpreter/GPU cases next (T-731..T-734) |
| M8 Graphics integration | Translated shaders → pipeline creation, shader and pipeline caches, pipeline warm-up, title window | T-803–T-810 | not started |
| M9 Audio, input, local data | Interfaces demanded by verified tests | T-901–T-904 | not started |
| M10 Retail compatibility baseline | Criteria below | T-1001 | not started |
| infra Portability | Windows first, Linux-ready (decision 0004) | T-801, T-802 | groundwork done; Linux CI blocked |

## Acceptance criteria and integration gates

### M0 — Foundation

Create the minimal CMake project, CLI, README, documentation and CTest setup. Provide Debug/Release presets and reproducible build commands. A smoke test must pass on actual Windows x64 before calling this complete. If only a non-Windows environment is available, mark Windows execution unverified.

### M1 — PE inspector (read-only)

Implement a bounded PE32+ AMD64 inspector using documented PE/COFF structures. Validate file offsets, RVAs, section ranges, sizes, directory bounds and architecture. Report entry point, image base, sections, imports (names/ordinals), relocations, TLS and exception directory presence. Add delay-import inspection when needed. Reject malformed/truncated and unsupported inputs with precise diagnostics. Compare self-built test executable metadata against dumpbin or another independent Windows tool. No execution in this milestone.

Proposed CLI: `scale inspect <file> [--json]`.

### M2 — Native execution vertical slice

Start with a small, trusted, self-authored PE test executable with a deliberately simple entry contract, known return value, initialized data and a documented minimal dependency set. Establish a Windows-native baseline using the same test executable where applicable. Implement the minimum image loading and entry invocation necessary for this test executable, respecting section permissions, image bounds and relocation requirements. Use a dedicated worker; capture exit code, output, timeout and exceptions. Validate a second test executable that exercises relocated data. Explicitly reject unsupported directories and initialization requirements rather than claiming a general EXE loader.

Distinguish a custom test executable entry function from a standard CRT executable entry point; success with the former does not prove support for the latter. Prefer documented OS loader facilities where they satisfy requirements; evaluate custom mapping only for demonstrated console compatibility needs. Record the design decision. Do not add dynamic CPU translation or whole-binary recompilation.

Proposed CLI: `scale run <executable> [-- args]`.

### M3 — Import resolution and Windows runtime fidelity

Expand test executable coverage incrementally to normal imports, imported DLL functions and ordinals, forwarded exports, relocations, delay loading, standard process startup/CRT expectations, TLS callbacks and static TLS across threads, x64 unwind/SEH and C++ exceptions, module lifetime and DLL initialization. Treat each as a separate feature with a test. Respect Windows x64 ABI, stack alignment, unwind metadata, process/thread semantics and documented initialization rules. Unsupported combinations must fail clearly.

Resolver policy: explicit documented adapter -> guest DLL from a configured module root -> explicitly permitted compatible Windows export -> unresolved error. Match signatures, structure layout, semantics and ownership, not merely names. Avoid arbitrary DLL search paths. Use a test DLL with a synthetic platform-only API to test substitution and error reporting. Do not assume that PC GDK exports or Win32 APIs are ABI-compatible with console counterparts. Never generate a callable stub for an unknown signature.

Acceptance: multiple test executables execute with expected results, including normal Windows reference runs where applicable. Negative tests demonstrate predictable rejection. TLS and exceptions are not considered implemented until exercised.

### M4 — System adapters and filesystem

Implement a small documented adapter registry and versioned evidence records. Start with self-authored platform-like tests. Provide SCALE-owned implementations of console module exports as their behavior becomes known. Support explicitly supplied guest DLLs through the M3 loader path when their imports and initialization are supported. Add documented file I/O, synchronization, memory and threading adaptations only when justified by tests or evidence. Define a title root and separate writable data root; translate paths deterministically, reject traversal and protect originals. Test reads, writes, missing files and path handling. Implement a writable overlay only when necessary. Console kernelx and GDK adapters require verified signatures and behavioral contracts. Never fabricate license or service success.

Resolve a display name for the selected executable from local title metadata and verify the fallback order with self-authored fixtures. See [T-405](research/TRANSLATION_GAPS.md#t-405--read-local-title-identity-and-executable-metadata).

### M5 — Application integration and diagnostics

Add `report.json` with loader stage, import resolutions, unsupported operations, exit result, test executable identity and timing. Keep reports free of secrets. Build a small headless test title from source that uses synthetic adapter imports, multiple DLLs, memory, threads and resource files. Compare observed results with a native reference or explicit golden expectations. A passing synthetic test title proves the exercised host features, not Xbox title support. If no console test executable is available, continue synthetic work and record the retail tests as not run.

### M6 — Graphics vertical slice

Start a small host graphics interface once the first native execution slice and shared diagnostic contracts are stable. Develop and test host GPU plumbing independently while CPU/runtime fidelity expands. Vulkan is the planned initial backend, with a later optional D3D12 backend. Revisit this decision if verified console API evidence favors D3D12; avoid implementing both simultaneously. First run a known PC shader and simple compute/triangle test program through a synthetic graphics adapter, validating readback or expected pixels. This tests host graphics plumbing, not D3D12.X compatibility. Map actual console graphics objects, ABI, descriptors, resources, GPU memory and synchronization only with evidence. Do not assume a one-to-one Vulkan mapping.

Present the synthetic scene in a Windows host window. Verify a toggleable overlay that shows FPS, the most recent frame time in milliseconds and a short frame-time graph. See [T-605](research/TRANSLATION_GAPS.md#t-605--present-and-resize-a-host-window) and [T-607](research/TRANSLATION_GAPS.md#t-607--show-frame-pacing-in-the-host-window).

### M7 — RDNA2 shader recompiler

Build a standalone core module in parallel with CPU/runtime and graphics work after the initial native execution slice. Shader recompilation is a mandatory project deliverable, not an optional research task. Do not wait for retail binaries or a complete platform runtime. Start with standard gfx1030 compute instructions, not an assumed Xbox ISA or proprietary container format. Use AMD public ISA specifications and LLVM-generated binaries. Require explicit synthetic shader ABI metadata (register inputs, wave size, buffers and output mapping). Architecture: decoder -> small typed IR -> SPIR-V emitter -> validation -> Vulkan execution. An independent CPU interpreter evaluates decoded instructions directly; do not share semantic lowering code with the compiler.

Implement a first end-to-end arithmetic program, then extend on demand: control flow, EXEC masks, wave operations, memory and resources. Account for inactive lanes, overflow, flags, floating-point modes and subgroup capabilities. Unsupported instructions or capabilities must produce errors. Never substitute a no-op or zero. Keep console container parsing and Xbox-specific ISA extensions separate and evidence-driven.

Acceptance: decoder agrees with independent LLVM disassembly; hand-authored expected values agree with the interpreter; validated SPIR-V actually runs on Vulkan hardware and observable results agree with the reference. Validation alone is not proof of correctness. Record device, driver, tool versions and test seeds. Test software Vulkan separately and label it as such.

### M8 — Graphics integration

Connect shader compiler to graphics pipeline creation with explicit resource/ABI mapping. Use deterministic compute and rendering tests, then evidence-backed title traces if available. Add cache only after correct translation, with keys covering binary, ABI, translator version and relevant backend configuration. Do not claim graphical correctness from presents or non-black pixels alone.

When the integrated path presents a running title in a host window, show `SCALE — <display name>` in the title bar and provide the M6 performance overlay in that window. Verify both on a synthetic title through the integrated path. See [T-809](research/TRANSLATION_GAPS.md#t-809--show-the-game-name-and-performance-overlay).

Queue pipeline builds ahead of first use where the guest API permits it and warm up previously used pipelines on subsequent runs. Compare cold and warm runs of the same scene for rendering equivalence, time the title waits for a pipeline and time to reach the scene. See [T-810](research/TRANSLATION_GAPS.md#t-810--warm-up-previously-used-vulkan-pipelines).

### M9 — Audio, input and local data

Implement only interfaces demanded by verified test titles; start with public host APIs and deterministic tests. Handle offline behavior accurately, return documented unsupported errors, and never bypass authentication or licensing.

### M10 — Retail compatibility: baseline, triage and iteration

We recommend starting systematic retail testing once SCALE meets the M10 baseline below. Earlier experiments with game files you obtained yourself can help identify gaps. Record the build, observed results and missing capabilities so the experiment is reproducible.

- Native loader: trusted PE32+ test executables covering imports, relocation, ordinary startup, TLS, unwind/SEH and DLL initialization pass; unsupported cases are rejected predictably.
- Platform runtime: documented adapters and title filesystem are tested, with explicit unsupported behavior and reliable diagnostic reporting.
- Graphics: integrated synthetic application creates resources and pipelines, submits work, synchronizes and produces verified readback/render output.
- Shader recompiler: representative compute and vertex/pixel shaders are translated and tested on actual GPU hardware; instruction and ABI limitations are enumerated. Complete instruction coverage is not a prerequisite.
- Audio, input and local data: baseline synthetic tests pass; unavailable services are accurately reported rather than impersonated.
- Integration: one self-authored test title exercises multiple components together with repeatable results; crash/timeout reports are captured.
- Test material: use a readable retail executable and its required resources that you obtained yourself. If test files are unavailable, continue synthetic integration and report retail testing as not run.

The baseline covers representative APIs and GPU instructions. Use retail test results to determine compatibility and prioritize the gaps encountered.

#### Evidence-driven compatibility loop

1. Inspect the title and record its identity/build, imports, architecture and known unsupported features. Preserve original files.
2. Run a title you obtained yourself in a controlled test environment; capture stage transitions, missing dependencies, unsupported API/graphics/shader operations, crashes and timeouts.
3. Group repeated diagnostics into actionable blockers and identify the earliest confirmed failure on the observed path. Do not assume the first warning is the root cause.
4. Consult public documentation and public reference material. Record what is known, what is inferred and what remains unknown. Prioritize blockers that prevent further execution or affect multiple titles.
5. Implement the smallest reusable, evidence-backed fix in the responsible module. Never guess unknown function signatures or return fake success to advance execution.
6. Add a reproducible synthetic regression test for the discovered behavior whenever possible. Run the affected module tests, then repeat the title test and compare progress.
7. Update the compatibility report, limitations and next blocker. Repeat incrementally; prefer general fixes over per-title patches. Any justified title-specific workaround must be explicit, documented and independently tested.

Do not auto-generate executable shims from missing function names. Diagnostics guide investigation, not autonomous implementation. Do not continue execution past a required unknown import or an unsupported operation whose effects cannot safely be represented. An unsupported optional operation may return a documented error when the ABI and semantics are known.

#### Diagnostic contract (available from early milestones)

Keep a human-readable console log and versioned machine-readable report. Include, as applicable:

- Run ID, title/test executable identity, build/hash, runtime version, backend and host capabilities.
- Execution stage, module load events, entry/exit, elapsed time, crash/timeout and exception details.
- Unresolved imports: module, symbol or ordinal, importing module, import kind and resolution failure.
- Unsupported runtime calls: known interface/function, relevant version, documented status and module-relative call site when safely available.
- Graphics failures: operation/interface, pipeline or resource identifier, unsupported format/capability and backend error.
- Shader failures: shader hash, stage, ISA target, instruction offset/opcode, unsupported modifier or ABI feature, compiler stage and validation error. Include only bounded, non-proprietary diagnostic excerpts when authorized.
- Deduplicated occurrence counts, first/last occurrence, severity, fatal/nonfatal classification and the observed blocker. Keep detailed traces opt-in and bounded; do not log secrets or raw title assets.

Example diagnostic event (illustrative schema, not a known Xbox API):

```json
{
  "schema_version": 1,
  "component": "runtime",
  "stage": "import_resolution",
  "code": "unresolved_import",
  "module": "test_platform.dll",
  "symbol": "TestAllocate",
  "severity": "error",
  "fatal": true,
  "count": 1
}
```

Store local run reports outside version control by default. Provide a concise summary of blockers at the end of each run. A simple JSON report and structured logging are sufficient; no database, AI inference engine or elaborate tracing framework is required initially.

#### Compatibility and performance reporting

Track per-title observed stages: inspected, loaded, entry reached, initialized, menu, rendering, gameplay. Distinguish NOT_TESTED, BLOCKED, FAILED and PASS for each tested stage; include the test environment and known limitations. Never label a title playable solely because its entry point or menu works. Record actual evidence and report regressions. Profile startup, frame times, memory and shader compile costs after correctness. Optimize only measured bottlenecks. Keep retail files and private traces out of the repository.


