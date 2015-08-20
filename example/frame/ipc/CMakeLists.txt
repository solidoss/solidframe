add_executable (example_ipc_echo_stress ipcechostress.cpp)

target_link_libraries (example_ipc_echo_stress
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_core
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

add_executable (example_ipc_echo ipcecho.cpp)

target_link_libraries (example_ipc_echo
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



