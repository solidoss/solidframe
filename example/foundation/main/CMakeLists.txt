#include_directories (${SOLIDGROUND_SOURCE_DIR})

link_directories (${SOLIDGROUND_SOURCE_DIR}/system ${SOLIDGROUND_SOURCE_DIR}/foundation
${SOLIDGROUND_SOURCE_DIR}/test/foundation/core ${SOLIDGROUND_SOURCE_DIR}/test/foundation/echo
${SOLIDGROUND_SOURCE_DIR}/test/foundation/alpha
${SOLIDGROUND_SOURCE_DIR}/test/foundation/cluster)

add_executable (concept src/concept.cpp)

#NOTE:
#set_target_properties(test PROPERTIES COMPILE_FLAGS "-DUDEBUG")

target_link_libraries (concept
	concept_echo
	concept_beta
	concept_proxy
	concept_alpha
	concept_core
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
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
	dl
)
