add_executable (example_echo_server example_echo_server.cpp)

target_link_libraries (example_echo_server
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

# openssl genrsa 2048 > ca-key.pem
# openssl req -new -x509 -nodes -days 1000 -key ca-key.pem > ca-cert.pem
# openssl req -newkey rsa:2048 -days 1000 -nodes -keyout server-key.pem > server-req.pem
# openssl x509 -req -in server-req.pem -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > server-cert.pem
# openssl req -newkey rsa:2048 -days 1000 -nodes -keyout client-key.pem > client-req.pem
# openssl x509 -req -in client-req.pem -days 1000 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > client-cert.pem

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

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/echo-client-key.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-client-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/echo-client-cert.pem
)

add_custom_target(example_secure_certs
	DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

add_executable (example_secure_echo_server example_secure_echo_server.cpp)
add_dependencies(example_secure_echo_server example_secure_certs)

target_link_libraries (example_secure_echo_server
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

add_executable (example_secure_echo_client example_secure_echo_client.cpp)
add_dependencies(example_secure_echo_client example_secure_certs)

target_link_libraries (example_secure_echo_client
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



