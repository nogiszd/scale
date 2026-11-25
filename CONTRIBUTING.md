# Contributing to SCALE

SCALE is developed through a small, repeatable pipeline: a shared task
registry, one branch per task, explicit ownership of files, test gates, and a
single integrator who merges. The same pipeline works for people and for
coding agents (Claude Code, Codex, or your own), and for any mix of them.
This guide explains how to join it.

Read [AGENTS.md](AGENTS.md) first. It gives the project's mission and core engineering rules. Detailed milestone
criteria are in [docs/ROADMAP.md](docs/ROADMAP.md). Everything
below assumes it.

## 1. Set up a machine

Requirements: Windows 11 x64, Visual Studio 2022+ (or Build Tools) with
*Desktop development with C++*, and Git. For graphics and shader work you
also need the LunarG Vulkan SDK and LLVM with the AMDGPU target.

```powershell
git clone https://github.com/nogiszd/scale.git
cd scale
. .\scripts\vsenv.ps1              # MSVC x64 environment for this session
.\scripts\check-env.ps1            # reports what is installed
.\scripts\build-llvm-amdgpu.ps1 -AddToUserPath   # only for shader work (10-30 min)
cmake --preset debug
cmake --build --preset debug
ctest --preset full
```

The first configure downloads GoogleTest 1.18.0, pinned and hash-checked.
Nothing is installed machine-wide, and no administrator rights are needed.

## 2. Roles

| Role | Responsibilities |
|---|---|
| **Coordinator** (one per repository) | Owns the root build, shared libraries (`src/common`, `src/diagnostics`), the task registry and the `integration`/`main` branches. Assigns tasks, freezes interfaces, reviews and merges. Currently the Claude agent, supervised by the maintainer. |
| **Contributor** (person or agent) | Works on assigned tasks in their own branch and directories, tests them, and hands them over for review. |
| **Maintainer** | The human owner. Approves the roster, priorities, pushes and releases. |

Current contributors and their areas are listed in
[docs/MULTI_AGENT.md](docs/MULTI_AGENT.md) and
[docs/architecture.md](docs/architecture.md).

## 3. Join the pipeline

1. **Introduce yourself.** Open an issue (or ask the maintainer) naming the
   area you want to work on. New contributors get:
   - a name used in branch names (`alice`, `codex`, …);
   - a handoff file `docs/agents/<name>/HANDOFF.md`;
   - one or more tasks in [docs/TASKS.md](docs/TASKS.md) with owned paths.
2. **Never self-assign** a task the coordinator hasn't given you. Read
   `docs/TASKS.md` on the current `integration` branch, not a stale copy.
3. **Use your own worktree** so builds and branches never collide:

   ```powershell
   git fetch origin
   git worktree add ..\scale-wt\<name> -b <name>/<task-id>-<slug> origin/integration
   ```

   Each worktree has its own `out/` build tree.

## 4. Work on a task

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

## 5. Hand a task over

Before asking for review:

- [ ] `ctest --preset full` is green in Debug, and `release-full` for library
      changes. Shader work also builds with `-DSCALE_ENABLE_SHADER=ON`.
- [ ] Every new error code or rejection path has a negative test.
- [ ] Contracts and limitations are documented, including what is *not*
      supported.
- [ ] Commits are on your task branch, with messages starting with the task id:
      `T-123: parse section headers`. Keep messages factual: what changed and
      how it was verified.
- [ ] Your task status in `docs/TASKS.md` is `REVIEW`.
- [ ] `docs/agents/<name>/HANDOFF.md` lists the commands you ran, their
      actual results (counts, skips, tool versions) and known limitations.

Then push your branch and open a pull request **against `integration`**, or
tell the maintainer the branch is ready if you work in the local repository.

## 6. Review and merge (coordinator)

The coordinator does the following for every task:

1. Reads the code and the handoff. Doesn't take reported results on trust.
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

## 7. Using the pipeline with your own agents

Any coding agent can take part. Give it repository access through its own
worktree and a first prompt like this:

> You are the `<name>` agent for SCALE. Read AGENTS.md, CONTRIBUTING.md,
> docs/MULTI_AGENT.md, docs/TASKS.md, docs/architecture.md, the relevant
> docs/contracts/ files and docs/agents/<name>/HANDOFF.md. You work in your
> own worktree on branch `<name>/<task-id>-<slug>`. Do only the tasks assigned
> to you, stay inside their owned paths, and follow the shared-file rules.
> When a task meets its acceptance criteria, commit, set it to REVIEW in
> docs/TASKS.md, record the commands and results in your HANDOFF.md, and stop
> for review. Never commit to `integration` or `main`.

Useful follow-up prompts:

- **Review feedback:** "Coordinator review of `<task>`: required changes: …;
  optional: …. Amend on your branch, re-run the checks, set REVIEW again."
- **Next task:** "`<task>` is merged into `integration` (`<commit>`). Start
  `<next-task>` on `git switch -c <name>/<next-task> integration`."

Agent configuration files in the root (`CLAUDE.md`, and the `AGENTS.md`
that Codex reads natively) all point to the same rules. For a tool that reads
a different file, add a one-line pointer to `AGENTS.md`, not a copy.

## 8. Reporting results and compatibility

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

## Development environment checks

Windows 11 x64 with MSVC x64, Windows SDK, CMake and Ninja is the initial target. Check `git --version`, `cmake --version`, `ninja --version`, `Get-Command cl, link, dumpbin` and the compiler/SDK architecture. Verify used C++23 features with the installed compiler. Do not auto-install dependencies or change machine-wide settings; report missing tools and exact installation steps.

For graphics work, verify `vulkaninfo --summary` and `spirv-val --version`; for shader decoding, verify `llvm-mc --version`, `llvm-objdump` and AMDGPU target registration. Pin and record tool versions. Public PC GDK does not establish console GDKX compatibility. See [portability decision 0004](docs/decisions/0004-platform-portability.md).
