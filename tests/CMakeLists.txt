add_library(scale_test_support STATIC support/scale_test_main.cpp)
add_library(scale::test_support ALIAS scale_test_support)
target_include_directories(scale_test_support PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/support")
target_link_libraries(scale_test_support PRIVATE scale::build_options)

add_subdirectory(unit)
add_subdirectory(integration)
