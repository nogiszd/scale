// test_ret42.exe — first native execution test executable (T-202).
//
// Entry contract (custom test executable entry, NOT a CRT entry point):
//   int test_entry(void)   — the image's AddressOfEntryPoint
//   returns the value of the initialized global `test_result` (42)
// Dependencies: none (no CRT, no imports).
// Native reference: Windows runs the entry as the initial thread's start
// routine; its return value became the process exit code in 30/30 observed
// runs (Windows 11 26200). Scale's worker maps the image and calls the entry
// directly; the result must match.

extern "C" {
volatile int test_result = 42;  // initialized data in .data
}

extern "C" int test_entry() {
    return test_result;
}
