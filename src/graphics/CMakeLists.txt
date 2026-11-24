# Host graphics (T-601): Vulkan compute harness. Built with
# -DSCALE_ENABLE_GRAPHICS=ON; requires the Vulkan SDK headers and loader.
find_package(Vulkan REQUIRED)
message(STATUS "Vulkan for graphics: ${Vulkan_VERSION} (${Vulkan_LIBRARY})")

scale_add_library(graphics
    SOURCES
        vulkan_compute.cpp
    DEPS
        scale::common
        Vulkan::Vulkan
)
