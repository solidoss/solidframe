add_executable (example_alphaclient_p alphaclient_p.cpp writer.cpp)
add_executable (example_alphaclient_s alphaclient_s.cpp writer.cpp)
add_executable (example_alphafetch_p alphafetch_p.cpp writer.cpp)
add_executable (example_alphastore_p alphastore_p.cpp writer.cpp)
add_executable (example_imapappender imapappender.cpp base64stream.cpp writer.cpp)
set_source_files_properties(imapappender.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')
set_source_files_properties(alphaclient_s.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH='"${EXTERN_PATH}"')
target_link_libraries (example_alphaclient_p utility system ${OPENSSL_LIBS} ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
target_link_libraries (example_alphaclient_s utility system ${OPENSSL_LIBS} ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
target_link_libraries (example_alphafetch_p utility system ${OPENSSL_LIBS}  ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
target_link_libraries (example_alphastore_p utility system ${OPENSSL_LIBS}  ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})
target_link_libraries (example_imapappender
	utility
	system
	${OPENSSL_LIBS}
	${BOOST_FILESYSTEM_LIB}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
