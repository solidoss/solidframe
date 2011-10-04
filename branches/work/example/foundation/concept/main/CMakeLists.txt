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
	algorithm_protocol
	audit_log_manager
	audit_log
	algorithm_serialization
	utility
	system
	${OPENSSL_LIBS}
	${BOOST_FILESYSTEM_LIB}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
