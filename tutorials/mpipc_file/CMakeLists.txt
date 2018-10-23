if(${Boost_FOUND})

add_executable (tutorial_mpipc_file_server mpipc_file_server.cpp mpipc_file_messages.hpp)

add_dependencies(tutorial_mpipc_file_server build_boost)

target_link_libraries (tutorial_mpipc_file_server
    solid_frame_mpipc
    solid_frame_aio
    solid_frame_aio_openssl
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)

add_executable (tutorial_mpipc_file_client mpipc_file_client.cpp mpipc_file_messages.hpp)

target_link_libraries (tutorial_mpipc_file_client
    solid_frame_mpipc
    solid_frame_aio
    solid_frame_aio_openssl
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)

endif()
