# 0005 — Shader translation timing, caching and runtime diagnostics

Status: accepted (2026-11-24, maintainer decision). Implemented in M8.

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
   version and relevant Vulkan backend configuration (AGENTS.md M8). Later
   launches load from the cache. If the same binary is used under a
   different ABI or state that changes the translation, that is a different
   key. The cache is added only after translation is verified correct
   (AGENTS.md).
3. **Don't stall the frame.** Translation runs off the calling thread. How a
   pipeline behaves while its shader is still translating is its own
   decision, made in M8. It must never silently skip work or produce
   unverified output.
4. **Optional warm-up.** A later `scale-shader` command may pre-translate
   shaders recorded on an earlier run, to avoid first-run stutter. This is an
   optimization, never a requirement.
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
- The `scale-shader` CLI (T-706) is the natural home for the optional
  warm-up and for offline inspection of cached translations.
- Debug-mode comparisons inherit the interpreter's coverage, so the
  interpreter grows together with the translator.
