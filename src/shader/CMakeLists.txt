scale_add_library(shader_decode
    SOURCES decode.cpp
    DEPS scale::common)

scale_add_library(shader_ir
    SOURCES ir.cpp
    DEPS scale::shader_decode)

scale_add_library(shader_spirv
    SOURCES spirv.cpp
    DEPS scale::shader_ir)

add_library(scale_shader_testcase INTERFACE)
add_library(scale::shader_testcase ALIAS scale_shader_testcase)
target_sources(scale_shader_testcase INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/scale/shader/test_case.h
)
target_link_libraries(scale_shader_testcase INTERFACE scale::common)
