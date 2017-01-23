if(${Boost_FOUND})
    add_executable (example_file_open example_file_open.cpp)
    
    add_dependencies(example_file_open build_boost)
    
    target_link_libraries (example_file_open solid_system ${Boost_FILESYSTEM_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${SYS_BASIC_LIBS})
endif()
