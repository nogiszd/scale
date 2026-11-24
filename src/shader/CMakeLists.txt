scale_add_library(shader_decode
    SOURCES decode.cpp
    DEPS scale::common)

scale_add_library(shader_ir
    SOURCES ir.cpp ir_v1.cpp
    DEPS scale::shader_decode)

scale_add_library(shader_spirv
    SOURCES spirv.cpp
    DEPS scale::shader_ir)

scale_add_library(shader_testcase
    SOURCES test_case.cpp
    DEPS scale::common)
