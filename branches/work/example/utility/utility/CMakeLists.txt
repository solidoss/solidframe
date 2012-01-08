
add_executable (bitcount bitcount.cpp)

target_link_libraries (bitcount utility system ${SYS_BASIC_LIBS})

add_executable (crcvalue crcvalue.cpp)

target_link_libraries (crcvalue utility system ${SYS_BASIC_LIBS})
