# Shared tool runner for tests (decision 0004: platform implementation files).
if(WIN32)
    set(_scale_run_tool_impl support/run_tool_win32.cpp)
else()
    set(_scale_run_tool_impl support/run_tool_posix.cpp)
endif()
add_library(scale_test_tools STATIC ${_scale_run_tool_impl})
add_library(scale::test_tools ALIAS scale_test_tools)
target_include_directories(scale_test_tools PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_link_libraries(scale_test_tools PUBLIC scale::common PRIVATE scale::build_options)

# Decision 0004 rule 1: no OS headers in portable components.
add_test(NAME portable_components_have_no_os_headers
    COMMAND ${CMAKE_COMMAND} "-DSCALE_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/support/check_portable_headers.cmake")
set_tests_properties(portable_components_have_no_os_headers PROPERTIES LABELS quick TIMEOUT 30)
if(SCALE_PLATFORM_WINDOWS)
    # Windows-only: MSVC-built test executables, worker/CLI integration, dumpbin.
    add_subdirectory(executables/pe)
    add_subdirectory(executables/native)
endif()
add_subdirectory(unit)
if(SCALE_PLATFORM_WINDOWS)
    add_subdirectory(integration)
endif()
if(SCALE_ENABLE_GRAPHICS)
    add_subdirectory(gpu)
endif()
