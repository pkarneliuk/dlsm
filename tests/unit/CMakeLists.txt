find_package(GTest REQUIRED COMPONENTS gtest gmock)

add_executable            (unit Unit.cpp)
target_precompile_headers (unit PRIVATE Unit.hpp)
target_include_directories(unit PRIVATE ../)
target_link_libraries     (unit PRIVATE dlsm GTest::gtest GTest::gmock)
target_sources            (unit PRIVATE
    Asan.cpp
    Tsan.cpp
    Usan.cpp
    Flat.cpp
    TestAllocator.cpp
    TestClock.cpp
    TestDisruptor.cpp
    TestDisruptorGraph.cpp
    TestLock.cpp
    TestLogger.cpp
    TestMemory.cpp
    TestSharedMemory.cpp
    TestSignal.cpp
    TestStr.cpp
    TestThread.cpp
    TestTransport.cpp
    TestVersion.cpp
)

add_test(NAME unit COMMAND $<TARGET_FILE:unit>)
set_tests_properties(unit PROPERTIES TIMEOUT 10)
