# Multi-agent coordination

[AGENTS.md](../AGENTS.md) sets the project-wide rules. [CONTRIBUTING.md](../CONTRIBUTING.md) describes worktrees, task handoff, testing and review. This page records only agent-specific coordination.

## Ownership and isolation

- The coordinator (currently Claude) assigns tasks in [TASKS.md](TASKS.md), freezes shared interfaces, reviews changes and merges into `integration` and `main`. Read the current `integration` version before taking an assigned task; do not claim another agent's work or infer ownership from a stale handoff.
- Contributors work on separate task branches and worktrees with separate `out/` directories. Do not edit paths assigned to another agent. Shared-file exceptions and current interface freezes are in [TASKS.md](TASKS.md). Ask the coordinator through your handoff for changes to coordinator-owned files.
- `main` is verified merged work; `integration` receives reviewed task branches; `<agent>/<task-id>-<slug>` is an isolated task branch. The maintainer controls pushes to the remote. Never force-update a branch another agent might use.
- Coordinate GPU tests if simultaneous execution could invalidate correctness or performance measurements.

## Current areas

| Agent | Area |
|---|---|
| Claude (coordinator) | Shared architecture and build, native loader/runtime, reference shader interpreter, Vulkan backend, integration and reviews |
| Codex | Shader decoding, IR, SPIR-V translation, shader ABI and compiler verification |

These areas describe current responsibility; actual tasks and owned paths in [TASKS.md](TASKS.md) take precedence. A consumer can begin against a frozen public interface merged to `integration`, before its producer's implementation is finished. Interface changes require a coordinator-approved note in the task registry.

## Durable handoff

Keep task status and dependencies in [TASKS.md](TASKS.md), verified results in [PROGRESS.md](PROGRESS.md), and concise personal context in `docs/agents/<agent>/HANDOFF.md`. A handoff should contain the working branch, next action, outstanding reviews, blockers and exact test evidence that has not already been recorded in PROGRESS. Refer to canonical records instead of copying long task descriptions or milestone text.

The repository, rather than an agent's conversation history, is the shared source of truth.
