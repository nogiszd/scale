// test_reloc.exe — relocated-data test executable (T-203).
//
// Entry contract: int test_entry(void), no CRT, no imports.
// `test_table` holds absolute pointers to two initialized globals, so the
// image carries DIR64 base relocations. The entry returns *table[0] + *table[1]
// (40 + 2 = 42). Without relocation the pointers would still refer to the
// preferred base, which the worker never uses for relocatable images.

extern "C" {
volatile int test_a = 40;
volatile int test_b = 2;
volatile int* volatile test_table[2] = {&test_a, &test_b};
}

extern "C" int test_entry() {
    return *test_table[0] + *test_table[1];
}
