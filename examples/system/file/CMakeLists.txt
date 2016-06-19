add_executable (example_file_open example_file_open.cpp)

target_link_libraries (example_file_open solid_system boost_filesystem boost_system ${SYS_BASIC_LIBS})
