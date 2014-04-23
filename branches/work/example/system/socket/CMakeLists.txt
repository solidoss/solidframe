add_executable(example_addrtest addrtest.cpp)


target_link_libraries (example_addrtest solid_system  ${SYS_BASIC_LIBS})

add_executable(example_udptest udptest.cpp)


target_link_libraries (example_udptest solid_system ${SYS_BASIC_LIBS})
