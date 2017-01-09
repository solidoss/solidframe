add_executable (example_workpool example_workpool.cpp)
target_link_libraries (example_workpool solid_utility solid_system ${SYS_BASIC_LIBS})

if(${Boost_FOUND})
    add_executable (example_file_open_pool example_file_open_pool.cpp)
    target_link_libraries (example_file_open_pool solid_utility solid_system ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${SYS_BASIC_LIBS})
endif()
