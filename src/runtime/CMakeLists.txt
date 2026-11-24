scale_add_library(runtime
    SOURCES
        import_resolver.cpp
    DEPS
        scale::loader
        scale::diagnostics
)
