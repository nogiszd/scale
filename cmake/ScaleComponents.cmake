# scale_add_component(<relative-dir> [REQUIRED])
#
# Adds a component directory if it has a CMakeLists.txt. Planned components
# that do not exist yet are reported in the configure log so their absence is
# visible. REQUIRED turns absence into a configure error.

function(scale_add_component dir)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "REQUIRED" "" "")
    set(full "${CMAKE_SOURCE_DIR}/${dir}")
    if(EXISTS "${full}/CMakeLists.txt")
        add_subdirectory("${full}")
        message(STATUS "SCALE component: ${dir} [present]")
    elseif(ARG_REQUIRED)
        message(FATAL_ERROR "SCALE component ${dir} is required but ${full}/CMakeLists.txt does not exist.")
    else()
        message(STATUS "SCALE component: ${dir} [not yet implemented]")
    endif()
endfunction()

# scale_add_library(<name> [STATIC|SHARED] SOURCES ... [DEPS ...])
#
# Creates target `scale_<name>` with alias `scale::<name>`. Public headers live
# under `<component>/include/scale/<name>/`.

function(scale_add_library name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "SHARED" "" "SOURCES;DEPS")
    if(ARG_SHARED)
        set(kind SHARED)
    else()
        set(kind STATIC)
    endif()
    add_library(scale_${name} ${kind} ${ARG_SOURCES})
    add_library(scale::${name} ALIAS scale_${name})
    target_include_directories(scale_${name} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include")
    target_link_libraries(scale_${name}
        PUBLIC ${ARG_DEPS}
        PRIVATE scale::build_options)
endfunction()
