add_executable (tutorial_mprpc_echo_server mprpc_echo_server.cpp mprpc_echo_messages.hpp)

target_link_libraries (tutorial_mprpc_echo_server
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_executable (tutorial_mprpc_echo_client mprpc_echo_client.cpp mprpc_echo_messages.hpp)

target_link_libraries (tutorial_mprpc_echo_client
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)


add_executable (tutorial_mprpc_echo_client_pool mprpc_echo_client_pool.cpp mprpc_echo_messages.hpp)

target_link_libraries (tutorial_mprpc_echo_client_pool
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)
