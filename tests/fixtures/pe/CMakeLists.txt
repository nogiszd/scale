# Self-authored PE fixtures (T-102). Built without the CRT so their imports,
# sections and relocations are small and fully specified by the sources.
#
# They do not link scale::build_options: /guard:cf and /sdl need CRT support
# code. They keep /GS (no buffers, so no cookie checks are emitted), ASLR,
# high-entropy ASLR and DEP. /RTC1 (CMake's Debug
# default) is removed here because it requires the CRT.

string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")

function(scale_add_pe_fixture target kind)
    cmake_parse_arguments(PARSE_ARGV 2 ARG "" "ENTRY" "SOURCES;LIBS")
    if(kind STREQUAL "DLL")
        add_library(${target} SHARED ${ARG_SOURCES})
    else()
        add_executable(${target} ${ARG_SOURCES})
    endif()
    target_compile_options(${target} PRIVATE /W4 /WX /Zl)
    target_link_options(${target} PRIVATE
        /NODEFAULTLIB
        /ENTRY:${ARG_ENTRY}
        /SUBSYSTEM:CONSOLE
        /DYNAMICBASE
        /HIGHENTROPYVA
        /NXCOMPAT
        /INCREMENTAL:NO
        /MANIFEST:NO)
    target_link_libraries(${target} PRIVATE ${ARG_LIBS})
endfunction()

scale_add_pe_fixture(fixture_exit EXE
    ENTRY fixture_entry
    SOURCES exit_fixture.cpp
    LIBS kernel32)

scale_add_pe_fixture(fixture_lib DLL
    ENTRY fixture_dll_entry
    SOURCES lib_fixture.cpp lib_fixture.def)

scale_add_pe_fixture(fixture_ordinal EXE
    ENTRY fixture_entry
    SOURCES ordinal_fixture.cpp
    LIBS kernel32 fixture_lib)
