find_package(benchmark REQUIRED benchmark)

add_executable            (perf Perf.cpp)
target_precompile_headers (perf PRIVATE Perf.hpp Timestamps.hpp)
target_include_directories(perf PRIVATE ../)
target_link_libraries     (perf PRIVATE dlsm benchmark::benchmark)
target_sources            (perf PRIVATE
    PerfClock.cpp
    PerfDisruptor.cpp
    PerfLock.cpp
    PerfTransport.cpp
)

add_test(NAME perf COMMAND $<TARGET_FILE:perf>
    #--benchmark_repetitions=1
    --benchmark_min_time=0.01s
    --benchmark_counters_tabular=true
)
