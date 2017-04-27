if(${Boost_FOUND} AND OPENSSL_FOUND)
    add_executable (example_relay_server example_relay_server.cpp)

    add_dependencies(example_relay_server build_boost)

    target_link_libraries (example_relay_server
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYS_BASIC_LIBS}
        ${SYS_DYNAMIC_LOAD_LIB}
    )
    
    add_executable (example_relay_server_bi example_relay_server_bi.cpp)

    add_dependencies(example_relay_server_bi build_boost)

    target_link_libraries (example_relay_server_bi
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYS_BASIC_LIBS}
        ${SYS_DYNAMIC_LOAD_LIB}
    )
    
    add_executable (example_relay_server_bi_cp example_relay_server_bi_cp.cpp)

    add_dependencies(example_relay_server_bi_cp build_boost)

    target_link_libraries (example_relay_server_bi_cp
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYS_BASIC_LIBS}
        ${SYS_DYNAMIC_LOAD_LIB}
    )
    
endif()


