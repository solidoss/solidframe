add_executable (example_fileserver fileserver.cpp)

target_link_libraries (example_fileserver
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame_file
	solid_frame_core
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (example_filebuf filebuf.cpp)
target_link_libraries (example_filebuf
	solid_frame_file
	solid_frame_core
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)


