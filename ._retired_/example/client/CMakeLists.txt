add_executable (example_alphaclient alphaclient.cpp writer.cpp)
add_executable (example_alphafetch_p alphafetch_p.cpp writer.cpp)
add_executable (example_alphastore_p alphastore_p.cpp writer.cpp)
add_executable (example_imapappender imapappender.cpp base64stream.cpp writer.cpp)

set_source_files_properties(imapappender.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH="\\\"${EXTERN_PATH}\\\"")

set_source_files_properties(
	alphaclient.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH="\\\"${EXTERN_PATH}\\\""
)


target_link_libraries (example_alphaclient
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

target_link_libraries (example_alphafetch_p
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

target_link_libraries (example_alphastore_p
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

target_link_libraries (example_imapappender
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_FILESYSTEM_LIB}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
