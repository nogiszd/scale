# Independent reference interpreter (T-711). Depends on the decoder's public
# interface only, never on shader_ir or shader_spirv (AGENTS.md M7).
scale_add_library(shader_ref
    SOURCES
        interpreter.cpp
    DEPS
        scale::shader_decode
        scale::common
)
