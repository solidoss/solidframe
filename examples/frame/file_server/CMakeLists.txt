if(OPENSSL_FOUND)
    add_executable (example_file_server example_file_server.cpp)

    add_dependencies(example_file_server build-openssl build-cxxopts)

    target_link_libraries (example_file_server
        solid_frame_aio
        solid_frame_file
        solid_frame
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )
endif()

add_executable (example_filebuf example_filebuf.cpp)
target_link_libraries (example_filebuf
    solid_frame_file
    solid_frame
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)


