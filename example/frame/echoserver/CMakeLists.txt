add_executable (example_echoserver echoserver.cpp)

#set_source_files_properties(echoserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH="\\\"${EXTERN_PATH}\\\"")

target_link_libraries (example_echoserver
	solid_frame
	solid_frame_aio
	solid_serialization
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
#	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/echo-ca-cert.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-key.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem
)

add_custom_target(example_secure
	DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
)

add_executable (example_secure_echoserver secureechoserver.cpp)

#set_source_files_properties(echoserver.cpp COMPILE_FLAGS -DOSSL_SOURCE_PATH="\\\"${EXTERN_PATH}\\\"")

target_link_libraries (example_secure_echoserver
	solid_frame
	solid_frame_aio
	solid_frame_aio_openssl
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)


