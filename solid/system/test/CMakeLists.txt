
set( MyTests
    test_nanotime.cpp
    test_exception.cpp
    test_flags.cpp
    test_log_basic.cpp
    test_log_file.cpp
    test_log_socket.cpp
    test_log_recorder.cpp
    test_crashhandler.cpp
    test_chunkedstream.cpp
    test_pimpl.cpp
)

create_test_sourcelist( Tests test_system.cpp ${MyTests})

add_executable(test_system ${Tests} test_pimpl_impl.cpp)

target_link_libraries(test_system solid_system ${SYSTEM_BASIC_LIBRARIES})


add_test(NAME TestSystemNanoTime        COMMAND  test_system test_nanotime)
add_test(NAME TestSystemException       COMMAND  test_system test_exception "some super text")
add_test(NAME TestSystemFlags           COMMAND  test_system test_flags)
add_test(NAME TestSystemLogBasic        COMMAND  test_system test_log_basic)
add_test(NAME TestSystemLogRecorder     COMMAND  test_system test_log_recorder)
add_test(NAME TestSystemChunkedStream   COMMAND  test_system test_chunkedstream)
add_test(NAME TestSystemPimpl           COMMAND  test_system test_pimpl)
