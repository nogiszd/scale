// fixture_ret42.exe — first native execution fixture (T-202).
//
// Entry contract (custom fixture entry, NOT a CRT entry point):
//   int fixture_entry(void)   — the image's AddressOfEntryPoint
//   returns the value of the initialized global `fixture_result` (42)
// Dependencies: none (no CRT, no imports).
// Native reference: Windows runs the entry as the initial thread's start
// routine; its return value became the process exit code in 30/30 observed
// runs (Windows 11 26200). Scale's worker maps the image and calls the entry
// directly; the result must match.

extern "C" {
volatile int fixture_result = 42;  // initialized data in .data
}

extern "C" int fixture_entry() {
    return fixture_result;
}
