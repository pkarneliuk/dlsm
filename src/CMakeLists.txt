find_package(spdlog REQUIRED)
find_package(ZeroMQ REQUIRED)

file(GLOB_RECURSE PUBLIC_HDRS   ../include/dlsm/*.hpp ${VERSION_HPP})
file(GLOB_RECURSE INTERNAL_HDRS ../include/impl/*.hpp)

add_library               (dlsm STATIC  ${INTERNAL_HDRS} ${PUBLIC_HDRS})
target_precompile_headers (dlsm PRIVATE ${INTERNAL_HDRS} ${PUBLIC_HDRS})
target_include_directories(dlsm PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/gen/include>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>"
)
target_sources            (dlsm PRIVATE
    Logger.cpp
    SharedMemory.cpp
    Signal.cpp
    Str.cpp
    Thread.cpp
    Transport.cpp
)
target_link_libraries     (dlsm
    PUBLIC
        Options
        $<TARGET_NAME_IF_EXISTS:Coverage>
        fbs
        spdlog::spdlog
        atomic
    PRIVATE
        libzmq-static
)

add_library               (dlsmRuntime SHARED Public.cpp ${INTERNAL_HDRS} ${PUBLIC_HDRS})
set_target_properties     (dlsmRuntime PROPERTIES PUBLIC_HEADER "${PUBLIC_HDRS}")
target_link_libraries     (dlsmRuntime PUBLIC Options PRIVATE dlsm)
target_include_directories(dlsmRuntime PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/gen/include>"
    "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>"
)
