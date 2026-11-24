# `scale inspect` output contract (schema_version 1)

`scale inspect <file> [--json]` reads a PE32+ file read-only with
`scale::loader::inspect_pe_file`. It never maps or executes the file.

Exit codes: `0` inspected, `1` rejected or unreadable (an error is printed as
`component/code: message`, and with `--json` an error document is written),
`2` usage error.

## JSON (`--json`)

Success document, written to stdout:

```json
{
  "schema_version": 1,
  "tool": { "name": "scale", "version": "0.1.0" },
  "file": "test_exit.exe",
  "result": "ok",
  "image": {
    "machine": "0x8664",
    "characteristics": "0x0022",
    "is_dll": false,
    "time_date_stamp": "0x6b05c469",
    "linker_version": "14.52",
    "entry_point_rva": "0x1000",
    "image_base": "0x140000000",
    "section_alignment": "0x1000",
    "file_alignment": "0x200",
    "size_of_image": "0x6000",
    "size_of_headers": "0x400",
    "subsystem": 3,
    "subsystem_version": "6.0",
    "dll_characteristics": "0x8160",
    "stack_reserve": "0x100000", "stack_commit": "0x1000",
    "heap_reserve": "0x100000", "heap_commit": "0x1000",
    "number_of_rva_and_sizes": 16
  },
  "directories": [ { "name": "import_table", "rva": "0x21e4", "size": "0x28" } ],
  "sections": [
    { "name": ".text", "virtual_address": "0x1000", "virtual_size": "0x1f",
      "raw_offset": "0x400", "raw_size": "0x200", "characteristics": "0x60000020",
      "access": "rx" }
  ],
  "imports": [
    { "module": "KERNEL32.dll", "lookup_table_rva": "0x2230", "address_table_rva": "0x2000",
      "entries": [ { "kind": "name", "name": "ExitProcess", "hint": 395 },
                   { "kind": "ordinal", "ordinal": 7 } ] }
  ],
  "relocations": { "blocks": 1, "entries": 2, "by_type": { "absolute": 1, "dir64": 1 } },
  "tls": null,
  "exceptions": { "runtime_functions": 1 }
}
```

Rules:

- Addresses, RVAs, sizes and offsets are strings: `0x` followed by
  lowercase hex without leading zeros. Flag fields are fixed width:
  `machine`, `characteristics` and `dll_characteristics` use 4 hex digits,
  and `time_date_stamp` and section `characteristics` use 8. Counts, hints,
  ordinals and `subsystem` are JSON integers.
- `directories` lists only the directories that are present, in index order,
  using the names from `scale::loader::directory_name`.
- `access` is built from the section's execute/read/write flags: `r`, `w`,
  `x`, in that order, with absent flags omitted.
- `relocations`, `tls` and `exceptions` are `null` when the directory is
  absent. Relocation `by_type` keys: `absolute`, `high`, `low`, `highlow`,
  `dir64`, or `type_<n>` for other types. Zero counts are omitted.
- `tls` (when present): `{ "callbacks_va": "0x...", "callback_rvas": ["0x..."],
  "size_of_zero_fill": n }`.
- String values come from untrusted files. The JSON writer replaces invalid
  UTF-8 with U+FFFD.

Error document (exit `1`):

```json
{
  "schema_version": 1,
  "tool": { "name": "scale", "version": "0.1.0" },
  "file": "bad.exe",
  "result": "error",
  "error": { "component": "loader", "code": "bad_dos_magic", "message": "..." }
}
```

`file` is the path exactly as given on the command line.

## Text (default)

The human-readable output is meant for people. It is not a stable format;
scripts must use `--json`.
