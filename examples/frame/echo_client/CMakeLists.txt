add_executable (example_secure_echo_client example_secure_echo_client.cpp)

target_link_libraries (example_secure_echo_client
	solid_frame
	solid_frame_aio
	solid_frame_aio_openssl
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

