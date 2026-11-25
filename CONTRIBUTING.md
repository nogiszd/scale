# Contributing to SCALE

SCALE is developed through a small, repeatable pipeline: a shared task
registry, one branch per task, explicit ownership of files, test gates, and
maintainer-led upstream integration. The same pipeline works for people and for
coding agents (Claude Code, Codex, or your own), and for any mix of them.
This guide explains how to join it.

Read [AGENTS.md](AGENTS.md) first. It gives the project's mission and core engineering rules. Detailed milestone
criteria are in [docs/ROADMAP.md](docs/ROADMAP.md). Everything
below assumes it.

## 1. Roles

| Role | Responsibilities |
|---|---|
| **Local coordinator** (optional person or agent) | Plans work, assigns local tasks, coordinates worktrees and reviews changes in their own setup. |
| **Upstream integration** (maintainer or delegate) | Maintains upstream assignments and owned paths, approves shared build and interface changes, and manages review and merges into SCALE's `integration` and `main`. |
| **Contributor** (person or agent) | Works solo or with a local team in their own task branch and directories, tests changes, and submits them for review. |
| **Maintainer** | The human owner. Approves the roster, priorities, pushes and releases. |

See the [example maintainer setup](docs/MULTI_AGENT.md#example-maintainer-setup) and the [upstream task registry](docs/TASKS.md).

## 2. Join the pipeline

1. **Find a free task.** Check the current `integration` copy of [the task
   registry](docs/TASKS.md) and the comments on the [pick-up board](https://github.com/nogiszd/scale/issues/5).
   Pick a listed task with no active owner or claim and with prerequisites
   ready for the listed scope. For entries marked **prep**, use the standalone
   scope shown on the board.
2. **Claim and start.** Post the claim on the pick-up board following the
   instructions there. A solo contributor can start immediately and take the
   work through to a PR. A local coordinator is optional. Keep your changes
   within the listed scope and raise any overlap or shared-file change before
   editing the affected paths.
3. **Use your own worktree** so builds and branches never collide:

   ```powershell
   git fetch origin
   git worktree add ..\scale-wt\<name> -b <name>/<task-id>-<slug> origin/integration
   ```

   Each worktree has its own `out/` build tree. See the [build guide](docs/BUILD.md) for setup and test commands.

## 3. Work on a task

Each task in `docs/TASKS.md` has an ID, owner, status, scope, dependencies,
owned paths and acceptance criteria. Stay inside the owned paths.

**Shared files** have fixed rules (details in `docs/TASKS.md`):

- `tools/scale/commands.cpp`: change only your command's line.
- `tests/unit/CMakeLists.txt`, `tests/integration/CMakeLists.txt`: append your
  own delimited block. Don't edit other blocks.
- `docs/TASKS.md`: change only the status of your own tasks.
- Everything else outside your paths (root CMake, `cmake/`, presets,
  `src/common`, `src/diagnostics`, `tests/support`) belongs to the
  coordinator. Ask for changes under *Requests to coordinator* in your
  `HANDOFF.md`.

**Interfaces other people consume** are frozen in two steps. First you submit
the public header (or contract document) alone for review. It is merged and
recorded as *Frozen* in `docs/TASKS.md`. Then you implement against it.
Changing a frozen interface needs coordinator approval.

**Conventions:** C++23/MSVC with `/W4 /WX`. Untrusted bytes go through
`scale::ByteView` and the `checked_*` helpers. Errors are `scale::Result<T>`
with stable `component/code` identifiers. Diagnostics follow
[docs/contracts/diagnostics.md](docs/contracts/diagnostics.md). Tests are
GoogleTest via `scale_add_test` (see
[docs/contracts/testing.md](docs/contracts/testing.md)). Each test has exactly
one label: `quick`, `integration` or `gpu`. Windows is the first platform and Linux a planned one: follow [decision 0004](docs/decisions/0004-platform-portability.md). No OS headers in shader, graphics, diagnostics or PE inspection code, and OS-specific code goes in `_win32.cpp` files behind platform-neutral headers. Test executables are source built
by CMake under `tests/executables/`. Binaries are never committed.

## 4. Hand a task over

Before asking for review:

- [ ] `ctest --preset full` is green in Debug, and `release-full` for library
      changes. Shader work also builds with `-DSCALE_ENABLE_SHADER=ON`.
- [ ] Every new error code or rejection path has a negative test.
- [ ] Contracts and limitations are documented, including what is *not*
      supported.
- [ ] Commits are on your task branch, with messages starting with the task id:
      `T-123: parse section headers`. Keep messages factual: what changed and
      how it was verified.
- [ ] For an assigned task, update its status in `docs/TASKS.md` to `REVIEW`.
      For a pick-up task claimed on the board, link the claim in the PR.
- [ ] Record commands, actual results (counts, skips, tool versions) and known
      limitations in the PR or your existing `HANDOFF.md`. If you used a coding
      agent, include the tool, model name if known, and what it did.

Then push your branch and open a pull request **against `integration`**, or
tell the maintainer the branch is ready if you work in the local repository.

## 5. Upstream review and merge

The maintainer or their delegate does the following for every task:

1. Reads the code and the evidence in the PR or handoff.
2. Rebuilds and reruns the tests in an isolated worktree, including
   independent checks where possible (e.g. LLVM disassembly, dumpbin,
   native reference runs).
3. Either approves, or requests changes as a short list of required versus
   optional items.
4. Merges approved branches into `integration` with `--no-ff`, resolves
   shared-file conflicts by keeping both sides' blocks, and runs the full
   suite on the merged tree.
5. Fast-forwards `main` to `integration`, updates `docs/TASKS.md` and
   `docs/PROGRESS.md`, and deletes the merged branch.

`main` always equals a fully tested `integration`. Nobody else commits to
`integration` or `main`.

## 6. Using the pipeline with your own agents

A contributor working with several agents can coordinate them locally. Give
each agent a separate worktree. Use a prompt like this for an assigned task
or an eligible task claimed on the pick-up board:

> You are the `<name>` agent for SCALE. Read AGENTS.md, CONTRIBUTING.md,
> docs/MULTI_AGENT.md, docs/TASKS.md, docs/architecture.md, the relevant
> docs/contracts/ files and your handoff if one exists. Work in your own
> worktree on branch `<name>/<task-id>-<slug>`. Follow the agreed or listed
> scope and the shared-file rules. Record your commands and results and
> submit the PR against `integration` for review.

Useful follow-up prompts:

- **Review feedback:** "Coordinator review of `<task>`: required changes: …;
  optional: …. Amend on your branch, re-run the checks, set REVIEW again."
- **Next task:** "`<task>` is merged into `integration` (`<commit>`). Start
  `<next-task>` on `git switch -c <name>/<next-task> integration`."

Agent configuration files in the root (`CLAUDE.md`, and the `AGENTS.md`
that Codex reads natively) all point to the same rules. For a tool that reads
a different file, add a one-line pointer to `AGENTS.md`, not a copy.

## 7. Reporting results and compatibility

- **GPU coverage needs you.** If you have a GPU class that hasn't been tested yet (see [docs/contracts/gpu_targets.md](docs/contracts/gpu_targets.md): AMD RDNA2, NVIDIA, Intel, software Vulkan), run `ctest --preset gpu` and send the printed device fingerprint and results. Each new class becomes recorded coverage.

- Record only executed results in `docs/PROGRESS.md`, newest first, with
  commands and counts.
- Keep run reports (`scale run --report`) and traces out of the repository.
- Never call a title compatible or playable based on entry-point or menu
  results. Use the stage vocabulary from docs/ROADMAP.md (NOT_TESTED, BLOCKED,
  FAILED, PASS per stage).

## Questions

Open an issue, or add a *Requests to coordinator* entry to your handoff file.
Discuss structural changes (new dependencies, new components, interface
changes) before you write code.

