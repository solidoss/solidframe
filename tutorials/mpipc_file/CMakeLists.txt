add_executable (tutorial_mpipc_file_server mpipc_file_server.cpp mpipc_file_messages.hpp)

target_link_libraries (tutorial_mpipc_file_server
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${BOOST_FILESYSTEM_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_file_client mpipc_file_client.cpp mpipc_file_messages.hpp)

target_link_libraries (tutorial_mpipc_file_client
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


