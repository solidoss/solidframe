add_executable (example_echoserver echoserver.cpp)

set_source_files_properties(echoserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')

target_link_libraries (example_echoserver
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



