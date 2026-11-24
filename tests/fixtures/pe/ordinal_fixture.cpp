// fixture_ordinal.exe — self-authored PE fixture for inspector comparison (T-102).
//
// Contract: no CRT. Imports fixture_lib.dll by ordinal 7 (FixtureAdd is
// exported NONAME) and by name (FixtureMul), plus KERNEL32.dll!ExitProcess.
// Used for inspection only in T-102; it is not executed by these tests.

extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int exit_code);
extern "C" __declspec(dllimport) int FixtureAdd(int a, int b);
extern "C" __declspec(dllimport) int FixtureMul(int a, int b);

extern "C" void fixture_entry() {
    ExitProcess(static_cast<unsigned int>(FixtureAdd(40, 2) + FixtureMul(0, 1)));
}
