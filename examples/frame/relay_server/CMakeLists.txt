if(OPENSSL_FOUND)
    add_executable (example_relay_server example_relay_server.cpp)
    
    add_dependencies(example_relay_server build-cxxopts)
    
    target_link_libraries (example_relay_server
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_executable (example_relay_server_bi example_relay_server_bi.cpp)
    
    add_dependencies(example_relay_server_bi build-cxxopts)
    
    target_link_libraries (example_relay_server_bi
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_executable (example_relay_server_bi_cp example_relay_server_bi_cp.cpp)
    
    add_dependencies(example_relay_server_bi_cp build-cxxopts)

    target_link_libraries (example_relay_server_bi_cp
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_executable (example_relay_server_bi_sh example_relay_server_bi_sh.cpp)
    
    add_dependencies(example_relay_server_bi_sh build-cxxopts)

    target_link_libraries (example_relay_server_bi_sh
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_executable (example_relay_server_bi_ex example_relay_server_bi_ex.cpp)
    
    add_dependencies(example_relay_server_bi_ex build-cxxopts)

    target_link_libraries (example_relay_server_bi_ex
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

endif()


