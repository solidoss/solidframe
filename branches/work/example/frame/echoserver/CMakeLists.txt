add_executable (example_echoserver echoserver.cpp)

#set_source_files_properties(echoserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH="\\\"${EXTERN_PATH}\\\"")

target_link_libraries (example_echoserver
	solid_frame
	solid_frame_aio
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)



