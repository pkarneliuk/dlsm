find_package(benchmark REQUIRED benchmark)

add_executable            (broadcast Broadcast.cpp)
target_include_directories(broadcast PRIVATE ../)
target_link_libraries     (broadcast PRIVATE dlsm)
