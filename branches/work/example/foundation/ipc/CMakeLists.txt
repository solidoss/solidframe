add_executable (example_ipcstress ipcstress.cpp)

target_link_libraries (example_ipcstress
	foundation_ipc
	foundation_aio
	foundation_aio_openssl
	foundation_core
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
	foundation_core
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
	foundation_ipc
	foundation_aio
	foundation_aio_openssl
	foundation_core
	algorithm_serialization
	utility
	system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)



