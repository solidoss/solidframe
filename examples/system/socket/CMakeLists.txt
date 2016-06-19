add_executable(example_socket_address example_socket_address.cpp)


target_link_libraries (example_socket_address solid_system  ${SYS_BASIC_LIBS})

add_executable(example_socket_udp example_socket_udp.cpp)


target_link_libraries (example_socket_udp solid_system ${SYS_BASIC_LIBS})
