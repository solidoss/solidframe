#==============================================================================

if(OPENSSL_FOUND)
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

    add_custom_target(aio_test_copy_certs
        DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
        ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    )
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
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestAioEchoTcpStress1        COMMAND  test_aio_echo test_echo_tcp_stress 1)
    add_test(NAME TestAioEchoTcpStress2        COMMAND  test_aio_echo test_echo_tcp_stress 2)
    add_test(NAME TestAioEchoTcpStress4        COMMAND  test_aio_echo test_echo_tcp_stress 4)
    add_test(NAME TestAioEchoTcpStress8        COMMAND  test_aio_echo test_echo_tcp_stress 8)
    add_test(NAME TestAioEchoTcpStress16       COMMAND  test_aio_echo test_echo_tcp_stress 16)

    add_test(NAME TestAioEchoTcpStress1r       COMMAND  test_aio_echo test_echo_tcp_stress 1 r)
    add_test(NAME TestAioEchoTcpStress2r       COMMAND  test_aio_echo test_echo_tcp_stress 2 r)
    add_test(NAME TestAioEchoTcpStress4r       COMMAND  test_aio_echo test_echo_tcp_stress 4 r)
    add_test(NAME TestAioEchoTcpStress8r       COMMAND  test_aio_echo test_echo_tcp_stress 8 r)
    add_test(NAME TestAioEchoTcpStress16r      COMMAND  test_aio_echo test_echo_tcp_stress 16 r)

    add_test(NAME TestAioEchoTcpStress1s       COMMAND  test_aio_echo test_echo_tcp_stress 1 s)
    add_test(NAME TestAioEchoTcpStress2s       COMMAND  test_aio_echo test_echo_tcp_stress 2 s)
    add_test(NAME TestAioEchoTcpStress4s       COMMAND  test_aio_echo test_echo_tcp_stress 4 s)
    add_test(NAME TestAioEchoTcpStress8s       COMMAND  test_aio_echo test_echo_tcp_stress 8 s)
    add_test(NAME TestAioEchoTcpStress16s      COMMAND  test_aio_echo test_echo_tcp_stress 16 s)

    add_test(NAME TestAioEchoTcpStress1rs      COMMAND  test_aio_echo test_echo_tcp_stress 1 r s)
    add_test(NAME TestAioEchoTcpStress2rs      COMMAND  test_aio_echo test_echo_tcp_stress 2 r s)
    add_test(NAME TestAioEchoTcpStress4rs      COMMAND  test_aio_echo test_echo_tcp_stress 4 r s)
    add_test(NAME TestAioEchoTcpStress8rs      COMMAND  test_aio_echo test_echo_tcp_stress 8 r s)
    add_test(NAME TestAioEchoTcpStress16rs     COMMAND  test_aio_echo test_echo_tcp_stress 16 r s)

    #==============================================================================
endif(OPENSSL_FOUND)

