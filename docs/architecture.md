# Architecture

## Components and targets

| Directory | CMake target | Depends on | Owner (current) | State |
|---|---|---|---|---|
| `src/common` | `scale::common` | Windows SDK | Claude | present |
| `src/diagnostics` | `scale::diagnostics` | common | Claude | present |
| `src/loader` | `scale::loader` | common | Claude | present (inspector, image mapping) |
| `src/host` | `scale::host` | loader, diagnostics | Claude | present (supervisor, run reports) |
| `src/runtime` | `scale::runtime` | loader, diagnostics | Claude | present (import resolution policy) |
| `src/filesystem` | `scale::filesystem` | common | unassigned | planned (M4) |
| `src/shader` | `scale::shader_decode`, `scale::shader_ir`, `scale::shader_spirv` | common, diagnostics | Codex | planned, `SCALE_ENABLE_SHADER` |
| `src/shader_ref` | `scale::shader_ref` | shader_decode (public header only), common | Claude | planned, `SCALE_ENABLE_SHADER` |
| `src/graphics` | `scale::graphics` | common, diagnostics, Vulkan | Claude | planned, `SCALE_ENABLE_GRAPHICS` |
| `tools/scale` | `scale` (exe) | everything above, host | Claude (registry); command owners own their file | present |
| `tests/support` | `expect_command.cmake` (CLI checks); GoogleTest 1.18.0 via FetchContent | — | Claude | present |

Directories are created only when their first task starts.
`scale_add_component` in the root `CMakeLists.txt` already lists every planned
directory, so creating one needs no root change. CMake reports missing
components as `[not yet implemented]`.

## Dependency rules

```text
tools/scale -> host -> loader, runtime
runtime -> filesystem (later graphics, audio, input)
graphics -> shader_*            (M8 only)
shader_ref -> shader_decode     (decoded instruction form only)
all -> diagnostics -> common
```

- `common` and `diagnostics` never depend on higher levels.
- `shader_*` never depends on loader, host, runtime or graphics. Vulkan
  execution lives in the graphics backend or test harnesses.
- `shader_ref` (the reference interpreter) shares the **decoder only** with the
  compiler. It must not use `shader_ir`, `shader_spirv` or any compiler
  lowering code. This keeps semantic errors uncorrelated.
- No cycles. A new cross-component dependency is a coordinator decision.

## Conventions

- C++23, MSVC, `/W4 /WX /permissive- /utf-8 /sdl /guard:cf`. The link flags
  keep ASLR, high-entropy ASLR, DEP, CFG and CET. Every first-party target
  links `scale::build_options`. Use `scale_add_library(<name> SOURCES ...
  DEPS ...)` to get `scale::<name>` with public headers in
  `include/scale/<name>/`.
- Namespaces: `scale` (common), `scale::<component>` elsewhere.
- Errors: `scale::Result<T>`. No exceptions across component boundaries, except
  `std::bad_alloc` and logic errors from programming mistakes.
- Untrusted bytes go only through `scale::ByteView` and the `checked_*`
  helpers.
- Strings are UTF-8 internally. Convert to UTF-16 only at Win32 calls
  (`scale/common/text.h`).
- Diagnostics follow `docs/contracts/diagnostics.md`. Tests follow
  `docs/contracts/testing.md`.

## Execution model (planned, M2)

The CLI supervisor (`scale run`) launches an unprivileged worker process. The
worker owns guest image state and execution. The supervisor enforces timeouts
with a job object (kill-on-close) and collects the exit code, exception and
diagnostics. This isolates crashes. It is **not** a security boundary. A
decision record will cover how images are mapped (OS loader vs. custom
mapping) before T-202 is implemented.
