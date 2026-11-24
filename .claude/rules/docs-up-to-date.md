# Keep docs up to date

After every larger block of work (a finished task, a merge or review, a
milestone step, a change of plan or task assignment), check whether the docs
still match the repository and update them before reporting:

- `docs/TASKS.md`: board statuses, owners, dependencies, freeze points, open
  task specs.
- `docs/ROADMAP.md`: milestone states and current tracks.
- `docs/PROGRESS.md`: the "Current state" table, suite totals and a dated
  entry with the commands and real results.
- `docs/agents/claude/HANDOFF.md`: next tasks, review queue, open items.
- `README.md` roadmap/status rows, `docs/architecture.md` component states,
  `docs/research/REFERENCES.md` and `KNOWN_UNKNOWNS.md` when the work used a
  new source or closed or found an unknown.

Remove stale "next", "planned" or "in progress" wording instead of piling
notes on top. Record only executed results. Don't edit other agents'
handoff files; flag their stale entries in your own handoff instead.
