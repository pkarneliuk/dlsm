cmake_minimum_required(VERSION 3.29)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

cmake_policy(SET CMP0091 ${CMAKE_POLICY_DEFAULT_CMP0091})
unset(CMAKE_TOOLCHAIN_FILE) # CMake Warning: Manually-specified variables were not used by the project

project(DLSM VERSION ${CONAN_PACKAGE_VERSION} LANGUAGES CXX)

include(GNUInstallDirs)
include(cmake/Ccache.cmake)
include(cmake/Coverage.cmake)
include(cmake/Doxygen.cmake)
include(cmake/Linters.cmake)
include(cmake/Options.cmake)
include(cmake/Version.cmake)
include(cmake/Package.cmake)

configure_version(VERSION_HPP include/dlsm/Version.hpp.in)

add_subdirectory(fbs)
add_subdirectory(src)

add_executable            (main src/main.cpp)
target_link_libraries     (main PRIVATE dlsmRuntime)

install(TARGETS main dlsmRuntime Options
        EXPORT  DLSM-Development
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/dlsm)

install(EXPORT DLSM-Development
            DESTINATION cmake
            FILE DLSM-Development.cmake
            NAMESPACE DLSM::
            COMPONENT DLSM-Development)

include(CTest)
if(BUILD_TESTING)
set(CMAKE_CTEST_ARGUMENTS -V) # Enable verbose output on test target
add_subdirectory(tests/apps)
add_subdirectory(tests/perf)
add_subdirectory(tests/unit)
endif()

include(CPack)