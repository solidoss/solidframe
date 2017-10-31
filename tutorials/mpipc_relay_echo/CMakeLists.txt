add_executable (tutorial_mpipc_relay_echo_client mpipc_relay_echo_client.cpp)

target_link_libraries (tutorial_mpipc_relay_echo_client
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_relay_echo_relay mpipc_relay_echo_relay.cpp)

target_link_libraries (tutorial_mpipc_relay_echo_relay
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)
