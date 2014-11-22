add_executable (study_prodcons prodcons.cpp)

target_link_libraries (study_prodcons
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (study_boost_queue boost_queue.cpp)

target_link_libraries (study_boost_queue
	${BOOST_THREAD_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)