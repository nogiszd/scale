// fixture_hang.exe — non-terminating fixture (T-203).
//
// Entry contract: int fixture_entry(void), no CRT, no imports.
// The entry spins forever; the supervisor's timeout must stop it.
// A global counter keeps the entry a leaf function without unwind data.

extern "C" {
volatile unsigned fixture_counter = 0;
}

extern "C" int fixture_entry() {
    for (;;) {
        fixture_counter = fixture_counter + 1;
    }
}
