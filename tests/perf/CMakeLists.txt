find_package(benchmark REQUIRED)

add_executable            (perf Perf.cpp)
target_precompile_headers (perf PRIVATE Perf.hpp)
target_include_directories(perf PRIVATE ../)
target_link_libraries     (perf benchmark::benchmark)

add_test(NAME perf COMMAND $<TARGET_FILE:perf>)
