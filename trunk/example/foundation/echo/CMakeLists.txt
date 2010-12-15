add_executable (echoserver echoserver.cpp)

set_source_files_properties(echoserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')

target_link_libraries (echoserver
	foundation_aio
	foundation_aio_openssl
	foundation_core
	algorithm_serialization
	utility
	system
	ssl
	crypto
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
	dl
)



