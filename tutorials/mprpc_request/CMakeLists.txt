add_executable (tutorial_mprpc_request_server mprpc_request_server.cpp mprpc_request_messages.hpp)

target_link_libraries (tutorial_mprpc_request_server
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_executable (tutorial_mprpc_request_client mprpc_request_client.cpp mprpc_request_messages.hpp)

target_link_libraries (tutorial_mprpc_request_client
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)


