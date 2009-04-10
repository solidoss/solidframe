#include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system ${SOLIDGROUND_SOURCE_DIR}/foundation
${SOLIDGROUND_SOURCE_DIR}/test/foundation/core ${SOLIDGROUND_SOURCE_DIR}/test/foundation/echo
${SOLIDGROUND_SOURCE_DIR}/test/foundation/alpha
${SOLIDGROUND_SOURCE_DIR}/test/foundation/cluster)

add_executable (test_concept src/test.cpp)

#NOTE:
#set_target_properties(test PROPERTIES COMPILE_FLAGS "-DUDEBUG")

target_link_libraries (test_concept
	test_echo
	test_alpha
	test_core
	test_beta
	test_proxy
	foundation_core
	foundation_ipc
	foundation_aio
	foundation_aio_openssl
	ssl
	crypto
	protocol
	audit_log_manager
	audit_log
	serialization
	utility
	system
	boost_filesystem
	pthread
	rt
	dl
)
