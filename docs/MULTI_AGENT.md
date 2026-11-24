# Multi-Agent Development

Scale supports concurrent development by Claude Code, Codex and other coding agents.

## Coordination

- Read `AGENTS.md` and current project documentation before starting work.
- Use `docs/TASKS.md` as the shared task registry.
- Work on assigned, unblocked tasks that are not claimed by another agent.
- Each task must have an ID, owner, status, scope and dependencies.
- Task assignment must be coordinated before implementation begins.

## Isolation

- Use a separate Git branch and worktree for each agent.
- Keep build directories and temporary artifacts isolated.
- Do not modify files or modules actively owned by another agent.
- Coordinate changes to shared interfaces, dependencies and root build configuration.
- Never overwrite or revert another agent's work.

## Implementation

- Work independently on unrelated tasks whenever possible.
- Prefer small, complete and testable implementation increments.
- Follow existing architecture and coding conventions.
- Run relevant tests before marking a task as completed.
- Report blockers instead of introducing speculative implementations.
- Review other agents' changes when requested.

## Persistent Memory

Maintain concise agent-specific progress and handoff notes under `docs/agents/<agent-name>/`.

Record:

- Completed tasks and commits.
- Test results and known limitations.
- Technical discoveries and decisions.
- Cross-module dependencies.
- Blockers and recommended next steps.

Update documentation after meaningful implementation increments, not after every minor change.

## Integration

- Commit completed and verified changes.
- Synchronize with the integration branch between tasks.
- Only the designated coordinator may assign conflicting tasks, resolve cross-module changes and merge into the main branch.
- Do not automatically claim tasks from stale local copies of `TASKS.md`.

The repository is the shared source of truth. Development must not depend on individual agent conversation history.

## Initial Agent Responsibilities

### Claude — Architecture, Integration and Graphics

- Project foundation and shared infrastructure.
- Architecture, CMake and shared interfaces.
- Native execution, PE loader and system runtime.
- Independent RDNA2 reference interpreter.
- Vulkan backend and graphics translation.
- GPU execution and differential shader verification.
- Graphics diagnostics and integration tests.
- Task coordination, integration and code review.
- Integration testing and regression testing.
- Sole authority to merge into the integration and main branches.

### Codex — Shader Compiler

- RDNA2 instruction decoding.
- Shader intermediate representation (IR).
- Direct SPIR-V recompilation.
- Shader ABI and resource mapping.
- Shader compiler correctness and optimization.
- SPIR-V validation.
- Compiler diagnostics and unit tests.
- Shader regression testing.
- Shader compiler integration with the Vulkan backend.

## Git Workflow

The canonical remote is `origin` (https://github.com/nogiszd/scale). Only
the maintainer decides what gets pushed. Codex works in its own worktree of
the same local repository. Claude (coordinator) works in the main checkout on
its own task branches. External contributors follow
[CONTRIBUTING.md](../CONTRIBUTING.md): fork or branch, work in a worktree,
and open pull requests against `integration`.

Branches:

- `main`: verified, merged state. Coordinator only.
- `integration`: the coordinator merges REVIEW branches here and runs the full
  suite, then fast-forwards `main`.
- `<agent>/<task-id>-<slug>`, e.g. `codex/T-701-decoder`: one branch per
  task, created from the current `integration`.

Worktree setup (run once per agent from the main checkout):

```powershell
git worktree add ..\scale-wt\codex -b codex/T-704-shader-abi integration
```

Each worktree has its own `out/` build tree, so builds never collide. To start
the next task: `git switch -c <agent>/<next-task> integration` inside your
worktree, after `integration` has been updated.

Finishing a task:

1. `ctest --preset full` is green in Debug, and in Release for library
   changes.
2. Commit on your task branch. Messages start with the task id:
   `T-101: parse section headers`.
3. Set the task status to `REVIEW` in `docs/TASKS.md` on your branch and
   update `docs/agents/<you>/HANDOFF.md` with the commands you ran and their
   results.
4. Tell the user the branch is ready. The coordinator reviews and merges.

Never rebase or force-update a branch another agent may have checked out,
and never commit to `integration` or `main` yourself.

## Testing Policy

Every agent is responsible for testing its own implementation.

The shader compiler and reference interpreter must remain independently implemented to reduce correlated semantic errors.

Use shared CTest infrastructure with quick, gpu and full test profiles.

GPU tests must be coordinated when concurrent execution could interfere with correctness or performance measurements.

## Merge Policy

Only the integration coordinator may merge changes into main.

Before merging:

1. Review the implementation and its scope.
2. Verify module-specific test results.
3. Integrate changes on an integration branch.
4. Build the complete project.
5. Run the full regression suite.
6. Resolve or explicitly document integration blockers.
7. Merge only verified changes.
8. Update the shared task registry.

Agents must commit completed tasks and synchronize with the integration branch before starting dependent work.

Initial responsibilities may be reassigned by the coordinator as the project evolves.
