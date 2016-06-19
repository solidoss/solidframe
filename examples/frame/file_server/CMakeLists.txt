add_executable (example_file_server example_file_server.cpp)

target_link_libraries (example_file_server
	solid_frame_aio
	solid_frame_file
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (example_filebuf example_filebuf.cpp)
target_link_libraries (example_filebuf
	solid_frame_file
	solid_frame
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)


