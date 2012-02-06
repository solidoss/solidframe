
set( MyTests
	test_thread.cpp
	test_file.cpp
	test_socket.cpp
)

create_test_sourcelist( Tests system_test.cpp ${MyTests})

add_executable(test_system ${Tests})

target_link_libraries(test_system system)

add_test( ThreadTest system_test
	test_thread
)

add_test( SocketTest system_test
	test_socket
)

add_test( FileTest system_test
	test_file
)
