# Native execution fixtures (T-202+). Uses scale_add_pe_fixture from
# tests/fixtures/pe (no CRT, ASLR/HEVA/DEP, /GS kept, /RTC1 removed).

string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

scale_add_pe_fixture(fixture_ret42 EXE
    ENTRY fixture_entry
    SOURCES ret42_fixture.cpp)
