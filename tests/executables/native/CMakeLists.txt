# Native execution test executables (T-202+). Uses scale_add_test_executable from
# tests/executables/pe (no CRT, ASLR/HEVA/DEP, /GS kept, /RTC1 removed).
# Each source documents its entry contract and expected behavior.

string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

scale_add_test_executable(test_ret42 EXE ENTRY test_entry SOURCES ret42.cpp)
scale_add_test_executable(test_reloc EXE ENTRY test_entry SOURCES reloc.cpp)
scale_add_test_executable(test_crash EXE ENTRY test_entry SOURCES crash.cpp)
scale_add_test_executable(test_hang EXE ENTRY test_entry SOURCES hang.cpp)
