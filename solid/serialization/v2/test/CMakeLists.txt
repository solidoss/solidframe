#==============================================================================
set( SerializationTestSuite
    test_binary.cpp
    test_binary_basic.cpp
    test_polymorphic.cpp
    test_container.cpp
)

create_test_sourcelist( SerializationTests test_serialization.cpp ${SerializationTestSuite})

add_executable(test_serialization_v2 ${SerializationTests})

target_link_libraries(test_serialization_v2
    solid_serialization_v2
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestSerializationV2Binary       COMMAND  test_serialization_v2 test_binary)
add_test(NAME TestSerializationV2BinaryBasic  COMMAND  test_serialization_v2 test_binary_basic)
add_test(NAME TestSerializationV2Polymorphic  COMMAND  test_serialization_v2 test_polymorphic)
add_test(NAME TestSerializationV2Container    COMMAND  test_serialization_v2 test_container)

#==============================================================================
