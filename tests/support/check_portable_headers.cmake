# Decision 0004 rule 1: portable components include no OS headers.
# cmake -DSCALE_SOURCE_DIR=<repo> -P check_portable_headers.cmake
#
# Scans the portable component directories (excluding *_win32.cpp and
# *_posix.cpp/*_linux.cpp implementation files) for OS header includes.

set(portable_dirs
    src/common
    src/diagnostics
    src/runtime
    src/shader
    src/shader_ref
    src/graphics)
set(portable_files
    src/loader/pe_image.cpp
    src/loader/include/scale/loader/pe_image.h
    src/loader/include/scale/loader/image_map.h)
set(forbidden "#[ \t]*include[ \t]*<(windows|winternl|bcrypt|shellapi|unistd|dlfcn|pthread|fcntl)\\.h>|#[ \t]*include[ \t]*<sys/")

set(candidates "")
foreach(dir IN LISTS portable_dirs)
    if(EXISTS "${SCALE_SOURCE_DIR}/${dir}")
        file(GLOB_RECURSE found "${SCALE_SOURCE_DIR}/${dir}/*.h" "${SCALE_SOURCE_DIR}/${dir}/*.cpp")
        list(APPEND candidates ${found})
    endif()
endforeach()
foreach(f IN LISTS portable_files)
    list(APPEND candidates "${SCALE_SOURCE_DIR}/${f}")
endforeach()

set(violations "")
set(checked 0)
foreach(file IN LISTS candidates)
    if(file MATCHES "_(win32|posix|linux)\\.cpp$")
        continue()
    endif()
    math(EXPR checked "${checked} + 1")
    file(STRINGS "${file}" lines REGEX "${forbidden}")
    foreach(line IN LISTS lines)
        list(APPEND violations "${file}: ${line}")
    endforeach()
endforeach()

message("checked ${checked} portable source files")
if(violations)
    list(JOIN violations "\n" text)
    message(FATAL_ERROR "OS headers in portable components (decision 0004):\n${text}")
endif()
if(checked EQUAL 0)
    message(FATAL_ERROR "no portable source files found under ${SCALE_SOURCE_DIR}")
endif()
