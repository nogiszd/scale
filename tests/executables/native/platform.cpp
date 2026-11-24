// test_platform.dll — synthetic platform-only API (T-204).
//
// Stands in for a console system module that SCALE has no adapter for.
// Contract: no CRT, no imports, entry `test_dll_entry` returns TRUE.
// Exports (platform.def): TestAllocate (by name), TestFree @2 NONAME.
//   int TestAllocate(int n) returns n
//   int TestFree(int n)     returns n
// The Windows loader can load it (native reference); SCALE must refuse to
// bind to it because it is not an approved guest module.

extern "C" int __stdcall test_dll_entry(void*, unsigned long, void*) {
    return 1;
}

extern "C" int TestAllocate(int n) {
    return n;
}

extern "C" int TestFree(int n) {
    return n;
}
