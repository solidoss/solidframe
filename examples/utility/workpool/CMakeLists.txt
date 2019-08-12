add_executable (example_workpool example_workpool.cpp)
target_link_libraries (example_workpool solid_utility solid_system ${SYSTEM_BASIC_LIBRARIES})

add_executable (example_file_open_pool example_file_open_pool.cpp)

target_link_libraries (example_file_open_pool solid_utility solid_system ${SYSTEM_BASIC_LIBRARIES})
