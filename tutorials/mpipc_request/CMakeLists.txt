add_executable (tutorial_mpipc_request_server mpipc_request_server.cpp mpipc_request_messages.hpp)

target_link_libraries (tutorial_mpipc_request_server
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_request_client mpipc_request_client.cpp mpipc_request_messages.hpp)

target_link_libraries (tutorial_mpipc_request_client
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)


