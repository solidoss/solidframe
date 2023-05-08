add_executable (example_threadpool example_threadpool.cpp)
target_link_libraries (example_threadpool solid_utility solid_system ${SYSTEM_BASIC_LIBRARIES})

add_executable (example_file_open_pool example_file_open_pool.cpp)

target_link_libraries (example_file_open_pool solid_utility solid_system ${SYSTEM_BASIC_LIBRARIES})
