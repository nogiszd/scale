add_executable(scale
    main.cpp
    commands.cpp
    inspect_command.cpp
    run_command.cpp
)
target_link_libraries(scale PRIVATE
    scale::common
    scale::diagnostics
    scale::loader
    scale::host
    scale::build_options
)
# scale run launches scale_worker.exe from the same directory.
add_dependencies(scale scale_worker)
