add_executable (example_file_create create.cpp)
add_executable (example_file_open open.cpp)

target_link_libraries (example_file_create solid_system boost_filesystem boost_system ${SYS_BASIC_LIBS})
target_link_libraries (example_file_open solid_system boost_filesystem boost_system ${SYS_BASIC_LIBS})
