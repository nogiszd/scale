# Multi-agent coordination

[AGENTS.md](../AGENTS.md) sets the project-wide rules. [CONTRIBUTING.md](../CONTRIBUTING.md) describes worktrees, task handoff, testing and review. This page records only agent-specific coordination.

## Ownership and isolation

- Contributors can take a pick-up task solo or organize their own local team. Choose a local coordinator if it helps your team.
- For upstream work, follow the [pick-up workflow](../CONTRIBUTING.md#2-join-the-pipeline). The board comment records the claim. Review and integration happen through the upstream PR.
- Contributors work in separate task branches and worktrees with separate build trees. Respect assigned paths and frozen interfaces. Raise overlap or required shared-file changes in the task thread or existing handoff before editing those paths.
- `main` is verified merged work; `integration` receives reviewed task branches; `<agent>/<task-id>-<slug>` is an isolated task branch. The maintainer controls pushes to the remote. Never force-update a branch another agent might use.
- Coordinate GPU tests if simultaneous execution could invalidate correctness or performance measurements.

## Example maintainer setup

This is the maintainer's local setup. Contributors can work solo or choose their own coordinator, tools and division of work.

| Agent | Area |
|---|---|
| Claude (coordinator) | Shared architecture and build, native loader/runtime, reference shader interpreter, Vulkan backend, integration and reviews |
| Codex | Shader decoding, IR, SPIR-V translation, shader ABI and compiler verification |

Use [TASKS.md](TASKS.md) for current upstream assignments and owned paths. A consumer can begin against a frozen public interface merged to `integration`, before its producer's implementation is finished. Interface changes require a coordinator-approved note in the task registry.

## Durable handoff

Keep task status and dependencies in [TASKS.md](TASKS.md), verified results in [PROGRESS.md](PROGRESS.md), and concise personal context in `docs/agents/<agent>/HANDOFF.md`. A handoff should contain the working branch, next action, outstanding reviews, blockers and exact test evidence that has not already been recorded in PROGRESS. Refer to canonical records instead of copying long task descriptions or milestone text.

The repository, rather than an agent's conversation history, is the shared source of truth.
