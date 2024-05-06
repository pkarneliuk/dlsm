
add_executable            (broadcast Broadcast.cpp)
target_link_libraries     (broadcast PRIVATE dlsm)

add_executable            (heartbeat Heartbeat.cpp)
target_link_libraries     (heartbeat PRIVATE dlsm)

add_executable            (command Command.cpp)
target_link_libraries     (command PRIVATE dlsm)
