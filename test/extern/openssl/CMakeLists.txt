add_executable (echo_client echo_client.cpp)

target_link_libraries (echo_client ssl crypto utility system pthread rt dl)
