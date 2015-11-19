#==============================================================================
set( UtilityTestSuite
	test_invalid_index.cpp
)

create_test_sourcelist( UtilityTests test_utility.cpp ${UtilityTestSuite})

add_executable(test_utility ${UtilityTests})

target_link_libraries(test_utility
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestUtility test_utility
	test_invalid_index
)
#==============================================================================