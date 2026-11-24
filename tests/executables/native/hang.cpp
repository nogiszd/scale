// test_hang.exe — non-terminating test executable (T-203).
//
// Entry contract: int test_entry(void), no CRT, no imports.
// The entry spins forever; the supervisor's timeout must stop it.
// A global counter keeps the entry a leaf function without unwind data.

extern "C" {
volatile unsigned test_counter = 0;
}

extern "C" int test_entry() {
    for (;;) {
        test_counter = test_counter + 1;
    }
}
