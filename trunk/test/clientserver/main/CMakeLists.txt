include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system ${SOLIDGROUND_SOURCE_DIR}/clientserver
${SOLIDGROUND_SOURCE_DIR}/test/clientserver/core ${SOLIDGROUND_SOURCE_DIR}/test/clientserver/echo
${SOLIDGROUND_SOURCE_DIR}/test/clientserver/alpha
${SOLIDGROUND_SOURCE_DIR}/test/clientserver/cluster)

add_executable (test src/test.cpp)

#NOTE:
#set_target_properties(test PROPERTIES COMPILE_FLAGS "-DUDEBUG")

target_link_libraries (test test_echo test_alpha test_core test_ipc test_beta
 clientserver_tcp clientserver_udp clientserver_core protocol serialization utility system pthread rt boost_filesystem)
