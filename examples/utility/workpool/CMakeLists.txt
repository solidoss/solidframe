add_executable (example_file_open_pool file_open_pool.cpp)
add_executable (example_workpool testa.cpp)

target_link_libraries (example_file_open_pool solid_utility solid_system boost_filesystem boost_system ${SYS_BASIC_LIBS})
target_link_libraries (example_workpool solid_utility solid_system ${SYS_BASIC_LIBS})
