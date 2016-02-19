
add_executable (example_bitcount bitcount.cpp)

target_link_libraries (example_bitcount solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_maxbitcount maxbitcount.cpp)

target_link_libraries (example_maxbitcount solid_utility solid_system ${SYS_BASIC_LIBS})
