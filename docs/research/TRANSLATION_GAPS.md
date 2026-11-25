# Translation gap review

Code baseline: SCALE `c4b49de587d92b2f83c5db43355e7f0d0ce55981` on `integration`. This is a source
review. The [progress log](../PROGRESS.md) records the executed tests and their
results. Each proposal gives a small behavior to verify and keeps any
console-specific ABI claims tied to evidence.

## What SCALE has today

| Area | Verified or present in the baseline | Gap after the first backlog |
|---|---|---|
| Loader | Bounded PE32+ inspection, mapping and unwind registration in a supervised worker. `resolve_imports` reports every required import unresolved. | The previous backlog covers import binding, DLL loading and CRT startup. Add module/export lookup at runtime and inspect a larger known build. |
| Runtime | No implemented console adapter registry or title filesystem. | The first adapter and mounts have tasks. Add title metadata, async completions, memory lifecycle and a baseline asset queue. |
| Graphics | Vulkan submits a compute dispatch and reads back a buffer. The host triangle and translated pipeline have tasks. | Add textures, resource transitions, submission, aliasing and presentation on the host, then map confirmed console objects and bindings. Verify the performance overlay on the synthetic scene before connecting it to a title's window. |
| Shader translation | Synthetic ABI v0 and v1 tests reported on RX 9070 XT. ABI v2 is specified and its implementation is tracked. The current ABI models one logical wave32. | Add one container form and explicit stage/resource metadata, then test image sampling, LDS, wave64 and one atomic operation. |
| Integration and services | The run report and the executable, shader and GPU test paths are separate. | Correlate failures by stage, add controlled sample comparisons, and take the synthetic input and audio adapters through real host backends. |

## Suggested order

1. Start T-503 and T-502 alongside the existing M3 work. A PC GDK sample can
   exercise the inspector before a normal CRT executable can run in SCALE.
   Run T-504 in parallel only when a Scarlett binary is available.
2. Continue the Vulkan host path from T-602 to T-603 and T-604. Build
   T-737 against that reference and use T-805 to join translation with
   resource binding. T-736 and T-804 can investigate real formats and ABIs
   in parallel with synthetic work as evidence becomes available.
   After T-605, verify the overlay in T-607. T-809 uses that overlay and
   the T-405 display name when the integrated graphics path presents a title.
3. Start with the smallest real behavior per title. Prioritize recorded
   command replay, tiling and streaming when the selected sample exercises
   them. Consider mesh/geometry, wave64 fallback on wave32-only devices,
   codecs, compressed streaming, ray tracing and upscaling when a verified
   blocker requires them. Rendering a synthetic triangle does not establish
   support for any of those paths.

## Evidence from other implementations

| ID | What we checked | How to use it in SCALE |
|---|---|---|
| R1 | [Wine loader source](https://github.com/wine-mirror/wine/blob/a79470449d0388ed3b5ff7975a4cefd6a3d80a2a/dlls/ntdll/loader.c), [XCL loader](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/loader.rs) | Reference for module lifetime and export lookup. Use the native Windows behavior of a self-built test as the comparison. |
| R2 | [XCL task queues](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/xgameruntime/taskqueue.rs), [Microsoft async model](https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/async/async-programming-model), [MicrosoftGame.config reference](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/microsoftgameconfig/microsoftgameconfig-schema) | Document work and completion ordering and parse evidenced title metadata. The PC API is a behavior reference for the tested subset. |
| R3 | [XCL storage adapter](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/dstorage.rs), [Microsoft console DirectStorage overview](https://learn.microsoft.com/en-us/xbox/gdk/docs/features/console/storage/directstorage/directstorage-overview) | Verify queue semantics using plain bytes before implementing specific codecs or hardware paths. |
| R4 | [XCL placed resources](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/d3d12x/vram.rs), [XCL tiling](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/gpu/tiling.rs) | A title can reuse an address across resource objects, so check aliasing explicitly. Compare layout behavior with the underlying AMD surface layout where applicable. |
| R5 | [XCL README and run-report description](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/README.md) | Report the actual stopping stage and relevant capability. XCL's game and GPU results are reported by its maintainers. |
| R6 | [Xbox GDK Samples](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/README.md), [Microsoft’s GDK/GDKX target documentation](https://learn.microsoft.com/en-us/gaming/gdk/docs/gdk-dev/console-dev/overviews/gdkx-new-layout) | Pick a specific sample, build target and sample revision. The sample matrix below distinguishes PC from Scarlett. |
| R7 | [vkd3d-proton command tests](https://github.com/HansKristian-Work/vkd3d-proton/blob/ec20719452e916baa2b9e69f33f18de4561a8313/tests/d3d12_command.c), [descriptor tests](https://github.com/HansKristian-Work/vkd3d-proton/blob/ec20719452e916baa2b9e69f33f18de4561a8313/tests/d3d12_descriptors.c), [Vulkan sync examples](https://docs.vulkan.org/guide/latest/synchronization_examples.html) | Use D3D12 resource and queue behavior as a comparison and Khronos for Vulkan synchronization. This supplies no console ABI evidence. |
| R8 | [XCL shader implementation](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/gpu/dxil/mod.rs), [XCL wave64 analysis](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/docs/wave64-on-wave32.md), [AMD ISA](https://gpuopen.com/machine-readable-isa/), [LLVM AMDGPU](https://llvm.org/docs/AMDGPUUsage.html), [Vulkan subgroups](https://docs.vulkan.org/guide/latest/subgroups.html) | XCL emits DXIL. Verify SCALE's SPIR-V behavior with ISA references, hand-computed checks and Vulkan execution. Confirm a readable container against actual evidence. |
| R9 | [XCL command list source](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/d3d12x/console_list.rs), [XCL graphics adapter](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/d3d12x/real.rs), [WinDurango](https://github.com/WinDurango/WinDurango/blob/e37f6c26132e73c3effe383e07de08db818dc4c6/README.md) | XCL provides examples of console-facing objects and command replay. WinDurango targets Xbox One ERA and D3D11.X and provides historical context for platform adapters. Verify the Series X\|S interface independently. |
| R10 | [vkd3d-proton cache](https://github.com/HansKristian-Work/vkd3d-proton/blob/ec20719452e916baa2b9e69f33f18de4561a8313/README.md#shader-cache), [SCALE decision 0005](../decisions/0005-shader-translation-timing.md) | Separate translated shader artifacts from driver-specific pipeline objects. Start with reproducible cold/hit equivalence. |
| R11 | [XCL GameInput](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/gameinput.rs), [XCL XAudio2](https://github.com/xcl-dev/xcl/blob/568e7db7729c8fa6c8f0f4b32d798faaf11462bb/crates/xcl/src/hle/xaudio2.rs) | Inspect which semantics XCL maps to host services, then test the small subset SCALE actually implements. |

## GDK sample verification plan

The target configurations were checked in the sample project files at the
revision linked below. Builds and runs require a prepared Windows environment.
The public PC GDK is enough for a PC build. The Scarlett target requires GDKX.
Use a devkit when a matching native Scarlett run is available as a reference.

| Sample | Verified target in the project file | What to check |
|---|---|---|
| [Gamepad](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/Samples/System/Gamepad/Gamepad.vcxproj) | Windows `x64`, `Gaming.Xbox.Scarlett.x64` | Start on PC. Compare PE headers and imports and record native controller behavior for T-903. Inspect the Scarlett build separately when available. |
| [SimpleTriangle](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/Samples/IntroGraphics/SimpleTriangle/SimpleTriangle.vcxproj) | `Gaming.Xbox.Scarlett.x64` | Inspect imports and capture a native draw baseline if possible. Compare the selected device-to-draw path in T-804/T-805. |
| [SimpleCompute](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/Samples/IntroGraphics/SimpleCompute/SimpleCompute.vcxproj) | `Gaming.Xbox.Scarlett.x64` | Inspect the executable and, when exposed as readable shader data, record stage and resource metadata. Compare a known compute result once the required ABI is supported. |
| [SimpleDeviceAndSwapChain](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/Samples/IntroGraphics/SimpleDeviceAndSwapChain/SimpleDeviceAndSwapChain.vcxproj) | `Gaming.Xbox.Scarlett.x64` | Reference for device creation and PresentX. Keep the host Vulkan presentation test in T-605 separate. |
| [SimplePlaySound](https://github.com/microsoft/Xbox-GDK-Samples/blob/b1e77b6ac10f95b07296025fda171ca2ddc42dbe/Samples/Audio/SimplePlaySound/SimplePlaySound.vcxproj) | `Gaming.Xbox.Scarlett.x64` | Use a short PCM clip as the first comparison once the required console audio exports have evidence. |

For each run record the sample revision, GDK or GDKX version, target platform,
configuration, executable hash, commands and result. Begin with read-only
inspection and compare against `dumpbin /headers /imports <exe>`. Keep native PC
output, native Scarlett output and SCALE's output in separate result fields.
A matching PC metadata dump verifies PE inspection on that PC build. A SCALE
execution blocked at a required import identifies the next missing capability.
Record missing tools or hardware as SKIPPED. Keep the SDK, binaries and sample
assets in the external checkout. Tests in SCALE should use their own source or
synthetic inputs.

## New task specifications

See the [task registry](../TASKS.md) for the proposed IDs, milestones and
dependencies. The acceptance checks below apply to each task's first slice.

### T-308 — Look up guest modules and exports at runtime

Expose a guest module handle and a name/ordinal lookup using the existing resolver policy. A self-built EXE must find an already loaded DLL and call its export. Missing modules, missing symbols and handles used after unload must return defined errors. Compare observable results with the Windows loader. [R1]

### T-405 — Read local title identity and executable metadata

Read an explicit executable and the supported fields of a local `MicrosoftGame.config`. Use synthetic files to test missing or ambiguous executable entries, malformed metadata and executable paths outside the title root. Report stable title identity and PE hash without treating configuration as a service entitlement. [R2]

Resolve a display name for the selected executable in this order.

1. `OverrideDisplayName` from the matching `Executable` entry in `MicrosoftGame.config`.
2. `ShellVisuals.DefaultDisplayName` from the same configuration.
3. `ProductName` from the executable's `VERSIONINFO` resource, when available.
4. The executable's filename, including the extension, without its directory path.

Use bounded, read-only parsing. Skip empty or unusable display-name values,
including unresolved localization references. Missing or unreadable optional
display-name metadata falls through to the next source. Verify the precedence,
per-executable matching, Unicode and fallback behavior with synthetic fixtures.
See the GDK references for [Executable](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/microsoftgameconfig/elements/microsoftgameconfig-element-executable)
and [ShellVisuals](https://learn.microsoft.com/en-us/gaming/gdk/docs/reference/system/microsoftgameconfig/elements/microsoftgameconfig-element-shellvisuals)
and the Win32 reference for [VERSIONINFO](https://learn.microsoft.com/en-us/windows/win32/menurc/versioninfo-resource).

### T-406 — Implement one documented GDK async queue mode

Start with one manually dispatched work/completion queue and one synthetic async operation. Check exactly-once completion, cancellation and shutdown. Compare ordering and results with the public PC GDK reference. Add a console export only when the signature and behavior have separate evidence. [R2]

### T-407 — Read assets through an uncompressed DirectStorage queue

Implement one synthetic file-to-RAM request with an explicit submit and completion result. Compare the output bytes with an ordinary file read. Test short reads, bad bounds and unsupported request options. Use console bindings only once their ABI has evidence. Add compression formats when a sample needs them. [R3]

### T-408 — Model one title memory allocation lifecycle

Pick one allocation/free pair whose contract is evidenced. Verify alignment, write/read, allocation failure and handle validity after free under a bounded synthetic memory budget. Keep graphics placement and aliasing in T-606. [R4]

### T-502 — Record compatibility failures by stage and subsystem

Extend the versioned run report to distinguish module resolution, unsupported service calls, shader failures and GPU submission failures when their producers exist. Test stable reason codes and a bounded report on generated input. Support a metadata-only preflight report for a failed run, including image hash and required imports. [R5]

### T-503 — Verify the PE inspector with a public GDK PC sample

Use the GDK sample plan in this document. Build `Gamepad` as Windows `x64` on an opt-in runner. Compare PE headers and import names and ordinals using `scale inspect --json` and `dumpbin /headers /imports`. Record GDK version, revision, hash, exact commands and any difference. Capture a native baseline for later input tests. [R6]

### T-504 — Inspect a Scarlett GDK sample when a GDKX build is available

Start with `SimpleTriangle` or `Gamepad` built for `Gaming.Xbox.Scarlett.x64`. Compare PE headers and imports with an independent tool and record first unsupported metadata or module. Use a matching devkit run for a behavioral baseline if available. Mark unavailable builds or execution SKIPPED with a reason. [R6]

### T-603 — Draw a textured primitive with host shaders

Upload a small known texture, bind one sampled image plus sampler and draw to an offscreen target. Compare readback against a reference color pattern. Exercise wrong format and row pitch explicitly. This gives T-737 a host-side reference. [R7]

### T-604 — Submit dependent GPU work with correct resource states

Record copy, draw and readback as separate command steps. Check image transitions, fence completion and resource lifetime through submission. Assert exact pixels with Vulkan validation enabled, then test rejection of an invalid resource-state sequence. [R7]

### T-605 — Present and resize a host window

Add a Windows Vulkan presentation path for the synthetic scene. Check resize, occlusion or minimization, swapchain recreation and cleanup after in-flight frames. Establish correctness with offscreen pixel tests and check that successful presentation advances the frame count. [R7, R6]

### T-606 — Check one placed-resource aliasing case

Use two compatible views over a deliberately shared placement and a documented synchronization sequence. Verify the expected write/read result and that an incompatible alignment or memory type is rejected. Establish the rules in a synthetic Vulkan test before mapping console addresses. [R4, R7]

### T-607 — Show frame pacing in the host window

Add a host-controlled overlay that can be shown or hidden without restarting
the synthetic scene. Show rolling FPS, the latest frame time in milliseconds
and a bounded graph of recent frame times. Measure the interval between
consecutive successful host presentation submissions for that window using a
monotonic host clock. Compute rolling FPS as the number of recent intervals
divided by their total elapsed time in seconds. The metric describes host
presentation cadence. Skipped or failed submissions do not produce samples.

Clear the history when presentation is suspended or the swapchain is recreated.
Show metrics as unavailable until two successful submissions establish the
first interval. Verify the calculation using controlled timestamps, including
uneven intervals, failed submissions and a reset. On Windows, use the T-605
scene to verify the toggle, overlay rendering, resize and minimize/restore.

### T-736 — Identify one readable console shader container and its stage metadata

Investigate U-003 using a caller-supplied shader or a pipeline sample with known provenance. Document the observed version, stage, code byte range, wave size and resource metadata where evidence exists. Define a bounded read-only parser for one evidenced form. Verify with synthetic bytes, a truncated record and an unsupported version. Leave unverified fields unknown. [R8]

### T-737 — Translate one texture sampling instruction family

Freeze a synthetic ABI with one image and one sampler, then implement one evidenced gfx1030 sample form. Cross-check instruction bytes with LLVM where supported and compare manual and GPU output for a tiny texture. Include unsupported resource forms and incorrect binding as negative cases. [R8, R7]

### T-738 — Model local data share and a workgroup barrier

Freeze a synthetic LDS contract for one pair of read/write forms and one workgroup synchronization pattern. Two logical waves exchange known words. Check both directions on the CPU reference and Vulkan. Reject unsupported or divergent barrier patterns with a diagnostic before dispatch. [R8]

### T-739 — Verify one wave64 operation across lane 31/32

Separate logical guest wave size from host subgroup size. Verify one cross-lane operation with active and inactive lanes on a supported host. For unsupported subgroup configurations return a capability diagnostic until a fallback has its own tests. Record the host subgroup configuration and GPU model. [R8]

### T-740 — Verify one storage-buffer atomic operation

Freeze scope, ordering and inactive lane behavior for one 32-bit atomic form. Use a multi-lane counter with a deterministic final sum and an ordering-independent check of returned values when the form returns a value. Verify with LLVM where supported, the CPU oracle and Vulkan readback. [R8]

### T-804 — Identify the first console graphics ABI slice

Investigate U-004 using documented calls, public sample code and a Scarlett sample when available. Select a single path such as device creation to one draw. Record verified signatures, object lifetime, method order and resource handoff. Record unresolved details without hard-coding offsets from another title. [R6, R9]

### T-805 — Bind a guest resource to one translated shader pipeline

Start with one evidenced image/sampler binding and an explicit mapping from guest resource identity and shader metadata to Vulkan descriptors. Replay a sampled draw with two different resources and assert the two outputs. Verify object lifetime through completion and precise errors for stale or incompatible bindings. [R7, R9]

### T-806 — Replay one evidenced recorded command sequence

Research whether the selected title or sample records command data directly. Implement replay only for one confirmed sequence, such as bind and draw, through existing SCALE graphics calls. Test offscreen pixel equivalence to the direct path, bounds checks for truncated records and explicit rejection of unknown records. [R9]

### T-807 — Cache verified shader translations with explicit invalidation

Implement the disk shader cache specified in decision 0005. Key on binary, ABI, translator version and all relevant translation state. Verify equivalence between a cold run and a cache hit, invalidation when a key input changes, and safe retranslation on a damaged cache record. [R10]

### T-808 — Untile and upload one evidenced texture layout

Choose one layout observed in a sample and corroborate it with AMD surface layout behavior where applicable. Compare synthetic patterned texels against an independent reference, then sample the uploaded result on Vulkan. Report unsupported layouts explicitly. [R4]

### T-809 — Show the game name and performance overlay

When the integrated graphics path opens the primary window for a running title,
set its title bar to `SCALE — <display name>` using the name resolved in T-405.
Reuse the host toggle and overlay from T-607. Sample the primary window's
presentation stream and keep the metrics in the current title session.

On Windows, run a self-authored title through the integrated graphics path and
verify the exact title bar text with and without display-name metadata, including
a Unicode name. Toggle the overlay in the running window and verify that its
metrics and graph update as that title presents frames. When a Scarlett GDK
sample can reach the same path, repeat the smoke check and record the sample
revision and result. [R6]

### T-903 — Connect the input adapter to one host controller

Route one documented reading path to a host controller. Validate neutral state, button transition and device removal using a scripted sequence, then record a manual PC GDK Gamepad comparison where possible. Add a console-facing adapter only for supported signatures. [R6, R11]

### T-904 — Play one PCM buffer through a Windows audio backend

Reuse a documented host audio API for one PCM format. Check sample count, completion and stopping through a deterministic sink and verify a separate host smoke test. Keep multichannel and compressed formats as later evidence-driven slices. [R11]

### T-1001 — Capture a repeatable early title compatibility result

When the tester has a readable sample or title, collect its hash, backend/device fingerprint and first verified stopping point. Repeat on the same build, capture stage and reason consistently, and file one reproducible follow-up with a minimal synthetic regression where possible. Record unavailable material as SKIPPED. [R5, R6]
