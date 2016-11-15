#add_executable (example_ipc_echo_stress ipcechostress.cpp)

# target_link_libraries (example_ipc_echo_stress
# 	solid_frame_ipc
# 	solid_frame_aio
# 	solid_frame_aio_openssl
# 	solid_frame_core
# 	solid_serialization
# 	solid_utility
# 	solid_system
# 	${OPENSSL_LIBS}
# 	${BOOST_PROGRAM_OPTIONS_LIB}
# 	${BOOST_SYSTEM_LIB}
# 	${OPENSSL_LIBS}
# 	${SYS_BASIC_LIBS}
# 	${SYS_DYNAMIC_LOAD_LIB}
# )


add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
#	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-ca-cert.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-server-key.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-server-cert.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-client-key.pem
)

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
	COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-client-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../aio_echo/echo-client-cert.pem
)

add_custom_target(example_mpipc_echo_copy_certs
	DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
	${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(${Boost_FOUND} AND OPENSSL_FOUND)
	add_executable (example_mpipc_echo example_mpipc_echo.cpp)
	add_dependencies(example_mpipc_echo example_mpipc_echo_copy_certs)

	target_link_libraries (example_mpipc_echo
		solid_frame_mpipc
		solid_frame_aio
		solid_frame_aio_openssl
		solid_frame
		solid_serialization
		solid_utility
		solid_system
		${OPENSSL_LIBRARIES}
		${Boost_PROGRAM_OPTIONS_LIBRARY}
		${Boost_SYSTEM_LIBRARY}
		${SYS_BASIC_LIBS}
		${SYS_DYNAMIC_LOAD_LIB}
	)
endif()


