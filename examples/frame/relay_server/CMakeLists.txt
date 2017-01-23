if(${Boost_FOUND} AND OPENSSL_FOUND)
    add_executable (example_relay_server example_relay_server.cpp)
    
    add_dependencies(example_relay_server build_boost build_openssl)

    target_link_libraries (example_relay_server
        solid_frame
        solid_frame_aio
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYS_BASIC_LIBS}
        ${SYS_DYNAMIC_LOAD_LIB}
    )
endif()


