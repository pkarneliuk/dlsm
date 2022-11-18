cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)

project(DLSM VERSION ${CONAN_PACKAGE_VERSION} LANGUAGES CXX)
set(CONAN_DISABLE_CHECK_COMPILER True)
conan_basic_setup(TARGETS)

include(cmake/Doxygen.cmake)
include(cmake/Options.cmake)
include(cmake/Coverage.cmake)

set(VERSION_H ${CMAKE_CURRENT_BINARY_DIR}/include/dlsm/Version.h)
configure_file(include/dlsm/Version.h.in ${VERSION_H} @ONLY)

set(PUBLIC_HDRS
    ${VERSION_H}
)

file(GLOB_RECURSE INTERNAL_HDRS
    ${PROJECT_SOURCE_DIR}/include/impl/*.h
)

add_subdirectory(fbs)


add_library               (dlsm STATIC  ${INTERNAL_HDRS} ${PUBLIC_HDRS})
target_precompile_headers (dlsm PRIVATE ${INTERNAL_HDRS} ${PUBLIC_HDRS})
target_include_directories(dlsm PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include>"
)
set_target_properties     (dlsm PROPERTIES PUBLIC_HEADER "${PUBLIC_HDRS}")
target_link_libraries     (dlsm
    PUBLIC
        Options
        $<TARGET_NAME_IF_EXISTS:Coverage>
        fbs
    PRIVATE
        CONAN_PKG::zmqpp
        CONAN_PKG::zeromq
)

add_subdirectory(src)

add_executable            (main src/main.cpp)
target_link_libraries     (main PRIVATE dlsm)

install(TARGETS main dlsm #Options fbs
#        EXPORT  dlsmTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dlsm)

#install(EXPORT dlsmTargets DESTINATION cmake
#    NAMESPACE DLSM:: FILE DLSM.cmake)

include(CTest)
if(BUILD_TESTING)
add_subdirectory(tests/perf)
add_subdirectory(tests/unit)
endif()

include(CPack)