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

## ABI v2 — Memory and resources

**Synthetic ABI decision.** ABI v2 inherits ABI v1 except where this
section extends it. Keep the single logical wave32, the entry values of
`v0` and immutable `v1`, the EXEC rules, and the forward-only scalar
control flow through one terminal store followed by `s_endpgm`.

### Fixed resources

**Synthetic ABI decision.** Use three distinct, non-aliasing 128-byte
buffers, each containing 32 little-endian dwords. An instruction names
its resource by its opaque, four-SGPR-aligned tuple. `s0` through `s11`
are reserved exclusively for those resource operands. Ordinary scalar
values use `s12` through `s105`. Guest instructions cannot read or modify
descriptor fields as scalar data.

| Tuple | Resource | Permitted access | Vulkan set and binding |
|---|---|---|---|
| `s[0:3]` | Input | `buffer_load_dword` | Set 0, binding 0 |
| `s[4:7]` | Output | `buffer_store_dword` | Set 0, binding 1 |
| `s[8:11]` | Constants | `s_buffer_load_dword` | Set 0, binding 2 |

**Synthetic ABI decision.** The input and output model linear raw V#
descriptors with dword-aligned bases, stride 0, `NUM_RECORDS = 128`
bytes, and `OOB_SELECT = 3`. Use the 32-bit unsigned format
(`BUF_FMT_32_UINT`), channel selections R, G, B, A in order, resource
level 1 and buffer type 0. Cache swizzle, address swizzle and
`ADD_TID_ENABLE` are clear. Swizzle-only fields and reserved bits are
zero. The scalar descriptor uses a dword-aligned base, stride 4 and
`NUM_RECORDS = 32` records. Only those three fields participate in
the scalar load. Each base denotes the start of its own backing buffer.
The tuple selects a binding in SCALE. Its numeric base is never supplied
by a test case or exposed as a device address.

AMD document 70648 §8.1.8, Table 37 defines V# and the units of
`NUM_RECORDS`. §§7.1–7.2 describe the fields used by scalar buffer
reads. LLVM AMDGPUUsage describes V# as a 128-bit buffer descriptor.
[AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)
[LLVM AMDGPUUsage, Buffer Resource](https://llvm.org/docs/AMDGPUUsage.html#buffer-resource)
[LLVM 23.1.6 GFX10 format names](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.6/llvm/lib/Target/AMDGPU/Utils/AMDGPUAsmUtils.cpp)

### Vector memory

**Synthetic ABI decision.** Use only the untyped dword MUBUF load and
store. Require `offen = 1`, `idxen = 0`, and clear optional cache,
fault, LDS and reserved bits. The address is any single VGPR, which
the program may compute separately in each lane. The 12-bit immediate
is an unsigned, dword-aligned byte offset. `soffset` is the inline zero
selector or an ordinary SGPR holding an unsigned byte offset. For each
active lane, read its address VGPR and the scalar offset. A store also
reads its data VGPR. All these sources must be defined and ready even
when the resulting access is out of range. A load reads its sources
before marking its destination pending. A load destination must be a
writable VGPR. Inactive lanes read no operands and leave values,
definedness and pending state unchanged.

**Synthetic ABI decision.** Let `V` be the unsigned VGPR value, `I`
the immediate and `S` the unsigned scalar offset. Compute
`vector_offset = V + I` and `byte_offset = S + vector_offset` with
checked arithmetic before classifying the access. Require `V` and
`S` to be dword-aligned, `S <= 124`, and the sums through
`byte_offset + 4` to fit in unsigned 32 bits. Use these rules for the
memory calculation independently of the wrapping ALU rules that
produced its operands. Misalignment, an offset outside this domain,
or overflow is an error.

| Condition after those checks | Load | Store |
|---|---|---|
| `byte_offset <= 124` | Read input word `byte_offset / 4` | Write output word `byte_offset / 4` |
| `vector_offset >= 128` | Produce raw `0x00000000` | Drop the write |
| `vector_offset <= 124` and `byte_offset >= 128` | Reject before dispatch | Reject before dispatch |

AMD document 70648 §8.1, Figure 8 and Tables 32–35 separate the
scalar addend from the vector-plus-immediate offset. Table 35
describes mode 3 as reducing the byte bound by the scalar addend and
specifies zero reads and dropped writes for out-of-range accesses with
the selected channels. LLVM's raw buffer intrinsic descriptions
exclude the scalar addend from bounds checks. The synthetic rules
above accept aligned dwords where both descriptions agree on the
outcome. Other descriptor modes and access widths are outside this
contract. [AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)
[LLVM AMDGPUUsage, Buffer Fat Pointer out of bounds handling](https://llvm.org/docs/AMDGPUUsage.html#buffer-fat-pointer-out-of-bounds-oob-handling)
[LLVM 23.1.6 AMDGPU intrinsic definitions](https://github.com/llvm/llvm-project/blob/llvmorg-23.1.6/llvm/include/llvm/IR/IntrinsicsAMDGPU.td)

**Synthetic ABI decision.** An active out-of-range load also creates a
pending VGPR value. The ABI v1 per-lane rules apply until
`s_waitcnt vmcnt(0)` completes it. The single final store may address
different words than the corresponding lane indices. The interpreter
must reject duplicate in-range destinations among active lanes of
that store, even when the lanes would write identical data. Dropped
stores and inactive lanes do not enter the collision check. Any
output word that receives no in-range store keeps its initial dword.

### Scalar memory and waits

**Synthetic ABI decision.** Use the immediate form of
`s_buffer_load_dword sdst, s[8:11], imm`. The descriptor form identifies
the constant resource by its tuple. An `s_load_dword` would require a
scalar address pair, which this ABI does not expose. The destination
must be an ordinary writable SGPR. Require `GLC = DLC = 0`, clear
reserved bits, and encode `SOFFSET` as `null` (selector 125). The
unsigned immediate is a byte offset in `0, 4, ..., 124`. LLVM
disassembles the zero-offset form as `s_buffer_load_dword sdst,
s[8:11], null`. Offsets outside the interval, scalar-register offsets
and other SMEM instructions are errors. With no scalar out-of-range
result defined by this ABI, an invalid immediate is rejected even on
an untaken path.

AMD document 70648 §§7.1–7.2, 7.5 and 12.6 distinguish the two
base operands, describe scalar buffer offsets and specify the dword
load. LLVM 23.1.6 assembles both opcodes for `gfx1030`; the buffer
form uses the unsigned `offset20u` immediate syntax.
[AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)
[LLVM GFX1030 SMEM syntax](https://releases.llvm.org/18.1.6/docs/AMDGPU/AMDGPUAsmGFX1030.html#smem)

**Synthetic ABI decision.** The scalar load reads `constant_words[imm / 4]`
once logically per wave, even with an empty EXEC mask. It preserves
EXEC, VCC and SCC and marks the destination pending. A pending SGPR
cannot be read or overwritten by an executing instruction, including
as a vector source or MUBUF `soffset`. The wait publishes the loaded
dword and makes the destination defined. Pending state survives
branches and EXEC changes. Distinct destinations may be pending at
the same time. A wait with nothing pending in its domain is valid.

**Synthetic ABI decision.** Accept exactly these `s_waitcnt` forms.
The counters for the other domains are encoded at their maximums.
Both waits execute even when EXEC is empty. Reject combined and
partial-count waits.

| Instruction | VM | EXP | LGKM | Effect |
|---|---:|---:|---:|---|
| `s_waitcnt vmcnt(0)` | 0 | 7 | 63 | Completes all earlier vector loads, including lanes currently inactive |
| `s_waitcnt lgkmcnt(0)` | 63 | 7 | 0 | Completes all earlier scalar loads |

**Synthetic ABI decision.** Every load issued on the executed path
must be covered by its corresponding wait before the final store,
even if the store mask is empty. AMD document 70648 §§7.3–7.4 and
12.5 describe the SMEM completion counter and the wait encoding.
[AMD RDNA 2 ISA reference](https://docs.amd.com/v/u/en-US/rdna2-shader-instruction-set-architecture)

### Vulkan and SPIR-V

**Synthetic ABI decision.** Emit SPIR-V 1.5 for Vulkan 1.2 using the
ABI v1 `GLCompute` entry point `main`, local size and workgroup-uniform
control flow. All three bindings are `VK_DESCRIPTOR_TYPE_STORAGE_BUFFER`
with 128-byte descriptor ranges. Each `StorageBuffer` `Block` contains
an array of 32 unsigned 32-bit integers at member `Offset 0` with
`ArrayStride 4`. Use `NonWritable` on both read-only members and
`NonReadable` on the output member. The harness uploads input and
constants and initializes output before dispatch. After completion,
it synchronizes shader writes for host readback and compares the
whole output buffer.

**Synthetic ABI decision.** Within the EXEC guard, use structured
control flow to create a buffer element pointer and access it only
on the in-range arm, at index `byte_offset / 4`. The out-of-range
load arm yields a literal dword zero. The out-of-range store arm
performs no write. These guards implement the accepted outcomes
without depending on Vulkan robustness features. The in-range
predicate can use checked unsigned 32-bit additions or equivalent
bounded comparisons. The scalar read uses the fixed constant word
index in uniform control flow outside the EXEC guard. Each physical
invocation may load the same immutable word into its uniform
scalar state. Validated guest waits may lower to synchronous
SPIR-V loads and normal value dependencies. Workgroup barriers
used by the ABI v1 VCC exchange keep their own synchronization.
[Khronos SPIR-V specification](https://registry.khronos.org/SPIR-V/specs/unified1/SPIRV.html)
[Vulkan shader interfaces and buffer layout](https://docs.vulkan.org/spec/latest/chapters/interfaces.html)

**Synthetic ABI decision.** Validate instruction boundaries, layout
and allowed operand forms throughout the stream. Before GPU
dispatch, run the independent interpreter with the same initial
mask and buffer contents used by Vulkan. Pre-check every executed
read and write for definedness, pending state, alignment, supported
bounds behavior and store collisions, and apply the inherited
float-domain rule. Report failures with the case name and the
offending instruction offset and opcode when available. Include
the logical lane and byte offsets for lane-specific address
errors. Dispatch requires a successful pre-check for those inputs.

### ABI v2 test cases

**Synthetic ABI decision.** Use the ABI v1 UTF-8 JSON format with the
required changes below. The inherited fields and their validation,
including rejection of unknown, missing and duplicate keys, still
apply. ABI v0 and v1 retain their existing field sets.

| Field | Value for an ABI v2 case |
|---|---|
| `schema_version`, `abi_version` | Integers `1` and `2` respectively |
| `constant_words` | Exactly 32 required dword strings, in increasing byte-offset order, including for programs that issue no scalar loads |
| `input_words`, `initial_output_words`, `expected_output_words` | Exactly 32 dwords each, in increasing byte-offset order. Expected output includes every preserved word. |

**Synthetic ABI decision.** Keep the bit-exact comparison, including
sentinel words in unwritten slots. The `f32` finite-normal rule covers
executed floating arithmetic and written float results. Constants
are raw dwords subject to the inherited arithmetic rules when used.

### Worked example

**Synthetic ABI decision.** This manual `u32` case uses `seed: null`,
`initial_exec: "0xd000000b"`, and `input_words[i] = i`. Constant word
1 is `0x00000004` and all other constant words are zero. Every initial
output word is `0xdeadbeef`.

```asm
s_buffer_load_dword s12, s[8:11], 0x4
s_waitcnt lgkmcnt(0)
v_add_nc_u32_e32 v2, 4, v1
buffer_load_dword v3, v2, s[0:3], s12 offen offset:4
s_waitcnt vmcnt(0)
v_add_nc_u32_e32 v3, s12, v3
buffer_store_dword v3, v2, s[4:7], 0 offen
s_endpgm
```

LLVM 23.1.6 assembled and disassembled these `gfx1030` wave32 dwords
in stream order.

```json
[
  "0xf4200304", "0xfa000004", "0xbf8cc07f", "0x4a040284",
  "0xe0301004", "0x0c000302", "0xbf8c3f70", "0x4a06060c",
  "0xe0701000", "0x80010302", "0xbf810000"
]
```

The scalar wait makes `s12 = 4` available. In active lane `i`,
`v2 = 4*i + 4`, so the input uses `vector_offset = 4*i + 8` and
`byte_offset = 4*i + 12`. The store uses `v2` alone and targets
output word `i + 1`. Add 4 to the loaded value after the vector wait.

| Active lane | Input byte offset | Loaded value | Output word index | Expected dword |
|---:|---:|---:|---:|---|
| 0 | 12 | 3 | 1 | `0x00000007` |
| 1 | 16 | 4 | 2 | `0x00000008` |
| 3 | 24 | 6 | 4 | `0x0000000a` |
| 28 | 124 | 31 | 29 | `0x00000023` |
| 30 | 132 | 0 | 31 | `0x00000004` |
| 31 | 136 | 0 | 32 (dropped) | — |

All other in-range words remain `0xdeadbeef`. Enabling lane 29
with `initial_exec: "0xf000000b"` must instead fail pre-check at the
load. That lane has `vector_offset = 124` and `byte_offset = 128`.
Removing the scalar wait must also fail pre-check at the first use
of `s12` as the load's scalar offset.

With `initial_exec: "0x00000000"` the scalar load and both waits
still execute and all 32 output words stay `0xdeadbeef`. Removing the
scalar wait in this variant fails at the final store because `s12`
is still pending.

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
