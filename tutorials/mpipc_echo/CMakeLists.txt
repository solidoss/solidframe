add_executable (tutorial_mpipc_echo_server mpipc_echo_server.cpp mpipc_echo_messages.hpp)

target_link_libraries (tutorial_mpipc_echo_server
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_echo_client mpipc_echo_client.cpp mpipc_echo_messages.hpp)

target_link_libraries (tutorial_mpipc_echo_client
    solid_frame_mpipc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)

