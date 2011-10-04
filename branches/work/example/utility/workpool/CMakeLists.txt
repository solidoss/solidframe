add_executable (file_open_pool file_open_pool.cpp)
add_executable (testa testa.cpp)

target_link_libraries (file_open_pool utility system boost_filesystem boost_system ${SYS_BASIC_LIBS})
target_link_libraries (testa utility system ${SYS_BASIC_LIBS})
