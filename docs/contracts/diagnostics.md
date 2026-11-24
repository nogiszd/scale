# Diagnostics contract (schema_version 1)

Implementation: `src/diagnostics` (`scale::diagnostics`). Golden test:
`tests/unit/diagnostics/run_report_test.cpp`.

## Errors inside code

Functions that can fail return `scale::Result<T>` (`std::expected<T, scale::Error>`).
`Error{component, code, message}`:

- `component` is one of: `common`, `loader`, `host`, `runtime`, `filesystem`,
  `diagnostics`, `graphics`, `shader`, `shader_ref`, `cli`.
- `code` is a stable lowercase snake_case identifier, unique within the
  component (e.g. `truncated_header`, `unsupported_opcode`). Tests assert on
  codes, never on messages.
- `message` is English, human-readable, bounded, and free of secrets and raw
  title asset bytes.

## Diagnostic events

A `DiagnosticEvent` is what gets reported to the user and to `report.json`:

| Field | Meaning |
|---|---|
| `component` | as above |
| `stage` | pipeline stage, e.g. `pe_parse`, `image_map`, `import_resolution`, `entry`, `worker`, `decode`, `ir_build`, `spirv_emit`, `spirv_validate`, `gpu_execute` |
| `code` | stable identifier |
| `severity` | `info` / `warning` / `error` |
| `fatal` | `true` if execution cannot safely continue |
| `message` | English text |
| `attributes` | ordered string key/value pairs, keys snake_case |

Standard attribute keys (use these where they apply):
`module`, `symbol`, `ordinal`, `importing_module`, `import_kind`
(`by_name`/`by_ordinal`/`delay`), `rva`, `file_offset`, `section`, `opcode`,
`instruction_offset`, `shader_hash`, `isa_target`, `capability`, `exception_code`,
`timeout_ms`. Numeric values are written as decimal strings unless the key
says otherwise. Addresses and offsets are `0x`-prefixed lowercase hex.

`DiagnosticLog` deduplicates identical events (all fields equal). It keeps
`count`, `first_sequence` and `last_sequence` for each entry and stores at most
4096 distinct entries. Overflow is counted in `diagnostics_dropped`.

## report.json

Top-level fields, in order: `schema_version`, `run_id`, `tool{name,version}`,
`command`, `subject{kind,name,sha256}`, `final_stage`, `outcome`, `exit_code`,
`elapsed_ms`, `diagnostics_total`, `diagnostics_dropped`, `diagnostics[]`.

`outcome` ∈ `PASS`, `FAILED`, `BLOCKED`, `SKIPPED`, `NOT_TESTED`.

The example from AGENTS.md maps to this event:

```json
{
  "component": "runtime",
  "stage": "import_resolution",
  "code": "unresolved_import",
  "severity": "error",
  "fatal": true,
  "message": "...",
  "attributes": { "module": "test_platform.dll", "symbol": "TestAllocate" },
  "count": 1,
  "first_sequence": 1,
  "last_sequence": 1
}
```

Component-specific detail lives under `attributes` rather than at top level,
so it can never collide with contract fields.

## Versioning

- Adding a top-level field or a standard attribute key needs no version bump.
  Document it here and update the golden test.
- Removing, renaming, reordering or retyping a field bumps
  `kReportSchemaVersion` and needs coordinator approval.
- Reports are written outside version control (`/reports/` is git-ignored).
