add_executable(example_log_client client.cpp)
add_executable(example_log_server server.cpp)

target_link_libraries (example_log_client audit_log utility system ${SYS_BASIC_LIBS})
target_link_libraries (example_log_server audit_log_manager audit_log utility system ${SYS_BASIC_LIBS})
