add_executable (tutorial_ipc_file_server ipc_file_server.cpp ipc_file_messages.hpp)

target_link_libraries (tutorial_ipc_file_server
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_ipc_file_client ipc_file_client.cpp ipc_file_messages.hpp)

target_link_libraries (tutorial_ipc_file_client
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)


