# Synthetic shader ABI v0

This contract is for SCALE's first self-authored `gfx1030` compute test programs.
Every requirement below is a **synthetic ABI decision**. Use the AMD and LLVM
references for instruction semantics and encodings. Xbox shader metadata,
container parsing and AMDHSA code objects are outside this contract.

## Dispatch and entry state

**Synthetic ABI decision.** Dispatch one workgroup of `32 × 1 × 1` work-items
as one logical wave32. Lane `i` is local invocation X, from 0 through 31.
The program starts at dword 0 and runs straight-line code through a single
terminal `s_endpgm` with immediate zero. The code stream ends there.

| Register | Entry value |
|---|---|
| `v0` | Lane index `i` |
| `v1` | Byte offset `4 * i`, reserved for memory addressing |
| `s[0:3]` | Opaque resource descriptor for a 128-byte read-only `input` buffer |
| `s[4:7]` | Opaque resource descriptor for a 128-byte write-only `output` buffer |
| `EXEC` | `0x00000000ffffffff`, all 32 lanes active |
| `VCC`, `SCC` | Zero |

Descriptor operands are identified by the SGPR tuple in the instruction.
Their contents have no scalar dword representation in v0. The tuples may
only appear as the resource operands listed below. `v1` is immutable. All
unlisted registers have undefined initial contents. An instruction may
read a temporary register only after a defined write. Scalar state is
wave-uniform and VGPR state is per lane. EXEC stays fixed throughout the
dispatch. Branches, EXEC writes and cross-lane operations are outside v0.
Interpreter unit tests may set up other masks independently of this dispatch.

## Buffer access and readback

**Synthetic ABI decision.** Use untyped MUBUF `buffer_load_dword` and
`buffer_store_dword` with `offen` set, `idxen` clear, `v1` as the address
VGPR, zero immediate byte offset and the inline scalar offset `0`. Optional
cache, fault and LDS flags must be clear. Loads use `s[0:3]` and stores use
`s[4:7]`. Both buffers are distinct, have 32 little-endian dwords and use
the same byte offset `4 * i`. Each access is therefore aligned and within
its buffer. Other resources, address forms, memory opcodes and out-of-range
access behavior are outside v0.

Every load must be covered by a later `s_waitcnt vmcnt(0)` before the final
store and before any use or overwrite of the load destination. This is the
only v0 wait form. The compiler may represent a validated load and wait
with a synchronous SPIR-V load and its normal value dependency. Each test
program issues exactly one store immediately before `s_endpgm`, writing one
dword per lane. The observable result is all 32 output dwords in lane order
after dispatch completion.

**Synthetic ABI decision.** Emit a `GLCompute` entry point with
`LocalSize 32 1 1`. Map `v0` to `LocalInvocationIndex` and `v1` to four times
that index. Use two Vulkan `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER` bindings,
descriptor set 0 binding 0 for `input` and binding 1 for `output`, each with
a 128-byte descriptor range. In SPIR-V each is a `StorageBuffer` `Block`
with one array of 32 unsigned 32-bit integers, member `Offset 0` and
`ArrayStride 4`. Mark the input member `NonWritable` and the output member
`NonReadable`. Index with `byte_offset / 4` and bitcast when the operation
requires `f32`. The Vulkan harness uploads the input, dispatches `(1, 1, 1)`,
and makes shader writes visible to host readback before comparing the
results. There are no device addresses or Vulkan subgroup operations in this
mapping. The logical wave32 does not constrain the physical subgroup size.

## Differential test cases

**Synthetic ABI decision.** A case is a UTF-8 JSON object with the following
required fields. Reject duplicate, missing or unknown fields, malformed
values, unsupported versions and dispatch parameters outside this contract.

| Field | Value |
|---|---|
| `schema_version`, `abi_version` | Integers `1` and `0` respectively |
| `name` | Nonempty case name for reports |
| `isa_target`, `stage` | Strings `gfx1030` and `compute` |
| `wave_size` | Integer `32` |
| `workgroup_size`, `workgroup_count` | Integer arrays `[32, 1, 1]` and `[1, 1, 1]` |
| `code_words` | Nonempty array of instruction dwords in stream order |
| `input_words` | Exactly 32 dwords in lane order |
| `expected_output_words` | Exactly 32 dwords in lane order |
| `output_type` | `u32` for integer or raw-bit cases without floating arithmetic, `f32` for floating results |
| `seed` | `null` for manual inputs, or an unsigned 32-bit JSON integer identifying the seed used to generate the supplied inputs |

A dword is a string `0x` followed by eight lowercase hexadecimal digits.
Serialize each instruction dword in little-endian byte order before decode.
Load `input_words` as supplied in both executions. The seed is recorded for
reproduction and does not alter the supplied inputs. Each operation needs
manual golden cases. Generated cases also supply explicit expected words.
Neither implementation provides the expected words for its own comparison.

**Synthetic ABI decision.** Compare both interpreter and GPU outputs against
the expected values and against each other. Compare `u32` outputs bit for
bit, including integer wraparound. The `f32` arithmetic domain in v0 is
finite normal binary32 operands and outputs. Exact results before rounding
must have magnitudes within the finite normal range. Round each unmodified
`v_add_f32` or `v_mul_f32` separately to nearest, ties to even. Compare
`f32` results bit for bit (0 ULP). The runner checks the domain on the
interpreter path before dispatch. The emitter requests `RoundingModeRTE 32`
for floating arithmetic and prevents contraction and reassociation. A
device without `shaderRoundingModeRTEFloat32` cannot run an `f32` arithmetic
case. Report the missing capability and mark the GPU case SKIPPED. Integer
cases have no float-controls dependency.

Report invalid test case data and unsupported instructions or ABI usage as
errors before dispatch. Include the case name and, for instruction-specific
errors, its offset and opcode when available. Follow the
[diagnostics contract](diagnostics.md) for event fields and the
[testing contract](testing.md) for skips. A GPU result records the device,
driver, tool versions and seed and identifies software Vulkan if used.

### Example

This kernel adds the lane index to the corresponding input dword modulo
`2^32`. Lane 1 exercises wraparound (`0xffffffff + 1 = 0`).

```asm
buffer_load_dword v2, v1, s[0:3], 0 offen
s_waitcnt vmcnt(0)
v_add_nc_u32_e32 v2, v0, v2
buffer_store_dword v2, v1, s[4:7], 0 offen
s_endpgm
```

```json
{
  "schema_version": 1,
  "abi_version": 0,
  "name": "add_lane_to_input",
  "isa_target": "gfx1030",
  "stage": "compute",
  "wave_size": 32,
  "workgroup_size": [32, 1, 1],
  "workgroup_count": [1, 1, 1],
  "code_words": [
    "0xe0301000", "0x80000201", "0xbf8c3f70", "0x4a040500",
    "0xe0701000", "0x80010201", "0xbf810000"
  ],
  "input_words": [
    "0x00000000", "0xffffffff", "0x00000002", "0x00000003",
    "0x00000004", "0x00000005", "0x00000006", "0x00000007",
    "0x00000008", "0x00000009", "0x0000000a", "0x0000000b",
    "0x0000000c", "0x0000000d", "0x0000000e", "0x0000000f",
    "0x00000010", "0x00000011", "0x00000012", "0x00000013",
    "0x00000014", "0x00000015", "0x00000016", "0x00000017",
    "0x00000018", "0x00000019", "0x0000001a", "0x0000001b",
    "0x0000001c", "0x0000001d", "0x0000001e", "0x0000001f"
  ],
  "expected_output_words": [
    "0x00000000", "0x00000000", "0x00000004", "0x00000006",
    "0x00000008", "0x0000000a", "0x0000000c", "0x0000000e",
    "0x00000010", "0x00000012", "0x00000014", "0x00000016",
    "0x00000018", "0x0000001a", "0x0000001c", "0x0000001e",
    "0x00000020", "0x00000022", "0x00000024", "0x00000026",
    "0x00000028", "0x0000002a", "0x0000002c", "0x0000002e",
    "0x00000030", "0x00000032", "0x00000034", "0x00000036",
    "0x00000038", "0x0000003a", "0x0000003c", "0x0000003e"
  ],
  "output_type": "u32",
  "seed": null
}
```

## ABI v1 — EXEC masks and scalar control flow

**Synthetic ABI decision.** ABI v1 inherits v0 except where this section
specifies a change. It keeps the `gfx1030` target, the `32 × 1 × 1`
workgroup, the single-workgroup dispatch, and both 128-byte buffer bindings
and their address mapping. The register and instruction rules below apply
to SCALE test programs.

### Wave state and mask updates

**Synthetic ABI decision.** `EXEC` and `VCC` are wave-uniform 32-bit masks.
Bit `i` names lane `i`, whose ID is `LocalInvocationIndex`. `initial_exec`
sets `exec_lo` at entry. Every bit pattern is valid, including zero and
all ones. `v0` and `v1` are initialized for all 32 lanes. `VCC` and `SCC`
start at zero. A later `exec_lo` write can activate any lane. Data written
before a lane became inactive survives reactivation. ABI v1 supports the
low halves of `EXEC` and `VCC`. Wave64, high-half operands, and additional
special-register access are outside this version.

**Synthetic ABI decision.** Scalar instructions and branches execute once
per wave even with `EXEC == 0`. A vector operation uses the mask at the
start of that instruction. Inactive lanes read no sources, perform no
arithmetic or memory access, and preserve their VGPR values and definedness.
A vector instruction with an empty mask reads no scalar or vector sources.
A read of an undefined SGPR by an executing scalar instruction, or of an
undefined source by an active vector lane, is an error. Reactivation
preserves any undefined state the lane had before.

**Synthetic ABI decision.** The additional mask instructions have the
following effects. Each reads its sources before writing any destination.

| Instruction | Supported operands and effect |
|---|---|
| `v_cmp_eq_u32_e32`, `v_cmp_lt_u32_e32` | Unmodified VOPC with `vcc_lo` as destination. Source 0 is a general-purpose register or an inline or literal constant. Source 1 is a VGPR. Compare as unsigned dwords in source order. Replace all of `VCC` with the mask of lanes enabled by `EXEC` whose comparison is true. Inactive bits clear, including when `EXEC == 0`. Preserve `EXEC` and `SCC`. |
| `s_and_saveexec_b32 dst, src` | `dst` is an ordinary writable SGPR outside the descriptor tuples. `src` is an ordinary SGPR, `vcc_lo`, or an inline or literal constant. Save old `EXEC` to `dst`, set `EXEC` to `old_exec & src`, and set `SCC` to `1` exactly when the new mask is nonzero. Preserve `VCC`. The saved mask is an ordinary dword and follows the scalar register lifetime rules. |
| `s_mov_b32 exec_lo, src` | `src` is an ordinary SGPR, `vcc_lo`, or an inline or literal constant. Replace `EXEC` with all 32 bits of `src`. Preserve `VCC` and `SCC`. This form can restore a saved mask even when the current mask is zero. |

The wave32 mask definitions, full VCC write on compare, and save/restore
instruction semantics are grounded in AMD document 70648 §§3.3, 3.8–3.9,
12.3, and 12.9. LLVM documents the GFX1030 operand forms and the low-half
mask names. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture) [LLVM GFX1030 instruction syntax](https://releases.llvm.org/18.1.6/docs/AMDGPU/AMDGPUAsmGFX1030.html) [LLVM AMDGPU operand syntax](https://releases.llvm.org/18.1.6/docs/AMDGPUOperandSyntax.html)

### Scalar control flow

**Synthetic ABI decision.** There is one wave-uniform program counter,
initially dword 0. A taken branch selects its target for the entire wave.
A conditional branch that is untaken falls through to the next decoded
instruction. Branches preserve registers, masks and condition flags.

| Instruction | Take the branch when |
|---|---|
| `s_cbranch_scc0` | `SCC == 0` |
| `s_cbranch_scc1` | `SCC == 1` |
| `s_cbranch_execz` | `EXEC == 0` |
| `s_branch` | Always |

**Synthetic ABI decision.** Compute the target dword as the offset of the
next instruction in dword units plus the sign-extended `SIMM16` field. Use
checked arithmetic so the offset cannot wrap.
Decode the complete stream before checking targets. A target must be the
first dword of a later decoded instruction in the same stream. Reject a
target in an instruction continuation or literal, outside the stream, at
the branch itself, or earlier than the branch. ABI v1 permits forward
control flow. Every path from entry passes through the sole output store
and then the sole terminal `s_endpgm`. A branch may target the store and
must never bypass it. Check the layout and operands
of all instructions, including paths a particular test case does not take.

The conditions and next-instruction-relative displacement are specified
in AMD document 70648 §§3.2, 12.5, and 13.1.5. LLVM identifies the SOPP
branch operands as labels. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture) [LLVM GFX1030 SOPP syntax](https://releases.llvm.org/18.1.6/docs/AMDGPU/AMDGPUAsmGFX1030.html#sopp)

### Loads, waits and inactive output lanes

**Synthetic ABI decision.** A load marks only its active destination lanes
pending. A pending lane of a VGPR cannot be read or overwritten until the
wave executes `s_waitcnt vmcnt(0)`. A masked-off lane creates no pending load
and an existing pending load survives any later mask change. The wait
clears all earlier pending loads, including for lanes currently inactive.
All loads issued on the executed path must be waited on before the final
store. The other wait counters retain their v0 settings.

**Synthetic ABI decision.** The harness initializes the output buffer with
`initial_output_words` before dispatch. The final `buffer_store_dword`
overwrites word `i` only when bit `i` of `EXEC` at that instruction is set.
An inactive lane leaves the entire initial dword unchanged. It need not
have a defined store source. An empty store mask preserves all 32 initial
words. Compare the entire readback with `expected_output_words`.

The load counter and MUBUF operands are described in AMD document 70648
§§8.1 and 12.5. The output initialization and pending-lane rules above are
part of the synthetic contract. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)

### Divergence in SPIR-V

**Synthetic ABI decision.** Launch all 32 Vulkan invocations and carry the
scalar PC, SGPR values, `EXEC`, `VCC` and `SCC` as values uniform across the
workgroup. Use structured control flow for scalar branches. One lowering
is a uniform block dispatcher using a structured loop containing an
`OpSwitch` selection. Guard lane work with a structured selection on bit
`LocalInvocationIndex` of `EXEC`. Preserve each VGPR across inactive arms
and branch joins. A masked store emits `OpStore` only on the active arm.
Logical deactivation never exits an invocation early. All invocations
complete at the same terminal `s_endpgm`.

**Synthetic ABI decision.** Form the compare mask from the logical lane
results across the workgroup. One lowering writes a per-invocation word in
`Workgroup` memory containing its lane bit on an active true comparison and
zero otherwise. After `OpControlBarrier` with `Workgroup` execution and
memory scopes and `AcquireRelease | WorkgroupMemory` semantics, every
invocation ORs the 32 words to obtain `VCC`.
A second barrier separates the reads from reuse of the scratch words. Both
barriers execute in workgroup-uniform control flow after the local mask
selection has merged. Even lanes with `EXEC` clear participate in this
exchange. The lowering depends on the workgroup size and the logical lane
IDs. It imposes no subgroup size or subgroup lane-order requirement.

**Synthetic ABI decision.** The compiler checks the decoded structure and
preserves per-lane and uniform state through control flow. A differential
test executes the independent interpreter first with the same initial mask
and buffers. It rejects invalid executed reads, missing waits, and
out-of-domain active floating-point operations before GPU dispatch. The
translated test program uses the case's `initial_exec` without requiring
an additional binding. A validated guest wait can lower to the synchronous
SPIR-V loads and their value dependencies, as in v0. The workgroup compare
exchange has its own synchronization.

SPIR-V specifies the structured merge instructions and control barrier.
Vulkan defines workgroup shared memory and the invocation index used for
the logical lane mapping. [Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html) [Vulkan compute shader execution](https://docs.vulkan.org/spec/latest/chapters/shaders.html#shaders-compute) [Vulkan LocalInvocationIndex](https://docs.vulkan.org/refpages/latest/refpages/source/LocalInvocationIndex.html)

### ABI v1 test cases

**Synthetic ABI decision.** Use the same UTF-8 JSON object format and
required v0 fields with the following version and additional fields. All
other fields retain their v0 types and validation. Unknown, missing and
duplicate keys remain errors. ABI v0 cases continue to use their original
field set.

| Field | Value for an ABI v1 case |
|---|---|
| `schema_version`, `abi_version` | Integers `1` and `1` respectively |
| `initial_exec` | One required dword string in the v0 format. Bit 0 represents lane 0. |
| `initial_output_words` | Exactly 32 required dword strings in lane order. These initialize the output binding before dispatch. |
| `expected_output_words` | Exactly 32 dwords in lane order, including preserved words for lanes inactive at the store. |

**Synthetic ABI decision.** Compare all output dwords bit for bit. For `f32`
cases the finite-normal rule applies to executed floating-point operations
and written float results. Unwritten words can contain arbitrary sentinel
bits and must match the initial output exactly. The seed retains its v0
meaning. `code_words` contains the whole stream including branch
instructions, and the acyclic control-flow rule bounds executed
instructions by the number decoded.

**Synthetic ABI decision.** In these manual `u32` test cases the seed is
`null`, `input_words[i]` is `i`, and all `initial_output_words` are
`0xdeadbeef`. With `initial_exec` set to
`0x000000ff`, the compare sets `VCC` to `0x000000f0`, the save stores
`0x000000ff` in `s8`, and `EXEC` becomes `0x000000f0` with `SCC == 1`.
Only lanes 4–7 write, producing `5, 6, 7, 8` in their respective slots.
With `initial_exec` set to `0x0000000f`, the compare yields zero and the
branch is taken. No lane reads `v2` or writes the output, so all words are
`0xdeadbeef`.

```asm
v_cmp_lt_u32_e32 vcc_lo, 3, v0
s_and_saveexec_b32 s8, vcc_lo
s_cbranch_execz store
buffer_load_dword v2, v1, s[0:3], 0 offen
s_waitcnt vmcnt(0)
v_add_nc_u32_e32 v2, 1, v2
store:
buffer_store_dword v2, v1, s[4:7], 0 offen
s_endpgm
```

## References

- AMD RDNA 2 ISA reference, document 70648. See §8.1 for MUBUF resource
  operands and byte addressing and Chapter 12 for `S_WAITCNT`, `S_ENDPGM`
  and the vector arithmetic instructions. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)
- LLVM AMDGPU backend guide. See GFX10.3 in
  [AMDGPU Processors](https://llvm.org/docs/AMDGPUUsage.html#amdgpu-processors),
  the descriptor model in [Buffer Resource](https://llvm.org/docs/AMDGPUUsage.html#buffer-resource)
  and the AMDHSA register conventions in
  [Initial Kernel Execution State](https://llvm.org/docs/AMDGPUUsage.html#initial-kernel-execution-state).
- Vulkan rules for [LocalInvocationIndex](https://docs.vulkan.org/refpages/latest/refpages/source/LocalInvocationIndex.html),
  [shader interfaces and buffer layout](https://docs.vulkan.org/spec/latest/chapters/interfaces.html)
  and [float-control properties](https://docs.vulkan.org/refpages/latest/refpages/source/VkPhysicalDeviceFloatControlsProperties.html).
