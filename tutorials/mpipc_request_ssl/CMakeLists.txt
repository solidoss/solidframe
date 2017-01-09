add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    #   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
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

    add_custom_target(tutorial_mpipc_request_ssl_certs
        DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    )



add_executable (tutorial_mpipc_request_ssl_server mpipc_request_server.cpp mpipc_request_messages.hpp)

add_dependencies(tutorial_mpipc_request_ssl_server tutorial_mpipc_request_ssl_certs)

target_link_libraries (tutorial_mpipc_request_ssl_server
    solid_frame_mpipc
    solid_frame_aio
    solid_frame_aio_openssl
    solid_frame
    solid_serialization
    solid_utility
    solid_system
    snappy
    ${OPENSSL_LIBRARIES}
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (tutorial_mpipc_request_ssl_client mpipc_request_client.cpp mpipc_request_messages.hpp)
add_dependencies(tutorial_mpipc_request_ssl_client tutorial_mpipc_request_ssl_certs)

target_link_libraries (tutorial_mpipc_request_ssl_client
    solid_frame_mpipc
    solid_frame_aio
    solid_frame_aio_openssl
    solid_frame
    solid_serialization
    solid_utility
    solid_system
    snappy
    ${OPENSSL_LIBRARIES}
    ${SYS_BASIC_LIBS}
    ${SYS_DYNAMIC_LOAD_LIB}
)


