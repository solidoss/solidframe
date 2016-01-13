add_executable (study_time_queue time_queue.cpp)

target_link_libraries (study_time_queue
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
)
