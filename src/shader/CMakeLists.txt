add_library(scale_shader_decode INTERFACE)
add_library(scale::shader_decode ALIAS scale_shader_decode)
target_sources(scale_shader_decode INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES include/scale/shader/decode.h
)
target_link_libraries(scale_shader_decode INTERFACE scale::common)
