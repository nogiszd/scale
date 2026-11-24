add_executable(scale
    main.cpp
    commands.cpp
)
target_link_libraries(scale PRIVATE
    scale::common
    scale::diagnostics
    scale::build_options
)
