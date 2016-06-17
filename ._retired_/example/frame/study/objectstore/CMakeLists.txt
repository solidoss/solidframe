add_executable (study_objectstore objectstore.cpp)

target_link_libraries (study_objectstore
	solid_utility
	solid_system
	${BOOST_PROGRAM_OPTIONS_LIB}
	${BOOST_SYSTEM_LIB}
	${SYS_BASIC_LIBS}
)
