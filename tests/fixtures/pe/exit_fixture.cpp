// fixture_exit.exe — self-authored PE fixture for inspector comparison (T-102).
//
// Contract: no CRT. Entry point `fixture_entry` calls kernel32!ExitProcess(42).
// Imports: KERNEL32.dll!ExitProcess by name (only).
// Data: an initialized global and an absolute pointer to it, which produces a
// DIR64 base relocation under /DYNAMICBASE.
// Used for inspection only in T-102; it is not executed by these tests.

extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int exit_code);

extern "C" {
int fixture_value = 42;
int* volatile fixture_value_pointer = &fixture_value;  // absolute address -> relocation
}

extern "C" void fixture_entry() {
    ExitProcess(static_cast<unsigned int>(*fixture_value_pointer));
}
