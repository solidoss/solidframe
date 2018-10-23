add_executable (example_workpool example_workpool.cpp)
target_link_libraries (example_workpool solid_utility solid_system ${SYSTEM_BASIC_LIBRARIES})

if(${Boost_FOUND})
    add_executable (example_file_open_pool example_file_open_pool.cpp)

    add_dependencies(example_file_open_pool build_boost)

    target_link_libraries (example_file_open_pool solid_utility solid_system ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${SYSTEM_BASIC_LIBRARIES})
endif()
