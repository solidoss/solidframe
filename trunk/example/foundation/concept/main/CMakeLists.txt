add_executable (concept src/concept.cpp)

#NOTE:
#set_target_properties(test PROPERTIES COMPILE_FLAGS "-DUDEBUG")

target_link_libraries (concept
	concept_proxy
	concept_alpha
	concept_gamma
	concept_core
	foundation_core
	foundation_ipc
	foundation_file
	foundation_aio
	foundation_aio_openssl
	ssl
	crypto
	algorithm_protocol
	audit_log_manager
	audit_log
	algorithm_serialization
	utility
	system
	boost_filesystem
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
	dl
)
