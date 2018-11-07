if(${Boost_FOUND})

add_executable (tutorial_mprpc_file_server mprpc_file_server.cpp mprpc_file_messages.hpp)

add_dependencies(tutorial_mprpc_file_server build-boost)

target_link_libraries (tutorial_mprpc_file_server
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_SYSTEM_LIBRARY}
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)

add_executable (tutorial_mprpc_file_client mprpc_file_client.cpp mprpc_file_messages.hpp)

target_link_libraries (tutorial_mprpc_file_client
    solid_frame_mprpc
    solid_frame_aio
    solid_frame
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
    ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
)

endif()
