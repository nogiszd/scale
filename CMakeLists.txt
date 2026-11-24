cmake_minimum_required(VERSION 3.28)

project(scale
    VERSION 0.1.0
    DESCRIPTION "Scarlett Compatibility and Layered Execution"
    LANGUAGES CXX)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Scale targets Windows x64 only (system: ${CMAKE_SYSTEM_NAME}, pointer size: ${CMAKE_SIZEOF_VOID_P}).")
endif()
if(NOT MSVC)
    message(FATAL_ERROR "Scale requires the MSVC toolchain. Configure from a Developer PowerShell or run scripts/vsenv.ps1 first.")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# C++ modules are not used; skip the per-file dependency scan.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# All executables and DLLs land in one directory so tests and the CLI can find
# each other without PATH manipulation.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

option(SCALE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(SCALE_BUILD_TESTS "Build the CTest suite" ON)
# Optional core tracks. They are mandatory deliverables, but their host
# toolchains (Vulkan SDK, LLVM AMDGPU) are not required to build the CPU track.
option(SCALE_ENABLE_GRAPHICS "Build the host graphics component (requires Vulkan SDK)" OFF)
option(SCALE_ENABLE_SHADER "Build the RDNA2 shader recompiler component" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ScaleOptions)
include(ScaleComponents)

if(SCALE_BUILD_TESTS)
    enable_testing()
    include(ScaleTesting)
endif()

# Components in dependency order (lower levels first). A component directory is
# added only once its owner creates it; absence is reported, never hidden.
scale_add_component(src/common)
scale_add_component(src/diagnostics)
scale_add_component(src/loader)
scale_add_component(src/filesystem)
scale_add_component(src/runtime)
scale_add_component(src/host)
if(SCALE_ENABLE_SHADER)
    # Compiler (decoder, IR, SPIR-V) and the independent reference interpreter
    # are separate components with separate owners; see docs/TASKS.md.
    scale_add_component(src/shader REQUIRED)
    scale_add_component(src/shader_ref)
endif()
if(SCALE_ENABLE_GRAPHICS)
    scale_add_component(src/graphics REQUIRED)
endif()
scale_add_component(tools/scale REQUIRED)

if(SCALE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
