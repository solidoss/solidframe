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

add_custom_target(tutorial_mpipc_request_ssl_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(OPENSSL_FOUND)

    add_executable (tutorial_mpipc_request_ssl_server mpipc_request_server.cpp mpipc_request_messages.hpp)

    add_dependencies(tutorial_mpipc_request_ssl_server tutorial_mpipc_request_ssl_certs build-openssl build-snappy)

    target_link_libraries (tutorial_mpipc_request_ssl_server
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v2
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${OPENSSL_LIBRARIES}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )

    add_executable (tutorial_mpipc_request_ssl_client mpipc_request_client.cpp mpipc_request_messages.hpp)

    add_dependencies(tutorial_mpipc_request_ssl_client tutorial_mpipc_request_ssl_certs build-openssl build-snappy)

    target_link_libraries (tutorial_mpipc_request_ssl_client
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v2
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${OPENSSL_LIBRARIES}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )
endif(OPENSSL_FOUND)

