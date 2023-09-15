find_package(flatbuffers REQUIRED)

set(SCHEMAS
    base.fbs
    dlsm.fbs
)

set(generated_h_dir "${CMAKE_CURRENT_BINARY_DIR}/fbs")
foreach(schema ${SCHEMAS})
    get_filename_component(filename ${schema} NAME_WE)
    set(generated_h "${generated_h_dir}/${filename}_generated.h")
    add_custom_command(
        COMMENT "Building ${schema} flatbuffers..."
        DEPENDS flatbuffers::flatc ${schema}
        OUTPUT ${generated_h}
        COMMAND flatbuffers::flatc --cpp-std c++17
            -o ${generated_h_dir}
            -c ${schema}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
    list(APPEND all_generated_h ${generated_h})
endforeach()

add_library     (fbs INTERFACE ${all_generated_h})
#target_sources  (fbs INTERFACE ${all_generated_h})
target_include_directories(fbs INTERFACE
"$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")
target_precompile_headers (fbs INTERFACE ${all_generated_h})
target_link_libraries     (fbs INTERFACE flatbuffers::flatbuffers)

#[[
flatbuffers_generate_headers(TARGET include INCLUDE_PREFIX fbs SCHEMAS
    base.fbs
    dlsm.fbs
    FLAGS --cpp-std c++17
)
get_target_property(GENERATED_H include INTERFACE_SOURCES)
list(FILTER GENERATED_H INCLUDE REGEX "^.*_generated.h$")

# Dummy target to expose fbs headers outside
add_library               (fbs STATIC Fbs.cpp)
target_link_libraries     (fbs PUBLIC include flatbuffers::flatbuffers)
target_precompile_headers (fbs INTERFACE ${GENERATED_H})
]]
