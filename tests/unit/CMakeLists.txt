add_executable            (unit Unit.cpp)
target_precompile_headers (unit PRIVATE Unit.hpp)
target_include_directories(unit PRIVATE ../)
target_link_libraries     (unit PRIVATE dlsm CONAN_PKG::gtest)
target_sources            (unit PRIVATE
    Asan.cpp
    Tsan.cpp
    Usan.cpp
    Flat.cpp
    TestAllocator.cpp
    TestLogger.cpp
    TestSharedMemory.cpp
    TestStr.cpp
    TestThread.cpp
    TestTransport.cpp
    TestVersion.cpp
)

add_test(NAME unit COMMAND $<TARGET_FILE:unit>)
set_tests_properties(unit PROPERTIES TIMEOUT 10)
