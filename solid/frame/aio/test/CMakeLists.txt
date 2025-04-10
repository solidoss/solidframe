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

    set( aioTestSuite
        test_echo_tcp_stress.cpp
        test_event_stress.cpp
        test_event_stress_wp.cpp
    )
    #
    create_test_sourcelist( aioTests test_aio.cpp ${aioTestSuite})

    add_executable(test_aio ${aioTests})
    add_dependencies(test_aio aio_test_copy_certs build-openssl)

    target_link_libraries(test_aio
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )
    if(SOLID_ON_WINDOWS AND OPENSSL_SSL_DLL AND OPENSSL_CRYPTO_DLL)
        add_custom_command(TARGET test_aio POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_SSL_DLL} $<TARGET_FILE_DIR:test_aio>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_CRYPTO_DLL} $<TARGET_FILE_DIR:test_aio>
        )
    endif()
    
    add_test(NAME TestAioEventStress100_100         COMMAND  test_aio test_event_stress 100 100)
    add_test(NAME TestEventStressWP00_100_100       COMMAND  test_aio test_event_stress_wp 0 100 100)
    
    add_test(NAME TestAioEventStress100_1000        COMMAND  test_aio test_event_stress 100 1000)
    add_test(NAME TestEventStressWP00_100_1000      COMMAND  test_aio test_event_stress_wp 0 100 1000)
    
    add_test(NAME TestAioEventStress20_10000        COMMAND  test_aio test_event_stress 20 10000)
    
    #add_test(NAME TestEventStressWP1_20_10000      COMMAND  test_aio test_event_stress_wp 1 20 10000)
    add_test(NAME TestEventStressWP02_20_10000      COMMAND  test_aio test_event_stress_wp 2 20 10000)
    add_test(NAME TestEventStressWP04_20_10000      COMMAND  test_aio test_event_stress_wp 4 20 10000)
    add_test(NAME TestEventStressWP08_20_10000      COMMAND  test_aio test_event_stress_wp 8 20 10000)
    add_test(NAME TestEventStressWP10_20_10000      COMMAND  test_aio test_event_stress_wp 10 20 10000)
    #add_test(NAME TestEventStressWP100_10000       COMMAND  test_aio test_event_stress_wp 0 100 1000)

    add_test(NAME TestAioEchoTcpStress1             COMMAND  test_aio test_echo_tcp_stress 1)
    add_test(NAME TestAioEchoTcpStress2             COMMAND  test_aio test_echo_tcp_stress 2)
    add_test(NAME TestAioEchoTcpStress4             COMMAND  test_aio test_echo_tcp_stress 4)
    add_test(NAME TestAioEchoTcpStress8             COMMAND  test_aio test_echo_tcp_stress 8)
    add_test(NAME TestAioEchoTcpStress16            COMMAND  test_aio test_echo_tcp_stress 16)

    add_test(NAME TestAioEchoTcpStress1r            COMMAND  test_aio test_echo_tcp_stress 1 r)
    add_test(NAME TestAioEchoTcpStress2r            COMMAND  test_aio test_echo_tcp_stress 2 r)
    add_test(NAME TestAioEchoTcpStress4r            COMMAND  test_aio test_echo_tcp_stress 4 r)
    add_test(NAME TestAioEchoTcpStress8r            COMMAND  test_aio test_echo_tcp_stress 8 r)
    add_test(NAME TestAioEchoTcpStress16r           COMMAND  test_aio test_echo_tcp_stress 16 r)

    add_test(NAME TestAioEchoTcpStress1s            COMMAND  test_aio test_echo_tcp_stress 1 s)
    add_test(NAME TestAioEchoTcpStress2s            COMMAND  test_aio test_echo_tcp_stress 2 s)
    add_test(NAME TestAioEchoTcpStress4s            COMMAND  test_aio test_echo_tcp_stress 4 s)
    add_test(NAME TestAioEchoTcpStress8s            COMMAND  test_aio test_echo_tcp_stress 8 s)
    add_test(NAME TestAioEchoTcpStress16s           COMMAND  test_aio test_echo_tcp_stress 16 s)

    add_test(NAME TestAioEchoTcpStress1rs           COMMAND  test_aio test_echo_tcp_stress 1 r s)
    add_test(NAME TestAioEchoTcpStress2rs           COMMAND  test_aio test_echo_tcp_stress 2 r s)
    add_test(NAME TestAioEchoTcpStress4rs           COMMAND  test_aio test_echo_tcp_stress 4 r s)
    add_test(NAME TestAioEchoTcpStress8rs           COMMAND  test_aio test_echo_tcp_stress 8 r s)
    add_test(NAME TestAioEchoTcpStress16rs          COMMAND  test_aio test_echo_tcp_stress 16 r s)

    set_tests_properties(
        TestAioEventStress100_100    
        TestEventStressWP00_100_100  
        TestAioEventStress100_1000   
        TestEventStressWP00_100_1000 
        TestAioEventStress20_10000   
        TestEventStressWP02_20_10000 
        TestEventStressWP04_20_10000 
        TestEventStressWP08_20_10000 
        TestEventStressWP10_20_10000
        TestAioEchoTcpStress1        
        TestAioEchoTcpStress2        
        TestAioEchoTcpStress4        
        TestAioEchoTcpStress8        
        TestAioEchoTcpStress16       
        TestAioEchoTcpStress1r       
        TestAioEchoTcpStress2r       
        TestAioEchoTcpStress4r       
        TestAioEchoTcpStress8r       
        TestAioEchoTcpStress16r      
        TestAioEchoTcpStress1s       
        TestAioEchoTcpStress2s       
        TestAioEchoTcpStress4s       
        TestAioEchoTcpStress8s       
        TestAioEchoTcpStress16s      
        TestAioEchoTcpStress1rs      
        TestAioEchoTcpStress2rs      
        TestAioEchoTcpStress4rs      
        TestAioEchoTcpStress8rs      
        TestAioEchoTcpStress16rs     
        PROPERTIES LABELS "aio stress"
    )
    
    set_tests_properties(
        TestAioEventStress100_100    
        TestEventStressWP00_100_100  
        TestAioEventStress100_1000   
        TestEventStressWP00_100_1000 
        TestAioEventStress20_10000   
        TestEventStressWP02_20_10000 
        TestEventStressWP04_20_10000 
        TestEventStressWP08_20_10000 
        TestEventStressWP10_20_10000   
        PROPERTIES LABELS "aio stress event"
    )

    set_tests_properties(
        TestEventStressWP00_100_100  
        TestEventStressWP00_100_1000 
        TestEventStressWP02_20_10000 
        TestEventStressWP04_20_10000 
        TestEventStressWP08_20_10000 
        TestEventStressWP10_20_10000   
        PROPERTIES LABELS "aio stress event threadpool"
    )

    #==============================================================================

    set( testPerfSuite
        test_perf_actor_aio.cpp
        test_perf_actor_frame.cpp
        test_perf_threadpool_lockfree.cpp
        test_perf_threadpool_synch_context.cpp
        test_perf_timestore.cpp
    )
    #
    create_test_sourcelist( perfTests test_perf.cpp ${testPerfSuite})

    add_executable(test_perf ${perfTests})
    add_dependencies(test_perf aio_test_copy_certs build-openssl)

    target_link_libraries(test_perf
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_utility
        solid_system
        ${SYSTEM_BASIC_LIBRARIES}
    )
    if(SOLIDSOLID_ON_WINDOWS AND OPENSSL_SSL_DLL AND OPENSSL_CRYPTO_DLL_ON_WINDOWS)
        add_custom_command(TARGET test_perf POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_SSL_DLL} $<TARGET_FILE_DIR:test_perf>
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OPENSSL_CRYPTO_DLL} $<TARGET_FILE_DIR:test_perf>
        )
    endif()
    
    add_test(NAME TestPerfActorAio                COMMAND  test_perf test_perf_actor_aio)
    add_test(NAME TestPerfTimeStore               COMMAND  test_perf test_perf_timestore)
    add_test(NAME TestPerfActorFrame              COMMAND  test_perf test_perf_actor_frame)
    add_test(NAME TestPerfThreadPoolLockFree      COMMAND  test_perf test_perf_threadpool_lockfree)
    add_test(NAME TestPerfThreadPoolSynchCtx      COMMAND  test_perf test_perf_threadpool_synch_context)

    set_tests_properties(
        TestPerfActorAio            
        TestPerfTimeStore           
        TestPerfActorFrame          
        TestPerfThreadPoolLockFree  
        TestPerfThreadPoolSynchCtx    
        PROPERTIES LABELS "aio perf"
    )

    set_tests_properties(      
        TestPerfThreadPoolLockFree  
        TestPerfThreadPoolSynchCtx    
        PROPERTIES LABELS "aio perf threadpool"
    )


endif(OPENSSL_FOUND)

