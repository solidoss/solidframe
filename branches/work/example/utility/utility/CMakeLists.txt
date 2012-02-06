
add_executable (example_bitcount bitcount.cpp)

target_link_libraries (example_bitcount utility system ${SYS_BASIC_LIBS})

add_executable (example_crcvalue crcvalue.cpp)

target_link_libraries (example_crcvalue utility system ${SYS_BASIC_LIBS})
