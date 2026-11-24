# 0003 — GPU backend: Vulkan with SPIR-V shader translation

Status: accepted (2026-11-24, maintainer decision)

## Context

AGENTS.md plans Vulkan as the first host graphics backend, with D3D12 as a
later option "if verified console API evidence favors D3D12". The
alternative considered was native D3D12, with RDNA2 shaders translated to
HLSL and compiled to DXIL by DXC. The argument for it is that the console
graphics API (D3D12.X) is a close relative of D3D12: descriptor heaps, root
signatures and command list model.

## Decision

Keep **Vulkan + SPIR-V** for the first graphics slice (T-601) and the
shader translator's first emitter (T-703).

## Rationale

- **The shader pipeline is backend-neutral up to the emitter.** The decoder
  (T-701), typed IR (T-702) and the independent reference interpreter (T-711)
  don't depend on the target. A DXIL route can be added later as a second
  emitter from the same IR without redoing verified work.
- **Verification is available now.** `spirv-val` (SPIRV-Tools v2026.4) and
  the Vulkan 1.4 driver on the development GPU are installed. That allows the
  M7 acceptance chain to run without new dependencies: decode → IR → SPIR-V
  → validation → GPU readback → comparison with the interpreter.
- **Evidence standard.** The similarity between D3D12.X and D3D12 is
  plausible but unverified (KNOWN_UNKNOWNS U-004). AGENTS.md requires
  verified evidence before switching. It also says not to build two
  backends at once.
- **Independent design.** SCALE keeps its own architecture and test
  methodology (differential CPU-interpreter vs. GPU verification) and does
  not mirror another project's backend choice.

## Consequences

- Mapping console graphics objects onto Vulkan is expected to be harder than
  onto D3D12. Do not assume a one-to-one mapping (AGENTS.md M6). Record
  mapping questions as evidence-backed tasks.
- Revisit at M8 (graphics integration). If verified D3D12.X evidence shows
  Vulkan mapping costs that a D3D12 backend would avoid, add a D3D12 backend
  with an HLSL/DXIL emitter from the shared IR in a new decision record. It
  would replace Vulkan for that path, not run in parallel with it.
