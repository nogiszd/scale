# Known unknowns

Console-specific facts we do not have verified evidence for. Do not implement
behavior that depends on an open item. Record a source and a confidence level
when you close one.

| ID | Unknown | Blocks | Status |
|---|---|---|---|
| U-001 | Console executable format details beyond standard PE32+ (extra directories, section flags, header fields) | M3+ console-specific loading | open: no lawful console binary available |
| U-002 | Console system DLL names, exports and signatures (kernelx etc.) | M4 adapters | open: no verified signatures; never guess |
| U-003 | Console shader container format and any Xbox-specific ISA extensions beyond gfx10.3 | M7 console shaders | open: M7 uses standard gfx1030 only |
| U-004 | Console graphics API object model and ABI (D3D12.X) | M6/M8 mapping | open |
| U-005 | Availability of authorized retail test material | M10 | BLOCKED until available lawfully |
