#==============================================================================
set( SerializationV4TestSuite
    test_binary.cpp
    test_polymorphic.cpp
    test_compat_v3.cpp
)

create_test_sourcelist( SerializationV4Tests test_serialization.cpp ${SerializationV4TestSuite})

add_executable(test_serialization_v4 ${SerializationV4Tests})

target_link_libraries(test_serialization_v4
    solid_serialization_v4
    solid_serialization_v3
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestSerializationV4Binary       COMMAND  test_serialization_v4 test_binary)
add_test(NAME TestSerializationV4Polymorphic  COMMAND  test_serialization_v4 test_polymorphic)
add_test(NAME TestSerializationV4CompatV3     COMMAND  test_serialization_v4 test_compat_v3)

#==============================================================================

add_executable(bench_serialization_v4 bench_binary.cpp)

target_link_libraries(bench_serialization_v4
    solid_serialization_v4
    solid_serialization_v3
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

#==============================================================================
