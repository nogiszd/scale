# Scale — Scarlett Compatibility and Layered Execution

## Mission and scope

Build an independent Windows x64 compatibility layer for Xbox Series X|S software. Execute compatible AMD64 machine code directly on the host CPU, supply missing platform APIs and eventually translate graphics and native GPU shaders. This is a full compatibility layer, not merely a PE loader or shader compiler. Native CPU execution, system/runtime fidelity, graphics translation and shader recompilation are equally essential pillars. CPU-first describes the initial implementation order only, not the relative importance of modules. The first deliverable is a native execution proof of concept using self-authored PE32+ fixtures. Develop graphics and shader recompilation in parallel as soon as the foundation is stable. Retail compatibility is the integration goal: begin retail testing once the essential modules meet their baseline acceptance criteria, then improve support using observed title-specific blockers. Broad retail compatibility is not guaranteed.

Use C++23, MSVC, CMake and Ninja. Use public documentation and lawfully accessible test material only. XCL is an architectural reference, not a code dependency. Do not copy its implementation or redistribute proprietary SDK files or games. Do not implement package decryption, DRM bypass, entitlement spoofing, or online service impersonation. Do not invent undocumented ABI behavior. All code, comments, documentation and CLI output are in English.

## Agent workflow

- Read this file and docs/PROGRESS.md before changes. Inspect the actual repository and toolchain; do not assume components exist.
- Work on one small vertical slice at a time. Build and run relevant tests before advancing. Record exact commands, results, skips and blockers.
- Prefer standard C++ and Windows SDK APIs over dependencies. Introduce a library only for a demonstrated need. No speculative abstractions or empty module trees.
- Do not modify original title files. Do not execute untrusted binaries; the native host is not a sandbox. Start with repository-built fixtures only, without administrator privileges. Use a disposable VM for unfamiliar binaries when appropriate.
- No silent fallbacks, dummy successes, blanket stubs, or fabricated test results. An unresolved required import must prevent execution. Report unsupported behavior explicitly.
- Use RAII, checked arithmetic, bounded parsing, explicit ownership and structured errors. Preserve ASLR, DEP and other applicable mitigations; do not disable system security features to make tests pass.
- Keep commits optional and only make them when requested. Never overwrite unrelated changes.
- If blocked on missing console evidence, document the exact unknown and continue on independent tasks. Ask the user only when their intervention is actually needed.

## Architecture and repository

The eventual repository contains all components, with independently buildable libraries. Create directories only when needed.

```text
scale/
  AGENTS.md
  README.md
  CMakeLists.txt
  CMakePresets.json
  docs/
    ROADMAP.md
    PROGRESS.md
    TASKS.md
    MULTI_AGENT.md
    architecture.md
    research/
      KNOWN_UNKNOWNS.md
      REFERENCES.md
    decisions/
  src/
    common/
    loader/          # PE inspection and native image loading
    host/            # execution lifecycle and worker process
    runtime/         # import resolver, kernel and GDK adapters
    filesystem/      # title mounts and writable overlay
    diagnostics/     # structured reports
    graphics/        # core: console API and host backend
    shader/          # core: RDNA2 decoder, interpreter, IR, SPIR-V
    audio/           # later
    input/           # later
  tools/
    scale/      # CLI
  tests/
    fixtures/        # self-authored source, built locally
    unit/
    integration/
  scripts/
```

Dependency direction: CLI -> host -> loader/runtime; runtime -> filesystem and, later, graphics/audio/input; graphics -> shader compiler. Shader compiler must not depend on the executable loader or runtime. Common utilities must not depend on higher-level components. Avoid cyclic dependencies. Use separate CMake targets and optional build flags for graphics and shader components. Keep the shader CLI and differential test runner usable independently from the native execution host. Do not introduce a dependency from the shader compiler to the graphics runtime; keep Vulkan execution in a test harness or host backend.

## Development tracks and integration gates

After M0 and the first M2 native execution slice, schedule three complementary tracks: CPU and platform runtime (M3-M5), host graphics (M6), and shader translation (M7). M7 does not depend on M6 or completion of M3-M5. Use the same deterministic test policy across tracks. M8 joins graphics and shader translation; M9 completes baseline peripheral and local-data support. Milestone numbers identify deliverables, not a strict serial schedule. Avoid implementing an entire ISA or system API before testing integration.

All core tracks are required for the retail baseline. A passing CPU-only executable is not a successful game compatibility result, and a passing shader test does not establish title compatibility. Keep each track independently testable and report its status separately.

The CLI supervisor starts a separate unprivileged worker process for native execution. This isolates crashes from the supervisor but is not a security boundary. The worker owns guest execution and runtime state. First implement inspection and controlled fixtures; do not attempt arbitrary executable execution until the relevant loader features are tested. Keep original binaries read-only and use a dedicated temporary data root for test writes.

## Windows development environment

Required initially: Windows 11 x64 (Windows 10 support is not an initial requirement), Git, Visual Studio C++ Build Tools with MSVC x64 and Windows SDK, CMake and Ninja. Use a Developer PowerShell or x64 Native Tools environment. An installed Visual Studio IDE is optional. Python 3 is optional for scripting; use CTest and small C++ tests initially. Do not install Rust, GDKX, Vulkan, LLVM, Z3 or other large dependencies for the CPU milestone unless there is a concrete need.

Check `git --version`, `cmake --version`, `ninja --version`, `Get-Command cl, link, dumpbin`, and the compiler/SDK architecture. Do not auto-install software or alter machine-wide settings; report missing tools and exact suggested installation steps. Set up Debug and Release presets using Ninja and the initialized MSVC environment. Verify C++23 features actually used are supported by the installed compiler; avoid unsupported features or experimental compiler switches unless justified.

Graphics/shader track (after the first native execution slice): Vulkan SDK and current GPU driver; verify `vulkaninfo --summary` and `spirv-val --version`. Shader track: LLVM tools with AMDGPU support, in particular llvm-mc and llvm-objdump; check `llvm-mc --version` and that AMDGPU is registered. Some Windows distributions do not include all required tools or targets; obtain/build an appropriate distribution if needed. Pin versions and record them. Public PC GDK may be useful later; it is not a substitute for console GDKX.

## Roadmap and acceptance criteria

### M0 — Foundation

Create the minimal CMake project, CLI, README, documentation and CTest setup. Provide Debug/Release presets and reproducible build commands. A smoke test must pass on actual Windows x64 before calling this complete. If only a non-Windows environment is available, mark Windows execution unverified.

### M1 — PE inspector (read-only)

Implement a bounded PE32+ AMD64 inspector using documented PE/COFF structures. Validate file offsets, RVAs, section ranges, sizes, directory bounds and architecture. Report entry point, image base, sections, imports (names/ordinals), relocations, TLS and exception directory presence. Add delay-import inspection when needed. Reject malformed/truncated and unsupported inputs with precise diagnostics. Compare self-built fixture metadata against dumpbin or another independent Windows tool. No execution in this milestone.

Proposed CLI: `scale inspect <file> [--json]`.

### M2 — Native execution vertical slice

Start with a small, trusted, self-authored PE fixture with a deliberately simple entry contract, known return value, initialized data and a documented minimal dependency set. Establish a Windows-native baseline using the same fixture where applicable. Implement the minimum image loading and entry invocation necessary for this fixture, respecting section permissions, image bounds and relocation requirements. Use a dedicated worker; capture exit code, output, timeout and exceptions. Validate a second fixture that exercises relocated data. Explicitly reject unsupported directories and initialization requirements rather than claiming a general EXE loader.

Distinguish a custom fixture entry function from a standard CRT executable entry point; success with the former does not prove support for the latter. Prefer documented OS loader facilities where they satisfy requirements; evaluate custom mapping only for demonstrated console compatibility needs. Record the design decision. Do not add dynamic CPU translation or whole-binary recompilation.

Proposed CLI: `scale run <trusted-fixture.exe> [-- args]`.

### M3 — Import resolution and Windows runtime fidelity

Expand fixture coverage incrementally to normal imports, imported DLL functions and ordinals, forwarded exports, relocations, delay loading, standard process startup/CRT expectations, TLS callbacks and static TLS across threads, x64 unwind/SEH and C++ exceptions, module lifetime and DLL initialization. Treat each as a separate feature with a test. Respect Windows x64 ABI, stack alignment, unwind metadata, process/thread semantics and documented initialization rules. Unsupported combinations must fail clearly.

Resolver policy: explicit documented adapter -> approved guest module -> explicitly permitted compatible Windows export -> unresolved error. Match signatures, structure layout, semantics and ownership, not merely names. Avoid arbitrary DLL search paths. Use a fixture DLL with a synthetic platform-only API to test substitution and error reporting. Do not assume that PC GDK exports or Win32 APIs are ABI-compatible with console counterparts. Never generate a callable stub for an unknown signature.

Acceptance: multiple fixture executables execute with expected results, including normal Windows reference runs where applicable. Negative tests demonstrate predictable rejection. TLS and exceptions are not considered implemented until exercised.

### M4 — System adapters and filesystem

Implement a small documented adapter registry and versioned evidence records. Use self-authored platform-like fixtures before real console modules. Add documented file I/O, synchronization, memory and threading adaptations only when justified by tests or evidence. Define a title root and separate writable data root; translate paths deterministically, reject traversal and protect originals. Test reads, writes, missing files and path handling. Implement a writable overlay only when necessary. Console kernelx and GDK adapters require verified signatures and behavioral contracts. Never fabricate license or service success.

### M5 — Application integration and diagnostics

Add `report.json` with loader stage, import resolutions, unsupported operations, exit result, fixture identity and timing. Keep reports free of secrets. Build a small headless test title from source that uses synthetic adapter imports, multiple DLLs, memory, threads and resource files. Compare observed results with a native reference or explicit golden expectations. A passing synthetic test title proves the exercised host features, not Xbox title support. If lawful console test binaries are unavailable, report the real-title track as blocked while continuing synthetic work.

### M6 — Graphics vertical slice

Start a small host graphics interface once the first native execution slice and shared diagnostic contracts are stable. Develop and test host GPU plumbing independently while CPU/runtime fidelity expands. Vulkan is the planned initial backend, with a later optional D3D12 backend. Revisit this decision if verified console API evidence favors D3D12; avoid implementing both simultaneously. First run a known PC shader and simple compute/triangle fixture through a synthetic graphics adapter, validating readback or expected pixels. This tests host graphics plumbing, not D3D12.X compatibility. Map actual console graphics objects, ABI, descriptors, resources, GPU memory and synchronization only with evidence. Do not assume a one-to-one Vulkan mapping.

### M7 — RDNA2 shader recompiler

Build a standalone core module in parallel with CPU/runtime and graphics work after the initial native execution slice. Shader recompilation is a mandatory project deliverable, not an optional research task. Do not wait for retail binaries or a complete platform runtime. Start with standard gfx1030 compute instructions, not an assumed Xbox ISA or proprietary container format. Use AMD public ISA specifications and LLVM-generated binaries. Require explicit synthetic shader ABI metadata (register inputs, wave size, buffers and output mapping). Architecture: decoder -> small typed IR -> SPIR-V emitter -> validation -> Vulkan execution. An independent CPU interpreter evaluates decoded instructions directly; do not share semantic lowering code with the compiler.

Implement a first end-to-end arithmetic program, then extend on demand: control flow, EXEC masks, wave operations, memory and resources. Account for inactive lanes, overflow, flags, floating-point modes and subgroup capabilities. Unsupported instructions or capabilities must produce errors. Never substitute a no-op or zero. Keep console container parsing and Xbox-specific ISA extensions separate and evidence-driven.

Acceptance: decoder agrees with independent LLVM disassembly; hand-authored expected values agree with the interpreter; validated SPIR-V actually runs on Vulkan hardware and observable results agree with the reference. Validation alone is not proof of correctness. Record device, driver, tool versions and test seeds. Test software Vulkan separately and label it as such.

### M8 — Graphics integration

Connect shader compiler to graphics pipeline creation with explicit resource/ABI mapping. Use deterministic compute and rendering fixtures, then evidence-backed title traces if available. Add cache only after correct translation, with keys covering binary, ABI, translator version and relevant backend configuration. Do not claim graphical correctness from presents or non-black pixels alone.

### M9 — Audio, input and local data

Implement only interfaces demanded by verified test titles; start with public host APIs and deterministic fixtures. Handle offline behavior accurately, return documented unsupported errors, and never bypass authentication or licensing.

### M10 — Retail compatibility: baseline, triage and iteration

Retail compatibility is the intended integration phase, not a shortcut around unfinished core modules. Start only after the following baseline is demonstrated:

- Native loader: trusted PE32+ fixtures covering imports, relocation, ordinary startup, TLS, unwind/SEH and DLL initialization pass; unsupported cases are rejected predictably.
- Platform runtime: documented adapters and title filesystem are tested, with explicit unsupported behavior and reliable diagnostic reporting.
- Graphics: integrated synthetic application creates resources and pipelines, submits work, synchronizes and produces verified readback/render output.
- Shader recompiler: representative compute and vertex/pixel fixtures are translated and tested on actual GPU hardware; instruction and ABI limitations are enumerated. Complete instruction coverage is not a prerequisite.
- Audio, input and local data: baseline synthetic fixtures pass; unavailable services are accurately reported rather than impersonated.
- Integration: one self-authored test title exercises multiple components together with repeatable results; crash/timeout reports are captured.
- Test material: an authorized, readable retail executable and its required resources are available for testing. If unavailable, retail execution remains BLOCKED; continue synthetic integration and documented compatibility research. Do not obtain unauthorized game dumps or bypass access controls.

This gate is a readiness baseline, not proof of retail compatibility. Do not require every possible API or GPU instruction to be implemented before the first authorized retail test. The purpose of the next phase is to discover real gaps, as in XCL's reported development approach.

#### Evidence-driven compatibility loop

1. Inspect the title and record its identity/build, imports, architecture and known unsupported features. Preserve original files.
2. Run an authorized title in a controlled test environment; capture stage transitions, missing dependencies, unsupported API/graphics/shader operations, crashes and timeouts.
3. Group repeated diagnostics into actionable blockers and identify the earliest confirmed failure on the observed path. Do not assume the first warning is the root cause.
4. Consult public documentation and lawful reference material. Record what is known, what is inferred and what remains unknown. Prioritize blockers that prevent further execution or affect multiple titles.
5. Implement the smallest reusable, evidence-backed fix in the responsible module. Never guess unknown function signatures or return fake success to advance execution.
6. Add a reproducible synthetic regression test for the discovered behavior whenever possible. Run the affected module tests, then repeat the title test and compare progress.
7. Update the compatibility report, limitations and next blocker. Repeat incrementally; prefer general fixes over per-title patches. Any justified title-specific workaround must be explicit, documented and independently tested.

Do not auto-generate executable shims from missing function names. Diagnostics guide investigation, not autonomous implementation. Do not continue execution past a required unknown import or an unsupported operation whose effects cannot safely be represented. An unsupported optional operation may return a documented error when the ABI and semantics are known.

#### Diagnostic contract (available from early milestones)

Keep a human-readable console log and versioned machine-readable report. Include, as applicable:

- Run ID, title/fixture identity, build/hash, runtime version, backend and host capabilities.
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
  "module": "fixture_platform.dll",
  "symbol": "FixtureAllocate",
  "severity": "error",
  "fatal": true,
  "count": 1
}
```

Store local run reports outside version control by default. Provide a concise summary of blockers at the end of each run. A simple JSON report and structured logging are sufficient; no database, AI inference engine or elaborate tracing framework is required initially.

#### Compatibility and performance reporting

Track per-title observed stages: inspected, loaded, entry reached, initialized, menu, rendering, gameplay. Distinguish NOT_TESTED, BLOCKED, FAILED and PASS for each tested stage; include the test environment and known limitations. Never label a title playable solely because its entry point or menu works. Record actual evidence and report regressions. Profile startup, frame times, memory and shader compile costs after correctness. Optimize only measured bottlenecks. Keep retail files and private traces out of the repository.

## Lean testing policy

Use CTest as the single runner. Small C++ assertion/test executables are sufficient initially; add GoogleTest only if it reduces complexity. Maintain tests close to features, with deterministic fixtures built by CMake and bounded timeouts. Never commit proprietary binaries. Store regression fixtures as source or reproducible synthetic data.

CPU tests: compare trusted fixture execution under normal Windows loading and the compatibility host, where contracts match. Check outputs, exit codes, file effects and observable behavior, not identical addresses or scheduling. Include malformed PE, missing imports, unsupported features and worker failure. A fixture using a synthetic console API needs an independently specified golden behavior; a normal Windows execution is not an Xbox oracle.

Shader tests (as soon as the shader track begins): use one data-driven differential runner. Each supported operation has a small set of manual expected results plus deterministic generated inputs. Compare independent CPU interpretation and actual GPU readback, using exact integer values and specification-based float tolerances/bit rules. Add short multi-instruction programs to detect interactions. Use a simple seedable generator only after the first working shader. Missing GPU tests are SKIPPED, never PASS; required milestone GPU acceptance remains incomplete until hardware execution is verified.

CTest labels: `quick` for small CPU tests, `integration` for worker and runtime fixtures, `gpu` for hardware tests. Run relevant tests after each feature; full suite before milestone completion. No mandatory thousands-of-tests quota, custom fuzz framework, separate regression framework or benchmark suite at startup.

## Evidence and research policy

Use public PE/COFF and Windows x64 ABI documentation; public GDK docs are useful context but do not establish console binary compatibility. Use public AMD ISA documentation, LLVM and Khronos specs for graphics. Record source URLs, revision/commit where applicable, observed behavior, confidence and limitations. Separate verified fact, hypothesis and missing information.

XCL demonstrates a related architecture in Rust and reports a custom loader and synthetic title tests. Its public documentation can inform questions to investigate, but do not copy code or treat its observations as independently validated facts. We do not know the provenance of its private retail test material. No dependency on its game files or confidential SDK headers.

Deferred research, not acceptance requirements: SMT/Z3 translation validation, superoptimization, equality saturation, delta debugging, advanced fuzzing, predictive compilation, automated API prioritization. Revisit only when a measured problem warrants the cost. No autonomous guessing of unknown functions or automatic application of inferred ABI adapters.

References:

- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://learn.microsoft.com/en-us/cpp/build/x64-calling-convention
- https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64
- https://github.com/microsoft/GDK
- https://github.com/microsoft/Xbox-GDK-Samples
- https://github.com/ivall/xcl
- https://gpuopen.com/machine-readable-isa/
- https://llvm.org/docs/AMDGPUUsage.html
- https://registry.khronos.org/vulkan/
- https://github.com/KhronosGroup/SPIRV-Tools

## Documentation and definition of done

Keep README build/use instructions accurate; docs/ROADMAP.md records milestone scope; docs/PROGRESS.md records completed work, next task, commands and actual outcomes; docs/research/KNOWN_UNKNOWNS.md records missing console-specific evidence. Use short decision records only for significant architecture choices. Do not build a documentation bureaucracy.

A feature is done when its contract is documented, implementation builds, appropriate positive and negative tests have run, and limitations are explicit. A milestone cannot be marked complete on unexecuted tests. Prefer a small working vertical slice to broad unverified coverage.

## Initial task

Start with M0 and M1, then implement the smallest M2 native execution fixture and its reference comparison. Establish the diagnostic event/report contract early and demonstrate a missing synthetic import diagnostic. Report actual results and the next concrete tasks for the CPU/runtime, graphics and shader tracks. After this initial slice, advance those tracks in parallel or alternate small vertical slices; do not postpone shader work until the entire CPU runtime is complete. The first shader slice must translate a small standard RDNA2 compute program to SPIR-V and compare real GPU readback with an independent CPU reference and manual expected results. Do not implement speculative Xbox APIs, a GUI, or retail binary testing in the initial task. Retail testing begins only after the M10 readiness gate is met.
