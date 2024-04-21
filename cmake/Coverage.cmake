if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(ENABLE_COVERAGE "Enable coverage calculation for GCC/Clang" OFF)
    if(ENABLE_COVERAGE OR DEFINED ENV{ENABLE_COVERAGE})

# Interface library with coverage build options
add_library           (Coverage INTERFACE)
target_compile_options(Coverage INTERFACE --coverage -O0 -g)
target_link_libraries (Coverage INTERFACE --coverage)

# Find tools
find_program(LCOV lcov REQUIRED)
find_program(GENHTML genhtml REQUIRED)

set(GCOVTOOL gcov)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    find_program(LLVMCOV llvm-cov REQUIRED)
    set(GCOVTOOL ${CMAKE_BINARY_DIR}/coverage/llvm-gcov.sh)
    file(WRITE ${GCOVTOOL}
    "#!/bin/bash
exec llvm-cov gcov $@"
    )
    file(CHMOD ${GCOVTOOL} FILE_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)
endif()

# Generate config for LCOV
set(LCOVRC ${CMAKE_BINARY_DIR}/coverage/lcovrc)
file(WRITE ${LCOVRC}
"
geninfo_gcov_tool = ${GCOVTOOL}
lcov_function_coverage = 1
lcov_branch_coverage   = 1
"
)

# Add target for LCOV
add_custom_target(lcov-coverage ALL
    COMMENT "Create LCOV Coverage in ${CMAKE_BINARY_DIR}/coverage"
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/coverage
    BYPRODUCTS  coverage/coverage_base.info
                coverage/coverage_test.info
                coverage/coverage.info
    COMMAND lcov --version
    COMMAND lcov --capture --initial --directory ${CMAKE_BINARY_DIR}
            --config-file ${LCOVRC}
            --base-directory ${CMAKE_SOURCE_DIR}
            --output-file coverage_base.info
            --include '${CMAKE_SOURCE_DIR}/src/*'
            --include '${CMAKE_SOURCE_DIR}/include/*'
            --quiet
    COMMAND lcov --zerocounters --directory ${CMAKE_BINARY_DIR}
            --quiet
    COMMAND $<TARGET_NAME_IF_EXISTS:unit>
    COMMAND lcov --capture --directory ${CMAKE_BINARY_DIR}
            --config-file ${LCOVRC}
            --base-directory ${CMAKE_SOURCE_DIR}
            --output-file coverage_test.info
            --include '${CMAKE_SOURCE_DIR}/src/*'
            --include '${CMAKE_SOURCE_DIR}/include/*'
            --quiet
    COMMAND lcov --config-file ${LCOVRC}
            -a coverage_base.info
            -a coverage_test.info
            --output-file coverage.info
            --quiet
    COMMAND genhtml coverage.info --output-directory .
            --title "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION} ${CMAKE_BUILD_TYPE}"
            --config-file ${LCOVRC}
            --prefix ${CMAKE_SOURCE_DIR}
)
    endif()
endif()

