add_executable (study_som_sm main.cpp sharedobjectmanager_sm.cpp)

target_link_libraries (study_som_sm	
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (study_som_ms main.cpp sharedobjectmanager_ms.cpp)

target_link_libraries (study_som_ms
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
