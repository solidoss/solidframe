add_executable (example_concept src/concept.cpp)

#NOTE:
#set_target_properties(test PROPERTIES COMPILE_FLAGS "-DUDEBUG")

target_link_libraries (example_concept
	example_concept_alpha
	example_concept_gamma
	example_concept_proxy
	example_concept_core
	solid_frame_core
	solid_frame_ipc
	solid_frame_file
	solid_frame_aio
	solid_frame_aio_openssl
	solid_protocol_text
	solid_audit_log_manager
	solid_audit_log
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_FILESYSTEM_LIB}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
