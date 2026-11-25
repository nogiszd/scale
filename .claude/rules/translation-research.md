# Research what SCALE needs for full translation

When planning tasks or doing research as SCALE's coordinator, identify the gaps
between the current implementation and end-to-end title execution. Check the
code, tests and task registry before calling a capability missing. Trace the
required path through native loading, runtime and services, graphics and shader
translation, audio, input and local data.

Use public specifications and public implementations such as XCL as starting
points. For each relevant approach, record the source and revision, the behavior
it demonstrates, and where its assumptions differ from SCALE's. Verify claims
with public documentation or a small reproducible test. Follow the evidence and
code reuse rules in [AGENTS.md](../../AGENTS.md).

Turn actionable gaps into small tasks in the [task registry](../../docs/TASKS.md).
Include a milestone, dependencies, evidence, scope and a testable acceptance
criterion. Prioritize gaps on the path to the next integration milestone.
Record unresolved requirements in [KNOWN_UNKNOWNS.md](../../docs/research/KNOWN_UNKNOWNS.md)
and new sources in [REFERENCES.md](../../docs/research/REFERENCES.md).
