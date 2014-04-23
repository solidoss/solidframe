
add_executable (example_bitcount bitcount.cpp)

target_link_libraries (example_bitcount solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_crcvalue crcvalue.cpp)

target_link_libraries (example_crcvalue solid_utility solid_system ${SYS_BASIC_LIBS})
