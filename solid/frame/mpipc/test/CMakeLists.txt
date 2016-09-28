#==============================================================================
set( mpipcProtocolTestSuite
	test_protocol_basic.cpp
 	test_protocol_synchronous.cpp
	test_protocol_cancel.cpp
)

create_test_sourcelist( mpipcProtocolTests test_mpipc_protocol.cpp ${mpipcProtocolTestSuite})

add_executable(test_mpipc_protocol ${mpipcProtocolTests})

target_link_libraries(test_mpipc_protocol
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestProtocolBasic		COMMAND  test_mpipc_protocol test_protocol_basic)
add_test(NAME TestProtocolCancel	COMMAND  test_mpipc_protocol test_protocol_cancel)
add_test(NAME TestProtocolSynch		COMMAND  test_mpipc_protocol test_protocol_synchronous)

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
 
target_link_libraries(test_mpipc_clientserver
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)
 
add_test(NAME TestClientServerBasic1		COMMAND  test_mpipc_clientserver test_clientserver_basic 1)
add_test(NAME TestClientServerBasic2		COMMAND  test_mpipc_clientserver test_clientserver_basic 2)
add_test(NAME TestClientServerBasic4		COMMAND  test_mpipc_clientserver test_clientserver_basic 4)
add_test(NAME TestClientServerBasic8		COMMAND  test_mpipc_clientserver test_clientserver_basic 8)
add_test(NAME TestClientServerSendRequest	COMMAND  test_mpipc_clientserver test_clientserver_sendrequest)
add_test(NAME TestClientServerCancelServer	COMMAND  test_mpipc_clientserver test_clientserver_cancel_server)
add_test(NAME TestClientServerCancelClient	COMMAND  test_mpipc_clientserver test_clientserver_cancel_client)
add_test(NAME TestClientServerNoServer		COMMAND  test_mpipc_clientserver test_clientserver_noserver)
add_test(NAME TestClientServerOneShot		COMMAND  test_mpipc_clientserver test_clientserver_oneshot)
add_test(NAME TestClientServerDelayed		COMMAND  test_mpipc_clientserver test_clientserver_delayed)
add_test(NAME TestClientServerIdempontent	COMMAND  test_mpipc_clientserver test_clientserver_idempotent)


#==============================================================================

set( mpipcKeepAliveTestSuite
	test_keepalive_fail.cpp
	test_keepalive_success.cpp
)

create_test_sourcelist( mpipcKeepAliveTests test_mpipc_keepalive.cpp ${mpipcKeepAliveTestSuite})

add_executable(test_mpipc_keepalive ${mpipcKeepAliveTests})

target_link_libraries(test_mpipc_keepalive
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestKeepAliveFail1		COMMAND  test_mpipc_keepalive test_keepalive_fail "0")
add_test(NAME TestKeepAliveFail2		COMMAND  test_mpipc_keepalive test_keepalive_fail "1")
add_test(NAME TestKeepAliveSuccess1		COMMAND  test_mpipc_keepalive test_keepalive_success "0")


#==============================================================================

set( mpipcConnectionTestSuite
	test_connection_close.cpp
)

create_test_sourcelist( mpipcConnectionTests test_mpipc_connection.cpp ${mpipcConnectionTestSuite})

add_executable(test_mpipc_connection ${mpipcConnectionTests})

target_link_libraries(test_mpipc_connection
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestConnectionClose		COMMAND  test_mpipc_connection test_connection_close)

#==============================================================================

set( mpipcPoolTestSuite
	test_pool_force_close.cpp
	test_pool_delay_close.cpp
)

create_test_sourcelist( mpipcPoolTests test_mpipc_pool.cpp ${mpipcPoolTestSuite})

add_executable(test_mpipc_pool ${mpipcPoolTests})

target_link_libraries(test_mpipc_pool
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestPoolForceClose1		COMMAND  test_mpipc_pool test_pool_force_close 1)
add_test(NAME TestPoolForceClose2		COMMAND  test_mpipc_pool test_pool_force_close 2)
add_test(NAME TestPoolForceClose4		COMMAND  test_mpipc_pool test_pool_force_close 4)

add_test(NAME TestPoolDelayClose1		COMMAND  test_mpipc_pool test_pool_delay_close 1)
add_test(NAME TestPoolDelayClose2		COMMAND  test_mpipc_pool test_pool_delay_close 2)
add_test(NAME TestPoolDelayClose4		COMMAND  test_mpipc_pool test_pool_delay_close 4)

#==============================================================================

add_subdirectory(test_multiprotocol_basic)

set( mpipcMultiProtocolTestSuite
	test_multiprotocol_basic.cpp
)

create_test_sourcelist( mpipcMultiProtocolTests test_mpipc_multiprotocol.cpp ${mpipcMultiProtocolTestSuite})

add_executable(test_mpipc_multiprotocol ${mpipcMultiProtocolTests})

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
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestMultiProtocolBasic		COMMAND  test_mpipc_multiprotocol test_multiprotocol_basic)

#==============================================================================

set( mpipcRawTestSuite
	test_raw_basic.cpp
	test_raw_proxy.cpp
)

create_test_sourcelist( mpipcRawTests test_mpipc_raw.cpp ${mpipcRawTestSuite})

add_executable(test_mpipc_raw ${mpipcRawTests})

target_link_libraries(test_mpipc_raw
	solid_frame_mpipc
	solid_frame_aio
	solid_frame_aio_openssl
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
	${SYS_BASIC_LIBS}
)

add_test(NAME TestRawBasic		COMMAND  test_mpipc_raw test_raw_basic)
#add_test(NAME TestRawProxy		COMMAND  test_mpipc_raw test_raw_proxy)

#==============================================================================
