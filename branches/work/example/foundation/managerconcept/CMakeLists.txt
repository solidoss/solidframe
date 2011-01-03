add_executable (managerconcept managerconcept.cpp)

target_link_libraries (managerconcept
	foundation_ipc
	foundation_aio
	foundation_core
	algorithm_serialization
	utility
	system
	boost_program_options
	boost_system
	${SYS_BASIC_LIBS}
)


