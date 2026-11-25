# Multi-agent coordination

[AGENTS.md](../AGENTS.md) sets the project-wide rules. [CONTRIBUTING.md](../CONTRIBUTING.md) describes worktrees, task handoff, testing and review. This page records only agent-specific coordination.

## Ownership and isolation

- Anyone can coordinate their own work and agents in a local checkout or fork. Choose a person or an agent to plan tasks, assign local work and review changes.
- For the upstream SCALE repository, the maintainer or their delegate confirms claims in [TASKS.md](TASKS.md), approves shared interface changes and handles integration. Check the current `integration` version and use the contributor board to request a claim before starting upstream work.
- Contributors work on separate task branches and worktrees with separate `out/` directories. Do not edit paths assigned to another agent. Shared-file exceptions and current interface freezes are in [TASKS.md](TASKS.md). Ask the coordinator through your handoff for changes to coordinator-owned files.
- `main` is verified merged work; `integration` receives reviewed task branches; `<agent>/<task-id>-<slug>` is an isolated task branch. The maintainer controls pushes to the remote. Never force-update a branch another agent might use.
- Coordinate GPU tests if simultaneous execution could invalidate correctness or performance measurements.

## Example maintainer setup

This is the maintainer's local setup. Contributors can use their own coordinator, tools and division of work.

| Agent | Area |
|---|---|
| Claude (coordinator) | Shared architecture and build, native loader/runtime, reference shader interpreter, Vulkan backend, integration and reviews |
| Codex | Shader decoding, IR, SPIR-V translation, shader ABI and compiler verification |

Use [TASKS.md](TASKS.md) for current upstream assignments and owned paths. A consumer can begin against a frozen public interface merged to `integration`, before its producer's implementation is finished. Interface changes require a coordinator-approved note in the task registry.

## Durable handoff

Keep task status and dependencies in [TASKS.md](TASKS.md), verified results in [PROGRESS.md](PROGRESS.md), and concise personal context in `docs/agents/<agent>/HANDOFF.md`. A handoff should contain the working branch, next action, outstanding reviews, blockers and exact test evidence that has not already been recorded in PROGRESS. Refer to canonical records instead of copying long task descriptions or milestone text.

The repository, rather than an agent's conversation history, is the shared source of truth.
