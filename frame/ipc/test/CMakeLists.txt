
set( TestSuite
	test_protocol_basic.cpp
)

create_test_sourcelist( IpcTests test_ipc.cpp ${TestSuite})

add_executable(test_ipc ${IpcTests})

target_link_libraries(test_ipc
	solid_frame_ipc
	solid_frame_aio
	solid_frame
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestProtocol test_ipc
	test_protocol_basic
)
add_test( TestProtocol test_ipc
	test_protocol_multi
)
