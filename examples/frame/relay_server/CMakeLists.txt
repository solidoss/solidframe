if(OPENSSL_FOUND)
    add_executable (example_relay_server example_relay_server.cpp)

    target_link_libraries (example_relay_server
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

    add_executable (example_relay_server_bi example_relay_server_bi.cpp)

    target_link_libraries (example_relay_server_bi
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

    add_executable (example_relay_server_bi_cp example_relay_server_bi_cp.cpp)

    target_link_libraries (example_relay_server_bi_cp
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

    add_executable (example_relay_server_bi_sh example_relay_server_bi_sh.cpp)

    target_link_libraries (example_relay_server_bi_sh
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

    add_executable (example_relay_server_bi_ex example_relay_server_bi_ex.cpp)

    target_link_libraries (example_relay_server_bi_ex
        solid_frame
        solid_frame_aio
        solid_utility
        solid_system
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

endif()


