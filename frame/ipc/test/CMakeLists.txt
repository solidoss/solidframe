
set( TestSuite
	test_protocol.cpp
)

create_test_sourcelist( IpcTests test_ipc.cpp ${TestSuite})

add_executable(test_ipc ${IpcTests})

target_link_libraries(test_ipc solid_system solid_frame solid_frame_aio solid_frame_ipc)

add_test( TestProtocol test_ipc
	test_protocol
)

