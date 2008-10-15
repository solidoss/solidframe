include_directories(${SOLIDGROUND_SOURCE_DIR})

add_executable(client client.cpp)
add_executable(server server.cpp)

target_link_libraries (client audit_log utility system pthread rt)
target_link_libraries (server audit_log_manager audit_log utility system pthread rt)