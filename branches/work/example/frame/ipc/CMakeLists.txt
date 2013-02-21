add_executable (example_ipcstress ipcstress.cpp)

target_link_libraries (example_ipcstress
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_core
	algorithm_serialization
	utility
	system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)


add_executable (example_ipcserialization ipcserialization.cpp)

target_link_libraries (example_ipcserialization
	solid_frame_core
	algorithm_serialization
	utility
	system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (example_ipcrelay ipcrelay.cpp)

target_link_libraries (example_ipcrelay
	solid_frame_ipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_core
	algorithm_serialization
	utility
	system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)



