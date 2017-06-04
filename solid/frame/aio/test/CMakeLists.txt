#==============================================================================

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-ca-cert.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-server-key.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-server-cert.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-client-key.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-client-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-client-cert.pem
)

add_custom_target(aio_test_copy_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(OPENSSL_FOUND)

    #==============================================================================

    set( aioEchoTestSuite
        test_echo_tcp_stress.cpp
    )
    #
    create_test_sourcelist( aioEchoTests test_aio_echo.cpp ${aioEchoTestSuite})

    add_executable(test_aio_echo ${aioEchoTests})
    add_dependencies(test_aio_echo aio_test_copy_certs build_openssl)

    target_link_libraries(test_aio_echo
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        snappy
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestAioEchoTcpStress1        COMMAND  test_aio_echo test_aio_tcp_stress 1)
    add_test(NAME TestAioEchoTcpStress2        COMMAND  test_aio_echo test_aio_tcp_stress 2)
    add_test(NAME TestAioEchoTcpStress4        COMMAND  test_aio_echo test_aio_tcp_stress 4)
    add_test(NAME TestAioEchoTcpStress8        COMMAND  test_aio_echo test_aio_tcp_stress 8)

    add_test(NAME TestAioEchoTcpStress1s       COMMAND  test_aio_echo test_aio_tcp_stress 1 s)
    add_test(NAME TestAioEchoTcpStress2s       COMMAND  test_aio_echo test_aio_tcp_stress 2 s)
    add_test(NAME TestAioEchoTcpStress4s       COMMAND  test_aio_echo test_aio_tcp_stress 4 s)
    add_test(NAME TestAioEchoTcpStress8s       COMMAND  test_aio_echo test_aio_tcp_stress 8 s)

    #==============================================================================
endif(OPENSSL_FOUND)

