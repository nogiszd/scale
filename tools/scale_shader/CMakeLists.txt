add_executable(scale-shader main.cpp disassemble.cpp write_module.cpp)
if(WIN32)
    target_sources(scale-shader PRIVATE main_win32.cpp validator_win32.cpp)
else()
    target_sources(scale-shader PRIVATE main_posix.cpp validator_posix.cpp)
endif()
target_link_libraries(scale-shader PRIVATE
    scale::shader_spirv scale::shader_testcase scale::diagnostics scale::build_options)

if(SCALE_BUILD_TESTS)
    include(tests.cmake)
endif()
