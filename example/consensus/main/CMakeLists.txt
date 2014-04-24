add_executable (example_multi_launcher src/multilauncher.cpp)

target_link_libraries (example_multi_launcher
	solid_system
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
)

add_executable (example_consensus_client src/consensusclient.cpp)

target_link_libraries (example_consensus_client
	example_consensus_clientobject
	example_consensus_core
	solid_consensus_core
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_core
	solid_serialization
	solid_utility
	solid_system
	boost_program_options
	boost_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_executable (example_consensus_server src/consensusserver.cpp)

target_link_libraries (example_consensus_server
	example_consensus_serverobject
	example_consensus_core
	solid_consensus_server
	solid_consensus_core
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_core
	solid_serialization
	solid_utility
	solid_system
	boost_program_options
	boost_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)





