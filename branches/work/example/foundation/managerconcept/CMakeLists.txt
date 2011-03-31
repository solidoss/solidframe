add_executable (managerconcept managerconcept.cpp)

target_link_libraries (managerconcept
	foundation_ipc
	foundation_aio
	foundation_core
	algorithm_serialization
	utility
	system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
)


