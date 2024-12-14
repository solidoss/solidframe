#==============================================================================

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

add_custom_target(mprpc_test_copy_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(OPENSSL_FOUND)

    #==============================================================================
    set( mprpcProtocolTestSuite
        test_protocol_basic.cpp
        test_protocol_synchronous.cpp
        test_protocol_cancel.cpp
    )

    create_test_sourcelist( mprpcProtocolTests test_mprpc_protocol.cpp ${mprpcProtocolTestSuite})

    add_executable(test_mprpc_protocol ${mprpcProtocolTests})

    add_dependencies(test_mprpc_protocol build-openssl)

    target_link_libraries(test_mprpc_protocol
        solid_frame_mprpc
        solid_frame_aio
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestProtocolBasic     COMMAND  test_mprpc_protocol test_protocol_basic)
    add_test(NAME TestProtocolCancel    COMMAND  test_mprpc_protocol test_protocol_cancel)
    add_test(NAME TestProtocolSynch     COMMAND  test_mprpc_protocol test_protocol_synchronous)

    #==============================================================================

    set( mprpcClientServerTestSuite
        test_clientserver_basic.cpp
        test_clientserver_versioning.cpp
        test_clientserver_sendrequest.cpp
        test_clientserver_cancel_server.cpp
        test_clientserver_cancel_client.cpp
        test_clientserver_noserver.cpp
        test_clientserver_oneshot.cpp
        test_clientserver_delayed.cpp
        test_clientserver_idempotent.cpp
        test_clientserver_split.cpp
        test_clientserver_upload.cpp
        test_clientserver_upload_single.cpp
        test_clientserver_download.cpp
        test_clientserver_timeout_secure.cpp
        test_clientserver_topic.cpp
        test_clientserver_stop.cpp
        test_clientserver_pause_read.cpp
    )

    if(SOLID_ON_WINDOWS)
        set_source_files_properties(test_clientserver_versioning.cpp PROPERTIES COMPILE_OPTIONS "/bigobj")
    endif()

    #
    create_test_sourcelist( mprpcClientServerTests test_mprpc_clientserver.cpp ${mprpcClientServerTestSuite})

    add_executable(test_mprpc_clientserver ${mprpcClientServerTests})

    target_link_libraries(test_mprpc_clientserver
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_dependencies(test_mprpc_clientserver build-snappy mprpc_test_copy_certs build-openssl)

    if(SOLID_ON_WINDOWS AND OPENSSL_SSL_DLL AND OPENSSL_CRYPTO_DLL)
        add_custom_command(TARGET test_mprpc_clientserver POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_SSL_DLL} $<TARGET_FILE_DIR:test_mprpc_clientserver>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_CRYPTO_DLL} $<TARGET_FILE_DIR:test_mprpc_clientserver>
        )
    endif()
    
    add_test(NAME TestClientServerBasic_1                COMMAND  test_mprpc_clientserver test_clientserver_basic 1)
    add_test(NAME TestClientServerBasic_2                COMMAND  test_mprpc_clientserver test_clientserver_basic 2)
    add_test(NAME TestClientServerBasic_4                COMMAND  test_mprpc_clientserver test_clientserver_basic 4)
    add_test(NAME TestClientServerBasic_8                COMMAND  test_mprpc_clientserver test_clientserver_basic 8)
    add_test(NAME TestClientServerBasic_8_C              COMMAND  test_mprpc_clientserver test_clientserver_basic 8 _ c)
    
    add_test(NAME TestClientServerBasic_1S               COMMAND  test_mprpc_clientserver test_clientserver_basic 1 s)
    add_test(NAME TestClientServerBasic_2S               COMMAND  test_mprpc_clientserver test_clientserver_basic 2 s)
    add_test(NAME TestClientServerBasic_4S               COMMAND  test_mprpc_clientserver test_clientserver_basic 4 s)
    add_test(NAME TestClientServerBasic_8S               COMMAND  test_mprpc_clientserver test_clientserver_basic 8 s)
    add_test(NAME TestClientServerBasic_8SC              COMMAND  test_mprpc_clientserver test_clientserver_basic 8 s c)

    add_test(NAME TestClientServerBasic_1C               COMMAND  test_mprpc_clientserver test_clientserver_basic 1 c)
    add_test(NAME TestClientServerBasic_2C               COMMAND  test_mprpc_clientserver test_clientserver_basic 2 c)
    add_test(NAME TestClientServerBasic_4C               COMMAND  test_mprpc_clientserver test_clientserver_basic 4 c)
    add_test(NAME TestClientServerBasic_8C               COMMAND  test_mprpc_clientserver test_clientserver_basic 8 c)
    add_test(NAME TestClientServerBasic_8CC              COMMAND  test_mprpc_clientserver test_clientserver_basic 8 c c)

    add_test(NAME TestClientServerBasic_1B               COMMAND  test_mprpc_clientserver test_clientserver_basic 1 b)
    add_test(NAME TestClientServerBasic_2B               COMMAND  test_mprpc_clientserver test_clientserver_basic 2 b)
    add_test(NAME TestClientServerBasic_4B               COMMAND  test_mprpc_clientserver test_clientserver_basic 4 b)
    add_test(NAME TestClientServerBasic_8B               COMMAND  test_mprpc_clientserver test_clientserver_basic 8 b)
    add_test(NAME TestClientServerBasic_8BC              COMMAND  test_mprpc_clientserver test_clientserver_basic 8 b c)

    add_test(NAME TestClientServerVersioning            COMMAND  test_mprpc_clientserver test_clientserver_versioning)

    add_test(NAME TestClientServerTopic                 COMMAND  test_mprpc_clientserver test_clientserver_topic)
    
    add_test(NAME TestClientServerSendRequest           COMMAND  test_mprpc_clientserver test_clientserver_sendrequest)
    add_test(NAME TestClientServerSendRequestS          COMMAND  test_mprpc_clientserver test_clientserver_sendrequest 1 s)
    add_test(NAME TestClientServerCancelServer          COMMAND  test_mprpc_clientserver test_clientserver_cancel_server)
    add_test(NAME TestClientServerCancelServerS         COMMAND  test_mprpc_clientserver test_clientserver_cancel_server s)
    add_test(NAME TestClientServerCancelClient          COMMAND  test_mprpc_clientserver test_clientserver_cancel_client)
    add_test(NAME TestClientServerCancelClientS         COMMAND  test_mprpc_clientserver test_clientserver_cancel_client s)
    add_test(NAME TestClientServerNoServer              COMMAND  test_mprpc_clientserver test_clientserver_noserver)
    add_test(NAME TestClientServerNoServerS             COMMAND  test_mprpc_clientserver test_clientserver_noserver 1 s)
    add_test(NAME TestClientServerOneShot               COMMAND  test_mprpc_clientserver test_clientserver_oneshot)
    add_test(NAME TestClientServerOneShotS              COMMAND  test_mprpc_clientserver test_clientserver_oneshot 1 s)
    add_test(NAME TestClientServerDelayed               COMMAND  test_mprpc_clientserver test_clientserver_delayed)
    add_test(NAME TestClientServerDelayedS              COMMAND  test_mprpc_clientserver test_clientserver_delayed 1 s)
    add_test(NAME TestClientServerIdempontent           COMMAND  test_mprpc_clientserver test_clientserver_idempotent)
    add_test(NAME TestClientServerIdempontentS          COMMAND  test_mprpc_clientserver test_clientserver_idempotent 1 s)
    add_test(NAME TestClientServerSplit1                COMMAND  test_mprpc_clientserver test_clientserver_split 1)
    add_test(NAME TestClientServerUpload                COMMAND  test_mprpc_clientserver test_clientserver_upload)
    add_test(NAME TestClientServerUploadSingle          COMMAND  test_mprpc_clientserver test_clientserver_upload_single)
    add_test(NAME TestClientServerDownload              COMMAND  test_mprpc_clientserver test_clientserver_download)
    add_test(NAME TestClientServerTimeoutSecureS        COMMAND  test_mprpc_clientserver test_clientserver_timeout_secure 10 s)
    add_test(NAME TestClientServerTimeoutSecureP        COMMAND  test_mprpc_clientserver test_clientserver_timeout_secure 10 p)
    add_test(NAME TestClientServerTimeoutSecureA        COMMAND  test_mprpc_clientserver test_clientserver_timeout_secure 10 a)

    set_tests_properties(
        TestClientServerBasic_1        
        TestClientServerBasic_2        
        TestClientServerBasic_4        
        TestClientServerBasic_8        
        TestClientServerBasic_8_C      
        TestClientServerBasic_1S       
        TestClientServerBasic_2S       
        TestClientServerBasic_4S       
        TestClientServerBasic_8S       
        TestClientServerBasic_8SC      
        TestClientServerBasic_1C       
        TestClientServerBasic_2C       
        TestClientServerBasic_4C       
        TestClientServerBasic_8C       
        TestClientServerBasic_8CC      
        TestClientServerBasic_1B       
        TestClientServerBasic_2B       
        TestClientServerBasic_4B       
        TestClientServerBasic_8B       
        TestClientServerBasic_8BC      
        TestClientServerVersioning     
        TestClientServerTopic          
        TestClientServerSendRequest    
        TestClientServerSendRequestS   
        TestClientServerCancelServer   
        TestClientServerCancelServerS  
        TestClientServerCancelClient   
        TestClientServerCancelClientS  
        TestClientServerNoServer       
        TestClientServerNoServerS      
        TestClientServerOneShot        
        TestClientServerOneShotS       
        TestClientServerDelayed        
        TestClientServerDelayedS       
        TestClientServerIdempontent    
        TestClientServerIdempontentS   
        TestClientServerSplit1         
        TestClientServerUpload         
        TestClientServerUploadSingle   
        TestClientServerDownload       
        TestClientServerTimeoutSecureS 
        TestClientServerTimeoutSecureP 
        TestClientServerTimeoutSecureA    
        PROPERTIES LABELS "mprpc clientserver"
    )
    #==============================================================================

    set( mprpcKeepAliveTestSuite
        test_keepalive_fail.cpp
        test_keepalive_success.cpp
    )

    create_test_sourcelist( mprpcKeepAliveTests test_mprpc_keepalive.cpp ${mprpcKeepAliveTestSuite})

    add_executable(test_mprpc_keepalive ${mprpcKeepAliveTests})

    add_dependencies(test_mprpc_keepalive build-openssl)

    target_link_libraries(test_mprpc_keepalive
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestKeepAliveFail1        COMMAND  test_mprpc_keepalive test_keepalive_fail "0")
    add_test(NAME TestKeepAliveFail2        COMMAND  test_mprpc_keepalive test_keepalive_fail "1")
    add_test(NAME TestKeepAliveSuccess1     COMMAND  test_mprpc_keepalive test_keepalive_success "0")
    
    set_tests_properties(
        TestKeepAliveFail1   
        TestKeepAliveFail2   
        TestKeepAliveSuccess1    
        PROPERTIES LABELS "mprpc keepalive"
    )

    #==============================================================================

    set( mprpcConnectionTestSuite
        test_connection_close.cpp
    )

    create_test_sourcelist( mprpcConnectionTests test_mprpc_connection.cpp ${mprpcConnectionTestSuite})

    add_executable(test_mprpc_connection ${mprpcConnectionTests})

    add_dependencies(test_mprpc_connection build-openssl)

    target_link_libraries(test_mprpc_connection
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestConnectionClose       COMMAND  test_mprpc_connection test_connection_close)

    set_tests_properties(
        TestConnectionClose   
        PROPERTIES LABELS "mprpc connection"
    )

    #==============================================================================

    set( mprpcPoolTestSuite
        test_pool_basic.cpp
        test_pool_force_close.cpp
        test_pool_delay_close.cpp
    )

    create_test_sourcelist( mprpcPoolTests test_mprpc_pool.cpp ${mprpcPoolTestSuite})

    add_executable(test_mprpc_pool ${mprpcPoolTests})

    add_dependencies(test_mprpc_pool build-openssl build-snappy)

    target_link_libraries(test_mprpc_pool
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestPoolBasic1            COMMAND  test_mprpc_pool test_pool_basic 1)
    
    add_test(NAME TestPoolForceClose1       COMMAND  test_mprpc_pool test_pool_force_close 1)
    add_test(NAME TestPoolForceClose2       COMMAND  test_mprpc_pool test_pool_force_close 2)
    add_test(NAME TestPoolForceClose4       COMMAND  test_mprpc_pool test_pool_force_close 4)

    add_test(NAME TestPoolDelayClose1       COMMAND  test_mprpc_pool test_pool_delay_close 1)
    add_test(NAME TestPoolDelayClose2       COMMAND  test_mprpc_pool test_pool_delay_close 2)
    add_test(NAME TestPoolDelayClose4       COMMAND  test_mprpc_pool test_pool_delay_close 4)

    set_tests_properties(
        TestPoolBasic1     
        TestPoolForceClose1
        TestPoolForceClose2
        TestPoolForceClose4
        TestPoolDelayClose1
        TestPoolDelayClose2
        TestPoolDelayClose4
        PROPERTIES LABELS "mprpc pool"
    )

    #==============================================================================

    add_subdirectory(multiprotocol_basic)

    set( mprpcMultiProtocolTestSuite
        test_multiprotocol_basic.cpp
    )

    create_test_sourcelist( mprpcMultiProtocolTests test_mprpc_multiprotocol.cpp ${mprpcMultiProtocolTestSuite})

    add_executable(test_mprpc_multiprotocol ${mprpcMultiProtocolTests})

    add_dependencies(test_mprpc_multiprotocol build-openssl)

    target_link_libraries(test_mprpc_multiprotocol
        multiprotocol_alpha_client
        multiprotocol_alpha_server
        multiprotocol_beta_client
        multiprotocol_beta_server
        multiprotocol_gamma_client
        multiprotocol_gamma_server
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestMultiProtocolBasic        COMMAND  test_mprpc_multiprotocol test_multiprotocol_basic)

    set_tests_properties(
        TestMultiProtocolBasic
        PROPERTIES LABELS "mprpc multiprotocol"
    )

    #==============================================================================

    set( mprpcRawTestSuite
        test_raw_basic.cpp
        test_raw_proxy.cpp
    )

    create_test_sourcelist( mprpcRawTests test_mprpc_raw.cpp ${mprpcRawTestSuite})

    add_executable(test_mprpc_raw ${mprpcRawTests})

    add_dependencies(test_mprpc_raw build-openssl)

    target_link_libraries(test_mprpc_raw
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestRawBasic      COMMAND  test_mprpc_raw test_raw_basic)
    #add_test(NAME TestRawProxy     COMMAND  test_mprpc_raw test_raw_proxy)

    set_tests_properties(
        TestRawBasic
        PROPERTIES LABELS "mprpc raw"
    )

    #==============================================================================

    set( mprpcRelayTestSuite
        test_relay_basic.cpp
        test_relay_disabled.cpp
        test_relay_cancel_request.cpp
        test_relay_cancel_response.cpp
        test_relay_close_request.cpp
        test_relay_close_response.cpp
        test_relay_split.cpp
        test_relay_detect_close.cpp
        test_relay_detect_close_while_response.cpp
    )
    #
    create_test_sourcelist( mprpcRelayTests test_mprpc_relay.cpp ${mprpcRelayTestSuite})

    add_executable(test_mprpc_relay ${mprpcRelayTests})
    add_dependencies(test_mprpc_relay mprpc_test_copy_certs build-openssl build-snappy)

    target_link_libraries(test_mprpc_relay
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestRelayDisabled1                    COMMAND  test_mprpc_relay test_relay_disabled 1)
    add_test(NAME TestRelayBasic1                       COMMAND  test_mprpc_relay test_relay_basic 1)
    add_test(NAME TestRelayCancelRequest1               COMMAND  test_mprpc_relay test_relay_cancel_request 1)
    add_test(NAME TestRelayCancelResponse1              COMMAND  test_mprpc_relay test_relay_cancel_response 1)
    add_test(NAME TestRelayCloseRequest1                COMMAND  test_mprpc_relay test_relay_close_request 1)
    add_test(NAME TestRelayCloseResponse1               COMMAND  test_mprpc_relay test_relay_close_response 1)
    add_test(NAME TestRelaySplit1                       COMMAND  test_mprpc_relay test_relay_split 1)
    add_test(NAME TestRelayDetectClose                  COMMAND  test_mprpc_relay test_relay_detect_close)
    add_test(NAME TestRelayDetectCloseWhileResponse     COMMAND  test_mprpc_relay test_relay_detect_close_while_response)
    
    set_tests_properties(
        TestRelayDisabled1               
        TestRelayBasic1                  
        TestRelayCancelRequest1          
        TestRelayCancelResponse1         
        TestRelayCloseRequest1           
        TestRelayCloseResponse1          
        TestRelaySplit1                  
        TestRelayDetectClose             
        TestRelayDetectCloseWhileResponse
        PROPERTIES LABELS "mprpc relay"
    )
    #==============================================================================

    set( mprpcRelayEngineTestSuite
        test_relay_engine_basic.cpp
    )

    create_test_sourcelist( mprpcRelayEngineTests test_mprpc_relay_engine.cpp ${mprpcRelayEngineTestSuite})

    add_executable(test_mprpc_relay_engine ${mprpcRelayEngineTests})

    add_dependencies(test_mprpc_relay_engine build-openssl)

    target_link_libraries(test_mprpc_relay_engine
        solid_frame_mprpc
        solid_frame_aio
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestRelayEngineBasic     COMMAND  test_mprpc_relay_engine test_relay_engine_basic)

    set_tests_properties(
        TestRelayEngineBasic
        PROPERTIES LABELS "mprpc relayengine"
    )
    
    #==============================================================================

    set( mprpcClientFrontBackTestSuite
        test_clientfrontback_upload.cpp
        test_clientfrontback_download.cpp
    )
    #
    create_test_sourcelist( mprpcClientFrontBackTests test_mprpc_clientfrontback.cpp ${mprpcClientFrontBackTestSuite})

    add_executable(test_mprpc_clientfrontback ${mprpcClientFrontBackTests})
    add_dependencies(test_mprpc_clientfrontback mprpc_test_copy_certs build-openssl build-snappy)

    target_link_libraries(test_mprpc_clientfrontback
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v3
        solid_utility
        solid_system
        ${SNAPPY_LIB}
        ${SYSTEM_BASIC_LIBRARIES}
    )

    add_test(NAME TestClientFrontBackUpload        COMMAND  test_mprpc_clientfrontback test_clientfrontback_upload)
    add_test(NAME TestClientFrontBackDownload      COMMAND  test_mprpc_clientfrontback test_clientfrontback_download)

    set_tests_properties(
        TestClientFrontBackUpload
        TestClientFrontBackDownload
        PROPERTIES LABELS "mprpc clientfrontback"
    )
    #==============================================================================
    
endif(OPENSSL_FOUND)
