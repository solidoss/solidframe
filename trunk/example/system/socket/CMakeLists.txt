add_executable(addrtest addrtest.cpp)


target_link_libraries (addrtest system ${SYS_BASIC_LIBS})

add_executable(udptest udptest.cpp)


target_link_libraries (udptest system ${SYS_BASIC_LIBS})
