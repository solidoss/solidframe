add_executable(log_client client.cpp)
add_executable(log_server server.cpp)

target_link_libraries (log_client audit_log utility system ${SYS_BASIC_LIBS})
target_link_libraries (log_server audit_log_manager audit_log utility system ${SYS_BASIC_LIBS})
