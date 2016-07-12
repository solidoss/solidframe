add_executable (tutorial_ipc_echo_server ipc_echo_server.cpp ipc_echo_messages.hpp)

target_link_libraries (tutorial_ipc_echo_server
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_ipc_echo_client ipc_echo_client.cpp ipc_echo_messages.hpp)

target_link_libraries (tutorial_ipc_echo_client
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

