# Shared compiler and linker settings, per compiler (decision 0004).
#
# Every first-party target links `scale::build_options`. Test executables that
# need a non-standard entry contract or no CRT define their own flags instead,
# but must keep ASLR/DEP/CFG unless a test documents why not.

add_library(scale_build_options INTERFACE)
add_library(scale::build_options ALIAS scale_build_options)

if(MSVC)
    target_compile_options(scale_build_options INTERFACE
        /W4
        /permissive-
        /utf-8
        /EHsc
        /Zc:__cplusplus
        /Zc:preprocessor
        /Zc:inline
        /guard:cf
        /sdl
    )
    if(SCALE_WARNINGS_AS_ERRORS)
        target_compile_options(scale_build_options INTERFACE /WX)
    endif()

    target_compile_definitions(scale_build_options INTERFACE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _WIN32_WINNT=0x0A00
    )

    # ASLR, high-entropy ASLR, DEP, CFG and CET shadow stack compatibility.
    target_link_options(scale_build_options INTERFACE
        /DYNAMICBASE
        /HIGHENTROPYVA
        /NXCOMPAT
        /GUARD:CF
        /CETCOMPAT
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # UNVERIFIED until T-802 runs a Linux build. Equivalent hardening:
    # PIE (ASLR), non-executable stack (DEP), full RELRO, stack protector,
    # fortified libc calls and CET where the toolchain supports it.
    target_compile_options(scale_build_options INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -fstack-protector-strong
        -fcf-protection=full
        -fPIE
        $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=2>
    )
    if(SCALE_WARNINGS_AS_ERRORS)
        target_compile_options(scale_build_options INTERFACE -Werror)
    endif()
    target_link_options(scale_build_options INTERFACE
        -pie
        -Wl,-z,relro,-z,now
        -Wl,-z,noexecstack
    )
else()
    message(FATAL_ERROR "No SCALE compiler settings for ${CMAKE_CXX_COMPILER_ID}")
endif()
