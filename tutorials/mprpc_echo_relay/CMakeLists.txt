add_executable (tutorial_mprpc_echo_relay_client mprpc_echo_relay_client.cpp)

target_link_libraries (tutorial_mprpc_echo_relay_client
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)

add_executable (tutorial_mprpc_echo_relay_server mprpc_echo_relay_server.cpp)

target_link_libraries (tutorial_mprpc_echo_relay_server
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)
