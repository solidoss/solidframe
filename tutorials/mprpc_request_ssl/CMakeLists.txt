#if(SOLID_ON_WINDOWS)
#    set(SUFFIX "${CMAKE_BUILD_TYPE}")
#endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    DEPENDS ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-ca-cert.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-key.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-cert.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-key.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-cert.pem
)

add_custom_target(tutorial_mprpc_request_ssl_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(OPENSSL_FOUND)

    add_executable (tutorial_mprpc_request_ssl_server mprpc_request_server.cpp mprpc_request_messages.hpp)

    add_dependencies(tutorial_mprpc_request_ssl_server tutorial_mprpc_request_ssl_certs build-openssl build-snappy)

    target_link_libraries (tutorial_mprpc_request_ssl_server
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v2
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_executable (tutorial_mprpc_request_ssl_client mprpc_request_client.cpp mprpc_request_messages.hpp)

    add_dependencies(tutorial_mprpc_request_ssl_client tutorial_mprpc_request_ssl_certs build-openssl build-snappy)

    target_link_libraries (tutorial_mprpc_request_ssl_client
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v2
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )
endif(OPENSSL_FOUND)

