#==============================================================================

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/certs/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
#   COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
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

add_custom_target(mpipc_test_copy_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(OPENSSL_FOUND)

    #==============================================================================
    set( mpipcProtocolTestSuite
        test_protocol_basic.cpp
        test_protocol_synchronous.cpp
        test_protocol_cancel.cpp
    )

    create_test_sourcelist( mpipcProtocolTests test_mpipc_protocol.cpp ${mpipcProtocolTestSuite})

    add_executable(test_mpipc_protocol ${mpipcProtocolTests})

    add_dependencies(test_mpipc_protocol build_openssl)

    target_link_libraries(test_mpipc_protocol
        solid_frame_mpipc
        solid_frame_aio
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestProtocolBasic     COMMAND  test_mpipc_protocol test_protocol_basic)
    add_test(NAME TestProtocolCancel    COMMAND  test_mpipc_protocol test_protocol_cancel)
    add_test(NAME TestProtocolSynch     COMMAND  test_mpipc_protocol test_protocol_synchronous)

    #==============================================================================

    set( mpipcClientServerTestSuite
        test_clientserver_basic.cpp
        test_clientserver_sendrequest.cpp
        test_clientserver_cancel_server.cpp
        test_clientserver_cancel_client.cpp
        test_clientserver_noserver.cpp
        test_clientserver_oneshot.cpp
        test_clientserver_delayed.cpp
        test_clientserver_idempotent.cpp
    )
    #
    create_test_sourcelist( mpipcClientServerTests test_mpipc_clientserver.cpp ${mpipcClientServerTestSuite})

    add_executable(test_mpipc_clientserver ${mpipcClientServerTests})
    add_dependencies(test_mpipc_clientserver mpipc_test_copy_certs build_openssl build_snappy)

    target_link_libraries(test_mpipc_clientserver
        solid_frame_mpipc
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

    add_test(NAME TestClientServerBasic1        COMMAND  test_mpipc_clientserver test_clientserver_basic 1)
    add_test(NAME TestClientServerBasic2        COMMAND  test_mpipc_clientserver test_clientserver_basic 2)
    add_test(NAME TestClientServerBasic4        COMMAND  test_mpipc_clientserver test_clientserver_basic 4)
    add_test(NAME TestClientServerBasic8        COMMAND  test_mpipc_clientserver test_clientserver_basic 8)

    add_test(NAME TestClientServerBasic1S       COMMAND  test_mpipc_clientserver test_clientserver_basic 1 s)
    add_test(NAME TestClientServerBasic2S       COMMAND  test_mpipc_clientserver test_clientserver_basic 2 s)
    add_test(NAME TestClientServerBasic4S       COMMAND  test_mpipc_clientserver test_clientserver_basic 4 s)
    add_test(NAME TestClientServerBasic8S       COMMAND  test_mpipc_clientserver test_clientserver_basic 8 s)

    add_test(NAME TestClientServerBasic1C       COMMAND  test_mpipc_clientserver test_clientserver_basic 1 c)
    add_test(NAME TestClientServerBasic2C       COMMAND  test_mpipc_clientserver test_clientserver_basic 2 c)
    add_test(NAME TestClientServerBasic4C       COMMAND  test_mpipc_clientserver test_clientserver_basic 4 c)
    add_test(NAME TestClientServerBasic8C       COMMAND  test_mpipc_clientserver test_clientserver_basic 8 c)

    add_test(NAME TestClientServerBasic1B       COMMAND  test_mpipc_clientserver test_clientserver_basic 1 b)
    add_test(NAME TestClientServerBasic2B       COMMAND  test_mpipc_clientserver test_clientserver_basic 2 b)
    add_test(NAME TestClientServerBasic4B       COMMAND  test_mpipc_clientserver test_clientserver_basic 4 b)
    add_test(NAME TestClientServerBasic8B       COMMAND  test_mpipc_clientserver test_clientserver_basic 8 b)

    add_test(NAME TestClientServerSendRequest   COMMAND  test_mpipc_clientserver test_clientserver_sendrequest)
    add_test(NAME TestClientServerSendRequestS  COMMAND  test_mpipc_clientserver test_clientserver_sendrequest 1 s)
    add_test(NAME TestClientServerCancelServer  COMMAND  test_mpipc_clientserver test_clientserver_cancel_server)
    add_test(NAME TestClientServerCancelServerS COMMAND  test_mpipc_clientserver test_clientserver_cancel_server s)
    add_test(NAME TestClientServerCancelClient  COMMAND  test_mpipc_clientserver test_clientserver_cancel_client)
    add_test(NAME TestClientServerCancelClientS COMMAND  test_mpipc_clientserver test_clientserver_cancel_client s)
    add_test(NAME TestClientServerNoServer      COMMAND  test_mpipc_clientserver test_clientserver_noserver)
    add_test(NAME TestClientServerNoServerS     COMMAND  test_mpipc_clientserver test_clientserver_noserver 1 s)
    add_test(NAME TestClientServerOneShot       COMMAND  test_mpipc_clientserver test_clientserver_oneshot)
    add_test(NAME TestClientServerOneShotS      COMMAND  test_mpipc_clientserver test_clientserver_oneshot 1 s)
    add_test(NAME TestClientServerDelayed       COMMAND  test_mpipc_clientserver test_clientserver_delayed)
    add_test(NAME TestClientServerDelayedS      COMMAND  test_mpipc_clientserver test_clientserver_delayed 1 s)
    add_test(NAME TestClientServerIdempontent   COMMAND  test_mpipc_clientserver test_clientserver_idempotent)
    add_test(NAME TestClientServerIdempontentS  COMMAND  test_mpipc_clientserver test_clientserver_idempotent 1 s)


    #==============================================================================

    set( mpipcKeepAliveTestSuite
        test_keepalive_fail.cpp
        test_keepalive_success.cpp
    )

    create_test_sourcelist( mpipcKeepAliveTests test_mpipc_keepalive.cpp ${mpipcKeepAliveTestSuite})

    add_executable(test_mpipc_keepalive ${mpipcKeepAliveTests})

    add_dependencies(test_mpipc_keepalive build_openssl)

    target_link_libraries(test_mpipc_keepalive
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestKeepAliveFail1        COMMAND  test_mpipc_keepalive test_keepalive_fail "0")
    add_test(NAME TestKeepAliveFail2        COMMAND  test_mpipc_keepalive test_keepalive_fail "1")
    add_test(NAME TestKeepAliveSuccess1     COMMAND  test_mpipc_keepalive test_keepalive_success "0")


    #==============================================================================

    set( mpipcConnectionTestSuite
        test_connection_close.cpp
    )

    create_test_sourcelist( mpipcConnectionTests test_mpipc_connection.cpp ${mpipcConnectionTestSuite})

    add_executable(test_mpipc_connection ${mpipcConnectionTests})

    add_dependencies(test_mpipc_connection build_openssl)

    target_link_libraries(test_mpipc_connection
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestConnectionClose       COMMAND  test_mpipc_connection test_connection_close)

    #==============================================================================

    set( mpipcPoolTestSuite
        test_pool_force_close.cpp
        test_pool_delay_close.cpp
    )

    create_test_sourcelist( mpipcPoolTests test_mpipc_pool.cpp ${mpipcPoolTestSuite})

    add_executable(test_mpipc_pool ${mpipcPoolTests})

    add_dependencies(test_mpipc_pool build_openssl)

    target_link_libraries(test_mpipc_pool
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestPoolForceClose1       COMMAND  test_mpipc_pool test_pool_force_close 1)
    add_test(NAME TestPoolForceClose2       COMMAND  test_mpipc_pool test_pool_force_close 2)
    add_test(NAME TestPoolForceClose4       COMMAND  test_mpipc_pool test_pool_force_close 4)

    add_test(NAME TestPoolDelayClose1       COMMAND  test_mpipc_pool test_pool_delay_close 1)
    add_test(NAME TestPoolDelayClose2       COMMAND  test_mpipc_pool test_pool_delay_close 2)
    add_test(NAME TestPoolDelayClose4       COMMAND  test_mpipc_pool test_pool_delay_close 4)

    #==============================================================================

    add_subdirectory(test_multiprotocol_basic)

    set( mpipcMultiProtocolTestSuite
        test_multiprotocol_basic.cpp
    )

    create_test_sourcelist( mpipcMultiProtocolTests test_mpipc_multiprotocol.cpp ${mpipcMultiProtocolTestSuite})

    add_executable(test_mpipc_multiprotocol ${mpipcMultiProtocolTests})

    add_dependencies(test_mpipc_multiprotocol build_openssl)

    target_link_libraries(test_mpipc_multiprotocol
        test_multiprotocol_alpha_client
        test_multiprotocol_alpha_server
        test_multiprotocol_beta_client
        test_multiprotocol_beta_server
        test_multiprotocol_gamma_client
        test_multiprotocol_gamma_server
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestMultiProtocolBasic        COMMAND  test_mpipc_multiprotocol test_multiprotocol_basic)

    #==============================================================================

    set( mpipcRawTestSuite
        test_raw_basic.cpp
        test_raw_proxy.cpp
    )

    create_test_sourcelist( mpipcRawTests test_mpipc_raw.cpp ${mpipcRawTestSuite})

    add_executable(test_mpipc_raw ${mpipcRawTests})

    add_dependencies(test_mpipc_raw build_openssl)

    target_link_libraries(test_mpipc_raw
        solid_frame_mpipc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestRawBasic      COMMAND  test_mpipc_raw test_raw_basic)
    #add_test(NAME TestRawProxy     COMMAND  test_mpipc_raw test_raw_proxy)

    #==============================================================================
    
    set( mpipcRelayTestSuite
        test_relay_basic.cpp
        test_relay_disabled.cpp
        test_relay_cancel_request.cpp
        test_relay_cancel_response.cpp
        test_relay_close_request.cpp
        test_relay_close_response.cpp
    )
    #
    create_test_sourcelist( mpipcRelayTests test_mpipc_relay.cpp ${mpipcRelayTestSuite})

    add_executable(test_mpipc_relay ${mpipcRelayTests})
    add_dependencies(test_mpipc_relay mpipc_test_copy_certs build_openssl build_snappy)

    target_link_libraries(test_mpipc_relay
        solid_frame_mpipc
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

    add_test(NAME TestRelayDisabled1        COMMAND  test_mpipc_relay test_relay_disabled 1)
    add_test(NAME TestRelayBasic1           COMMAND  test_mpipc_relay test_relay_basic 1)
    add_test(NAME TestRelayCancelRequest1   COMMAND  test_mpipc_relay test_relay_cancel_request 1)
    add_test(NAME TestRelayCancelResponse1  COMMAND  test_mpipc_relay test_relay_cancel_response 1)
    add_test(NAME TestRelayCloseRequest1    COMMAND  test_mpipc_relay test_relay_close_request 1)
    add_test(NAME TestRelayCloseResponse1   COMMAND  test_mpipc_relay test_relay_close_response 1)
    
    #==============================================================================
    
    set( mpipcRelayEngineTestSuite
        test_relay_engine_basic.cpp
    )

    create_test_sourcelist( mpipcRelayEngineTests test_mpipc_relay_engine.cpp ${mpipcRelayEngineTestSuite})

    add_executable(test_mpipc_relay_engine ${mpipcRelayEngineTests})

    add_dependencies(test_mpipc_relay_engine build_openssl)

    target_link_libraries(test_mpipc_relay_engine
        solid_frame_mpipc
        solid_frame_aio
        solid_frame
        solid_serialization
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${SYS_DYNAMIC_LOAD_LIB}
        ${SYS_BASIC_LIBS}
    )

    add_test(NAME TestRelayEngineBasic     COMMAND  test_mpipc_relay_engine test_relay_engine_basic)
    #==============================================================================
endif(OPENSSL_FOUND)
