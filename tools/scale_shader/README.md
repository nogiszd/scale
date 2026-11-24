# scale-shader

Build with `-DSCALE_ENABLE_SHADER=ON`. The executable is in the build
directory's `bin` folder.

```text
scale-shader decode tests/shader/cases/t704_add_lane_to_input.json
scale-shader decode 0xbf810000
scale-shader compile tests/shader/cases/t707_exec_ff_lanes_4_to_7.json -o masked.spv
scale-shader validate masked.spv
```

`decode` prints dword offsets, raw words and instruction operands. Inline words
use eight hex digits each with an optional `0x` prefix. Separate words with
spaces. Branch targets are dword offsets from the start of the stream.

`compile` reads the shader test-case format and selects the lowering from
`abi_version`. JSON cases are limited to 16 MiB. ABI v1 compilation embeds
the case's `initial_exec`, including zero. The module uses the two storage
buffer bindings from the shader ABI.
The caller supplies the buffer contents and checks the case's execution
domain before dispatch. [Shader ABI contract](../../docs/contracts/shader_abi.md)

`validate` runs the installed `spirv-val` for Vulkan 1.2. It looks on `PATH`,
then in the Vulkan SDK's binary directory under `VULKAN_SDK`. It prints
`SKIPPED` when the tool is unavailable and returns 1 because validation did
not run.

Exit codes are 0 for success, 1 for operation failure and 2 for invalid
command usage.
