// test_exit.exe — self-authored PE test executable for inspector comparison (T-102).
//
// Contract: no CRT. Entry point `test_entry` calls kernel32!ExitProcess(42).
// Imports: KERNEL32.dll!ExitProcess by name (only).
// Data: an initialized global and an absolute pointer to it, which produces a
// DIR64 base relocation under /DYNAMICBASE.
// Used for inspection only in T-102; it is not executed by these tests.

extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int exit_code);

extern "C" {
int test_value = 42;
int* volatile test_value_pointer = &test_value;  // absolute address -> relocation
}

extern "C" void test_entry() {
    ExitProcess(static_cast<unsigned int>(*test_value_pointer));
}
