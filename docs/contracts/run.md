# `scale run` contract

```text
scale run <executable> [--report <path>] [--timeout-ms <n>]
```

Runs a **trusted, self-authored test executable** in a supervised worker process
(`scale_worker.exe`, which must sit next to `scale.exe`). This is not a
general executable runner. The test executable must use the custom entry contract
`int test_entry(void)` and stay within the Scale loader's subset
(docs/decisions/0002-native-image-loading.md). Guest arguments (`-- args`)
are rejected because the entry contract has no parameters.

Defaults: timeout 10 000 ms (1 to 3 600 000), job memory limit 1 GiB, and
1 MiB captured per output stream.

## Outcome and exit codes

| Outcome | Meaning | `scale run` exit |
|---|---|---|
| `PASS` | the entry returned; the value is printed and stored in `exit_code` | 0 |
| `BLOCKED` | the loader rejected an unsupported feature (`unsupported_*`, `relocation_required`, `writable_executable_section`, `entry_not_executable`, `unresolved_import`) | 1 |
| `FAILED` | malformed or unreadable input, unhandled exception, timeout, unexpected worker exit | 1 |

Usage errors exit 2. A failure to write `--report` also makes the exit 1.

`PASS` means only that the run completed. Checking the returned value
against a test executable's documented expectation is the test's job.

## Worker report channel (internal protocol)

The worker writes newline-terminated lines to the report pipe (see
`scale/host/supervisor.h`):

```text
stage start
stage inspected
stage resolved imports=<n>          (only when the image has imports and all resolved)
stage mapped base=0x<hex> size=0x<hex> delta=<signed> relocations=<n>
stage entry
stage returned value=<int>
error stage=<read|inspect|map> component=<c> code=<code> message=<text>
exception code=0x<8 hex> address=0x<16 hex>
event component=<c> stage=<s> code=<k> severity=<...> fatal=<0|1> a.<key>=<v> ... message=<v>
```

`event` lines use the percent-encoded format in `scale/diagnostics/event_line.h`. At most 64 `unresolved_import` events are sent, followed by one `unresolved_imports_omitted` event (attribute `count`) if there are more.

Only this channel is trusted. Guest stdout and stderr are captured but never
parsed.

## report.json

`--report` writes report schema v1 (docs/contracts/diagnostics.md):

- `command` is `"run"`. `subject` is `{kind: "executable", name: <path as given>,
  sha256: <hex or null>}`.
- `final_stage` is the last `stage` line (`launch` if none), e.g. `returned`,
  `entry`, `inspected`.
- `exit_code` is the returned value for `PASS`, otherwise `null`.
- Diagnostic events (all fatal unless noted):
  - worker `event` lines, in order (e.g. `runtime/import_resolution/unresolved_import` with `module`, `symbol` or `ordinal`, `importing_module`, `import_kind`). The first fatal event sets the outcome;
  - loader errors: `loader/<stage>/<code>`, with the worker's message;
  - `host/<final_stage>/unhandled_exception` with `exception_code`,
    `address`, and `rva` + `module` when the address lies inside the mapped
    image;
  - `host/worker/timeout` with `timeout_ms`, `last_stage`;
  - `host/worker/abnormal_exit` or `host/worker/unexpected_exit` with
    `exit_code`, `last_stage`;
  - `host/worker/report_truncated` (warning, non-fatal).

Reports stay out of version control (`/reports/` and `report.json` are
git-ignored).
