
add_library(Options INTERFACE)

target_compile_options(Options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wno-unused-parameter
)

option(ENABLE_WERROR "Treat warnings as errors" ON)
if(ENABLE_WERROR)
target_compile_options(Options INTERFACE -Werror)
endif()
