add_executable (example_dchatstress dchatstress.cpp dchatstressservice.cpp cliparser.cpp dchatmessagematrix.cpp)


target_link_libraries (example_dchatstress
	example_dchat_core
	solid_frame_aio
	solid_frame_core
	solid_serialization
	solid_utility
	solid_system
	${OPENSSL_LIBS}
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
