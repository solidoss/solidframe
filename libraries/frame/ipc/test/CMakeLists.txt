#==============================================================================
set( IpcProtocolTestSuite
	test_protocol_basic.cpp
	test_protocol_synchronous.cpp
	test_protocol_cancel.cpp
	test_protocol_close.cpp
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

add_test( TestProtocol test_ipc_protocol
	test_protocol_basic
	test_protocol_synchronous
	test_protocol_cancel
	test_protocol_close
)

#==============================================================================

set( IpcClientServerTestSuite
	test_clientserver_basic.cpp
	test_clientserver_sendrequest.cpp
	test_clientserver_cancel_server.cpp
	test_clientserver_cancel_client.cpp
)

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

add_test( TestClientServer test_ipc_clientserver
	test_clientserver_sendrequest
	test_clientserver_cancel_server
	test_clientserver_cancel_client
)

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