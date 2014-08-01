add_executable (study_manyobjects manyobjects.cpp)

target_link_libraries (study_manyobjects
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)


add_executable (study_iteratevirtual iteratevirtual.cpp)

target_link_libraries (study_iteratevirtual
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (study_iteratemethodpointer iteratemethodpointer.cpp)

target_link_libraries (study_iteratemethodpointer
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)

add_executable (study_iteratefunctionpointer iteratefunctionpointer.cpp)

target_link_libraries (study_iteratefunctionpointer
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
	${SYS_DYNAMIC_LOAD_LIB}
)
