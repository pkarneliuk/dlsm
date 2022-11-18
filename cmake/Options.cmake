
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

if(CMAKE_BYILD_TYPE MATCHES "Release")
    target_compile_options(Options INTERFACE
    #    -fvisibility=hidden
    #    -fvisibility-inlines-hidden
    )
    target_link_options(Options INTERFACE
        -Wl,--exclude-libs,ALL # Exclude public symbols from static libs
        #-s                     # Strip all symbols
    )
endif()
