add_executable(example_addrtest addrtest.cpp)


target_link_libraries (example_addrtest system ${BOOST_CHRONO_LIB} ${SYS_BASIC_LIBS})

add_executable(example_udptest udptest.cpp)


target_link_libraries (example_udptest system ${BOOST_CHRONO_LIB} ${SYS_BASIC_LIBS})
