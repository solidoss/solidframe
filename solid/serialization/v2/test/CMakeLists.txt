#==============================================================================
set( SerializationTestSuite
    test_binary.cpp
)

create_test_sourcelist( SerializationTests test_serialization.cpp ${SerializationTestSuite})

add_executable(test_serialization_v2 ${SerializationTests})

target_link_libraries(test_serialization_v2
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
)

add_test(NAME TestSerializationBinary       COMMAND  test_serialization_v2 test_binary)
