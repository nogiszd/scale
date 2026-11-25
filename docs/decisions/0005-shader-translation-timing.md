# 0005 — Shader translation timing, caching and runtime diagnostics

Status: accepted (2026-11-24, maintainer decision). Implementation tracked in M8.

## Context

Console titles ship their shaders **precompiled** as RDNA2 GPU binaries.
There is no shader source and no runtime compilation on the console. SCALE
must therefore translate each RDNA2 binary into SPIR-V (decision 0003)
before the host GPU can run it. The question is when that happens and what
diagnostics are available while a title runs.

## Decision

1. **Translate on first use.** A shader binary is translated when the title
   first creates a pipeline that uses it. That is the point where SCALE first
   knows the binary and the state it is used with. No offline precompile
   step is needed to run a title.
2. **Translate once, then reuse.** A shader binary is fixed, so each distinct
   binary is normally translated exactly once. The result goes into a disk
   cache keyed by: shader binary hash, shader ABI version, translator
   version and relevant Vulkan backend configuration. Later
   launches load from the cache. If the same binary is used under a
   different ABI or state that changes the translation, that is a different
   key. The cache is added only after translation is verified correct
   (AGENTS.md).
3. **Build pipelines ahead of use.** Schedule translation and host pipeline
   construction when the full description is available. Defer completion when
   the guest API allows it. Share in-flight builds and prioritize pipelines the
   title needs. Wait for a valid pipeline at the point required by the guest
   API and report build failures. See
   [T-810](../research/TRANSLATION_GAPS.md#t-810--warm-up-previously-used-vulkan-pipelines).
4. **Warm up previously used pipelines.** Reuse local pipeline descriptions
   and compatible Vulkan driver cache data on subsequent runs. Keep both
   separate from the translated SPIR-V cache. Check driver cache header
   compatibility against the active Vulkan device, including vendor ID, device
   ID and pipeline cache UUID. A later optional offline tool may translate
   complete shader archives for supported formats. On-demand builds cover
   pipelines absent from the saved records.
5. **Runtime diagnostics, always on:** every translation failure becomes a
   structured event in `report.json` (AGENTS.md diagnostic contract), with
   shader hash, stage, ISA target, instruction offset and opcode, unsupported
   modifier or ABI feature, compiler stage and validation error. An
   unsupported shader is an error, never a no-op.
6. **Debug mode, opt-in** (slow, for investigation):
   - run `spirv-val` on every translated module;
   - enable the Vulkan validation layers;
   - run the reference interpreter (`scale::shader_ref`) alongside a
     dispatch and report the first lane and instruction where the GPU result
     differs. This applies within the interpreter's modeled subset; outside
     it, the comparison is reported as SKIPPED.

## Consequences

- The translator stays a library with no global state, callable from a
  background thread. The cache key inputs must stay explicit.
- The `scale-shader` CLI (T-706) can host later offline shader translation
  and inspection. Pipeline warm-up uses the same creation path as runtime
  requests.
- Debug-mode comparisons inherit the interpreter's coverage, so the
  interpreter grows together with the translator.
