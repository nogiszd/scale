scale_add_library(shader_decode
    SOURCES decode.cpp
    DEPS scale::common)

scale_add_library(shader_ir
    SOURCES ir.cpp
    DEPS scale::shader_decode)
