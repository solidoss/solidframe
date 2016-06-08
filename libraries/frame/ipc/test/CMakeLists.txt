#==============================================================================
set( IpcProtocolTestSuite
	test_protocol_basic.cpp
 	test_protocol_synchronous.cpp
	test_protocol_cancel.cpp
)

create_test_sourcelist( IpcProtocolTests test_ipc_protocol.cpp ${IpcProtocolTestSuite})

add_executable(test_ipc_protocol ${IpcProtocolTests})

target_link_libraries(test_ipc_protocol
	solid_frame_ipc
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

add_test(NAME TestProtocolBasic		COMMAND  test_ipc_protocol test_protocol_basic)
add_test(NAME TestProtocolCancel	COMMAND  test_ipc_protocol test_protocol_cancel)
add_test(NAME TestProtocolSynch		COMMAND  test_ipc_protocol test_protocol_synchronous)

#==============================================================================

set( IpcClientServerTestSuite
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
create_test_sourcelist( IpcClientServerTests test_ipc_clientserver.cpp ${IpcClientServerTestSuite})
 
add_executable(test_ipc_clientserver ${IpcClientServerTests})
 
target_link_libraries(test_ipc_clientserver
	solid_frame_ipc
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
 
add_test(NAME TestClientServerBasic1		COMMAND  test_ipc_clientserver test_clientserver_basic 1)
add_test(NAME TestClientServerBasic2		COMMAND  test_ipc_clientserver test_clientserver_basic 2)
add_test(NAME TestClientServerBasic4		COMMAND  test_ipc_clientserver test_clientserver_basic 4)
add_test(NAME TestClientServerBasic8		COMMAND  test_ipc_clientserver test_clientserver_basic 8)
add_test(NAME TestClientServerSendRequest	COMMAND  test_ipc_clientserver test_clientserver_sendrequest)
add_test(NAME TestClientServerCancelServer	COMMAND  test_ipc_clientserver test_clientserver_cancel_server)
add_test(NAME TestClientServerCancelClient	COMMAND  test_ipc_clientserver test_clientserver_cancel_client)
add_test(NAME TestClientServerNoServer		COMMAND  test_ipc_clientserver test_clientserver_noserver)
add_test(NAME TestClientServerOneShot		COMMAND  test_ipc_clientserver test_clientserver_oneshot)
add_test(NAME TestClientServerDelayed		COMMAND  test_ipc_clientserver test_clientserver_delayed)
add_test(NAME TestClientServerIdempontent	COMMAND  test_ipc_clientserver test_clientserver_idempotent)


#==============================================================================

set( IpcKeepAliveTestSuite
	test_keepalive_fail.cpp
	test_keepalive_success.cpp
)

create_test_sourcelist( IpcKeepAliveTests test_ipc_keepalive.cpp ${IpcKeepAliveTestSuite})

add_executable(test_ipc_keepalive ${IpcKeepAliveTests})

target_link_libraries(test_ipc_keepalive
	solid_frame_ipc
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

add_test(NAME TestKeepAliveFail1		COMMAND  test_ipc_keepalive test_keepalive_fail "0")
add_test(NAME TestKeepAliveFail2		COMMAND  test_ipc_keepalive test_keepalive_fail "1")
add_test(NAME TestKeepAliveSuccess1		COMMAND  test_ipc_keepalive test_keepalive_success "0")


#==============================================================================

set( IpcConnectionTestSuite
	test_connection_close.cpp
)

create_test_sourcelist( IpcConnectionTests test_ipc_connection.cpp ${IpcConnectionTestSuite})

add_executable(test_ipc_connection ${IpcConnectionTests})

target_link_libraries(test_ipc_connection
	solid_frame_ipc
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

add_test(NAME TestConnectionClose		COMMAND  test_ipc_connection test_connection_close)

#==============================================================================

set( IpcPoolTestSuite
	test_pool_force_close.cpp
	test_pool_delay_close.cpp
)

create_test_sourcelist( IpcPoolTests test_ipc_pool.cpp ${IpcPoolTestSuite})

add_executable(test_ipc_pool ${IpcPoolTests})

target_link_libraries(test_ipc_pool
	solid_frame_ipc
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

add_test(NAME TestPoolForceClose1		COMMAND  test_ipc_pool test_pool_force_close 1)
add_test(NAME TestPoolForceClose2		COMMAND  test_ipc_pool test_pool_force_close 2)
add_test(NAME TestPoolForceClose4		COMMAND  test_ipc_pool test_pool_force_close 4)

add_test(NAME TestPoolDelayClose1		COMMAND  test_ipc_pool test_pool_delay_close 1)
add_test(NAME TestPoolDelayClose2		COMMAND  test_ipc_pool test_pool_delay_close 2)
add_test(NAME TestPoolDelayClose4		COMMAND  test_ipc_pool test_pool_delay_close 4)

#==============================================================================

add_subdirectory(test_multiprotocol_basic)

set( IpcMultiProtocolTestSuite
	test_multiprotocol_basic.cpp
)

create_test_sourcelist( IpcMultiProtocolTests test_ipc_multiprotocol.cpp ${IpcMultiProtocolTestSuite})

add_executable(test_ipc_multiprotocol ${IpcMultiProtocolTests})

target_link_libraries(test_ipc_multiprotocol
	test_multiprotocol_alpha_client
	test_multiprotocol_alpha_server
	test_multiprotocol_beta_client
	test_multiprotocol_beta_server
	test_multiprotocol_gamma_client
	test_multiprotocol_gamma_server
	solid_frame_ipc
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

add_test(NAME TestMultiProtocolBasic		COMMAND  test_ipc_multiprotocol test_multiprotocol_basic)

#==============================================================================

set( IpcRawTestSuite
	test_raw_basic.cpp
)

create_test_sourcelist( IpcRawTests test_ipc_raw.cpp ${IpcRawTestSuite})

add_executable(test_ipc_raw ${IpcRawTests})

target_link_libraries(test_ipc_raw
	solid_frame_ipc
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

add_test(NAME TestRawBasic		COMMAND  test_ipc_raw test_raw_basic)
#==============================================================================