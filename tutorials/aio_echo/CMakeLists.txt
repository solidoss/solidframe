add_executable (tutorial_aio_echo_server aio_echo_server.cpp)

target_link_libraries (tutorial_aio_echo_server
    solid_frame_aio
    solid_frame
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

