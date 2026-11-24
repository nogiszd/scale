// fixture_lib.dll — self-authored PE fixture for inspector comparison (T-102).
//
// Contract: no CRT, no imports, entry point `fixture_dll_entry` returns TRUE.
// Exports (lib_fixture.def): FixtureAdd @7 NONAME (ordinal only), FixtureMul by name.

extern "C" int __stdcall fixture_dll_entry(void*, unsigned long, void*) {
    return 1;
}

extern "C" int FixtureAdd(int a, int b) {
    return a + b;
}

extern "C" int FixtureMul(int a, int b) {
    return a * b;
}
