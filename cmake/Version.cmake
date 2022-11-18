function(configure_version out_file_var_name in_file_path)
    string(REGEX REPLACE ".in$" "" out_file_path ${in_file_path})
    set(bin_out_file_path ${CMAKE_CURRENT_BINARY_DIR}/${out_file_path})

    configure_file(${in_file_path} ${bin_out_file_path} @ONLY)
    set(${out_file_var_name} ${bin_out_file_path} PARENT_SCOPE)
endfunction()

set(BUILD_TYPE "unknown")
if(DEFINED CMAKE_BUILD_TYPE)
    string(TOLOWER ${CMAKE_BUILD_TYPE} BUILD_TYPE)
endif()

string(TIMESTAMP BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

set(COMMIT_HASH "unknown")
find_package(Git)
if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} log -1 --pretty=format:%H\ %ai
    OUTPUT_VARIABLE COMMIT_HASH
    ERROR_VARIABLE  error)
    if(NOT ${error} STREQUAL "" )
        set(COMMIT_HASH "unknown")
    endif()
endif()
