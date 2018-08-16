add_executable (tutorial_mpipc_echo_relay_client mpipc_echo_relay_client.cpp)

target_link_libraries (tutorial_mpipc_echo_relay_client
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_echo_relay_server mpipc_echo_relay_server.cpp)

target_link_libraries (tutorial_mpipc_echo_relay_server
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)
