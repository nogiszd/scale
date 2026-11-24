# Native execution fixtures (T-202+). Uses scale_add_pe_fixture from
# tests/fixtures/pe (no CRT, ASLR/HEVA/DEP, /GS kept, /RTC1 removed).
# Each source documents its entry contract and expected behavior.

string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

scale_add_pe_fixture(fixture_ret42 EXE ENTRY fixture_entry SOURCES ret42_fixture.cpp)
scale_add_pe_fixture(fixture_reloc EXE ENTRY fixture_entry SOURCES reloc_fixture.cpp)
scale_add_pe_fixture(fixture_crash EXE ENTRY fixture_entry SOURCES crash_fixture.cpp)
scale_add_pe_fixture(fixture_hang EXE ENTRY fixture_entry SOURCES hang_fixture.cpp)
