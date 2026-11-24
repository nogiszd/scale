# Synthetic shader ABI v0

This contract is for Scale's first self-authored `gfx1030` compute fixtures.
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
with a synchronous SPIR-V load and its normal value dependency. Each fixture
issues exactly one store immediately before `s_endpgm`, writing one dword
per lane. The observable result is all 32 output dwords in lane order after
dispatch completion.

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

Report invalid fixture data and unsupported instructions or ABI usage as
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
