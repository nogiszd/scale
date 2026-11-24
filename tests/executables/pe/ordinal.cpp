// test_ordinal.exe — self-authored PE test executable for inspector comparison (T-102).
//
// Contract: no CRT. Imports test_lib.dll by ordinal 7 (TestAdd is
// exported NONAME) and by name (TestMul), plus KERNEL32.dll!ExitProcess.
// Used for inspection only in T-102; it is not executed by these tests.

extern "C" __declspec(dllimport) void __stdcall ExitProcess(unsigned int exit_code);
extern "C" __declspec(dllimport) int TestAdd(int a, int b);
extern "C" __declspec(dllimport) int TestMul(int a, int b);

extern "C" void test_entry() {
    ExitProcess(static_cast<unsigned int>(TestAdd(40, 2) + TestMul(0, 1)));
}
