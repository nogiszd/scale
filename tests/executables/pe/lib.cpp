// test_lib.dll — self-authored PE test executable for inspector comparison (T-102).
//
// Contract: no CRT, no imports, entry point `test_dll_entry` returns TRUE.
// Exports (lib.def): TestAdd @7 NONAME (ordinal only), TestMul by name.

extern "C" int __stdcall test_dll_entry(void*, unsigned long, void*) {
    return 1;
}

extern "C" int TestAdd(int a, int b) {
    return a + b;
}

extern "C" int TestMul(int a, int b) {
    return a * b;
}
