#==============================================================================
set( SerializationTestSuite
	test_binarybasic.cpp
	test_typeidjoin.cpp
)

create_test_sourcelist( SerializationTests test_serialization.cpp ${SerializationTestSuite})

add_executable(test_serialization ${SerializationTests})

target_link_libraries(test_serialization
	solid_serialization
	solid_utility
	solid_system
	${SYS_BASIC_LIBS}
)

add_test( TestSerialization test_serialization
	test_binarybasic
	test_typeidjoin
)

#==============================================================================