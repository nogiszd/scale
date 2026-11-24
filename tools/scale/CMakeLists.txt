add_executable(scale
    main.cpp
    commands.cpp
    inspect_command.cpp
)
target_link_libraries(scale PRIVATE
    scale::common
    scale::diagnostics
    scale::loader
    scale::build_options
)
