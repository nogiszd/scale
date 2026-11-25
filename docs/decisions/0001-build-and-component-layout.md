# 0001 — Build and component layout

Status: accepted (2026-11-24, T-000)

## Decision

- One CMake project. Each component is a static library `scale::<name>` that
  links `scale::build_options` privately. Hardening flags (ASLR, HEVA, DEP,
  CFG, CET, `/sdl`) are on for all first-party code.
- The root `CMakeLists.txt` lists every planned component through
  `scale_add_component`. A missing directory is reported, not an error, so
  agents can create their component without editing the root file.
- Shader (`SCALE_ENABLE_SHADER`) and graphics (`SCALE_ENABLE_GRAPHICS`) are
  opt-in until their toolchains are installed. They are still mandatory
  deliverables.
- The reference interpreter lives in `src/shader_ref`, separate from the
  compiler in `src/shader`, and may depend only on the decoder.
- ~~A minimal in-tree test harness; no GoogleTest.~~ Superseded 2026-11-24:
  AGENTS.md requires GoogleTest (pinned) via CTest. Done in T-001
  (GoogleTest 1.18.0).

## Rationale

These choices give each agent its own directories, keep conflicts confined
to a few documented shared files. The interpreter and compiler remain independent
as required by the [roadmap](../ROADMAP.md#m7--rdna2-shader-recompiler).
