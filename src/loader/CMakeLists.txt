# PE inspection (pe_image.cpp) is portable. Native image mapping uses the
# Windows virtual memory API and is built on Windows only (decision 0004);
# a POSIX implementation (mmap/mprotect) would be image_map_posix.cpp.
set(_scale_loader_sources pe_image.cpp)
if(WIN32)
    list(APPEND _scale_loader_sources image_map_win32.cpp)
endif()

scale_add_library(loader
    SOURCES ${_scale_loader_sources}
    DEPS
        scale::common
)
