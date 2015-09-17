#==============================================================================
set( IpcProtocolTestSuite
	test_protocol_basic.cpp
	test_protocol_synchronous.cpp
)

create_test_sourcelist( IpcProtocolTests test_ipc_protocol.cpp ${IpcProtocolTestSuite})

add_executable(test_ipc_protocol ${IpcProtocolTests})

target_link_libraries(test_ipc_protocol
	solid_frame_ipc
	solid_frame_aio
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestProtocol test_ipc_protocol
	test_protocol_basic
	test_protocol_synchronous
)

#==============================================================================

set( IpcClientServerTestSuite
	test_clientserver_basic.cpp
	test_clientserver_sendrequest.cpp
)

create_test_sourcelist( IpcClientServerTests test_ipc_clientserver.cpp ${IpcClientServerTestSuite})

add_executable(test_ipc_clientserver ${IpcClientServerTests})

target_link_libraries(test_ipc_clientserver
	solid_frame_ipc
	solid_frame_aio
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestClientServer test_ipc_clientserver
	test_clientserver_basic
	test_clientserver_sendrequest
)

#==============================================================================

set( IpcPeer2PeerTestSuite
	test_peer2peer_basic.cpp
)

create_test_sourcelist( IpcPeer2PeerTests test_ipc_peer2peer.cpp ${IpcPeer2PeerTestSuite})

add_executable(test_ipc_peer2peer ${IpcPeer2PeerTests})

target_link_libraries(test_ipc_peer2peer
	solid_frame_ipc
	solid_frame_aio
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestPeer2Peer test_ipc_peer2peer
	test_peer2peer_basic
)

#==============================================================================
