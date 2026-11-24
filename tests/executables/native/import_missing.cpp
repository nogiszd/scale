// test_import_missing.exe — unresolved synthetic import (T-204).
//
// Entry contract: int test_entry(void), no CRT.
// Imports: test_platform.dll!TestAllocate (by name) and ordinal 2 (TestFree,
// exported NONAME). Returns TestAllocate(40) + TestFree(2) = 42.
//
// Native reference: the Windows loader finds test_platform.dll next to the
// executable and the process exits with 42.
// SCALE: no adapter, approved guest module or permitted Windows export
// provides either import, so the run is BLOCKED before mapping, with one
// runtime/import_resolution/unresolved_import event per import.

extern "C" __declspec(dllimport) int TestAllocate(int n);
extern "C" __declspec(dllimport) int TestFree(int n);

extern "C" int test_entry() {
    return TestAllocate(40) + TestFree(2);
}
