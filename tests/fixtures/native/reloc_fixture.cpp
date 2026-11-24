// fixture_reloc.exe — relocated-data fixture (T-203).
//
// Entry contract: int fixture_entry(void), no CRT, no imports.
// `fixture_table` holds absolute pointers to two initialized globals, so the
// image carries DIR64 base relocations. The entry returns *table[0] + *table[1]
// (40 + 2 = 42). Without relocation the pointers would still refer to the
// preferred base, which the worker never uses for relocatable images.

extern "C" {
volatile int fixture_a = 40;
volatile int fixture_b = 2;
volatile int* volatile fixture_table[2] = {&fixture_a, &fixture_b};
}

extern "C" int fixture_entry() {
    return *fixture_table[0] + *fixture_table[1];
}
