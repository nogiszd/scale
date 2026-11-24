# GPU test targets

GPU behavior that SCALE's shader and graphics translation relies on varies
between vendors, architectures, drivers and software implementations. A
GPU result is evidence only for the device that produced it. This contract
defines which targets matter, what every GPU test must record, and when SCALE
may claim that a target class is covered.

## Properties that vary (and how the tests treat them)

| Property | Variation | Rule in SCALE tests |
|---|---|---|
| Subgroup size | Implementation-defined. AMD exposes 32 and 64 (the RX 9070 XT defaults to 64, range 32–64). Other vendors expose other sizes. Control is core in Vulkan 1.3 (`subgroupSizeControl`). | Translated shaders never depend on subgroup size or lane order. ABI v0/v1 model a logical wave32 over a 32-invocation workgroup (docs/contracts/shader_abi.md). Record `subgroupSize`, `minSubgroupSize` and `maxSubgroupSize`. |
| Float controls | `shaderRoundingModeRTEFloat32`, denorm preserve/flush and signed-zero/inf/nan preserve are optional per-device properties (core since Vulkan 1.2). | A case that needs RTE is **SKIPPED** with the reason on devices without it, never run approximately. Record all float-control properties. |
| Workgroup memory, limits | `maxComputeSharedMemorySize`, `maxComputeWorkGroupInvocations` and others vary above the spec minimums. | Tests stay within the Vulkan spec minimums unless they declare a requirement, which is checked before dispatch. |
| Driver compiler | Same hardware, different driver (vendor vs. Mesa) or driver version can expose different bugs. | Record the driver ID, name, info and version for every result. |
| Software Vulkan | lavapipe (Mesa, LLVM-based, Vulkan 1.3 conformant) and similar run on the CPU. | Useful for CI and correctness smoke tests. Results are labeled `software_gpu` and are never counted as hardware evidence (AGENTS.md M7). |

## Target classes

| Class | Why it matters | Example devices | Status |
|---|---|---|---|
| **A1** AMD RDNA2 | The Xbox Series X\|S GPU architecture; the closest host match for translated RDNA2 shaders | RX 6000 series | not available |
| **A2** AMD RDNA3/RDNA4 | Current AMD; development machine | RX 7000, **RX 9070 XT** (dev) | **covered**: RX 9070 XT, AMD proprietary 26.10.1 (LLPC), Vulkan 1.4.359 (T-601, 2026-11-24) |
| **N** NVIDIA | Different subgroup and compiler behavior; the largest PC install base | RTX 20–50 series | not available |
| **I** Intel | Variable subgroup sizes, different compiler | Arc / Xe | not available |
| **S** Software | CI without a GPU; independent implementation | lavapipe (Linux, T-802), others where available | not available |

A test run covers the classes of the devices it actually ran on. The
**coverage claim** for a feature lists each class that has a recorded PASS
in docs/PROGRESS.md, with device and driver. Classes without runs are
reported as NOT_TESTED, never assumed. M7 acceptance requires at least one
hardware class. Broad claims ("works on NVIDIA") need that class's runs.

## What every GPU test does

1. **Enumerate all Vulkan devices** and run on each one that meets the
   test's declared requirements, not just the first. Each device is
   reported separately. One device failing doesn't hide another's
   result.
2. **Record a capability fingerprint** with each result: device name, vendor
   ID and device ID, device type (discrete/integrated/CPU), API version,
   driver ID, name, info and version, subgroup properties, float controls,
   and shared-memory limit. SCALE test output prints it. PROGRESS records it.
3. **Declare requirements** (e.g. `rte_f32`). If a device lacks one, the
   case is SKIPPED with the missing capability named. If no device can run
   it, the test as a whole is SKIPPED.
4. **Compare exactly**: u32 bit for bit, f32 by the ABI rule (0 ULP inside
   the finite-normal domain). There are no tolerances except those a
   contract defines.
5. **Label** `gpu`. CTest labels are static, so results from CPU devices are marked `[software_gpu]` in the test output and fingerprint. They are recorded separately in PROGRESS and never count as hardware evidence.

## How coverage grows

- More machines: run `ctest --preset gpu` on each available device class
  and record the fingerprints and results in docs/PROGRESS.md.
- CI: T-802 (Linux) adds lavapipe as class S, and later hardware runners if
  available.
- Contributors with other GPUs are asked (CONTRIBUTING.md) to run the GPU
  preset and submit their fingerprint and results.

## Running the GPU tests

```powershell
. .\scripts\vsenv.ps1
cmake --preset debug-all          # shader + graphics components
cmake --build --preset debug-all
ctest --preset gpu                # every GPU test on every device present
```

For debugging, add `$env:VK_INSTANCE_LAYERS = "VK_LAYER_KHRONOS_validation"`.