add_executable            (unit Unit.cpp)
target_precompile_headers (unit PRIVATE Unit.h)
target_include_directories(unit PRIVATE ../)
target_link_libraries     (unit PRIVATE dlsm CONAN_PKG::gtest)
target_sources            (unit PRIVATE
    Asan.cpp
    Tsan.cpp
    Usan.cpp
    Flat.cpp
    Test.cpp
    TestStr.cpp
    TestTransport.cpp
)

add_test(NAME unit COMMAND $<TARGET_FILE:unit>)
