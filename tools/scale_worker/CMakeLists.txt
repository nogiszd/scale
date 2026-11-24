add_executable(scale_worker main.cpp)
target_link_libraries(scale_worker PRIVATE scale::loader scale::common scale::build_options)
