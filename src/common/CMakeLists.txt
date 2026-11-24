configure_file(version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/scale/common/version.h" @ONLY)

scale_add_library(common
    SOURCES
        error.cpp
        text.cpp
)
target_include_directories(scale_common PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/generated")
