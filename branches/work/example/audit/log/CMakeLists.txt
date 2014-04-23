add_executable(example_log_client client.cpp)
add_executable(example_log_server server.cpp)

target_link_libraries (example_log_client solid_audit_log solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_log_server solid_audit_log_manager solid_audit_log solid_utility solid_system ${SYS_BASIC_LIBS})
