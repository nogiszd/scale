// fixture_crash.exe — unhandled exception fixture (T-203).
//
// Entry contract: int fixture_entry(void), no CRT, no imports.
// The entry writes through a null pointer: EXCEPTION_ACCESS_VIOLATION
// (0xC0000005) inside the fixture's .text. It never returns.
// Globals (not stack locals) keep the entry a leaf function without unwind
// data, so the image has no exception directory (unsupported in T-202/T-203).

extern "C" {
volatile int* volatile fixture_null_target = nullptr;
}

extern "C" int fixture_entry() {
    *fixture_null_target = 1;
    return 0;
}
