#==============================================================================
set( SerializationV3TestSuite
    test_binary.cpp
    test_binary_basic.cpp
    test_container.cpp
    test_polymorphic.cpp
)

create_test_sourcelist( SerializationV3Tests test_serialization.cpp ${SerializationV3TestSuite})

add_executable(test_serialization_v3 ${SerializationV3Tests})
target_compile_options(test_serialization_v3 PUBLIC "SHELL: -fconcepts-diagnostics-depth=2")

target_link_libraries(test_serialization_v3
    solid_serialization_v3
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestSerializationV3Binary       COMMAND  test_serialization_v3 test_binary)
add_test(NAME TestSerializationV3BinaryBasic  COMMAND  test_serialization_v3 test_binary_basic)
add_test(NAME TestSerializationV3Polymorphic  COMMAND  test_serialization_v3 test_polymorphic)
add_test(NAME TestSerializationV3Container    COMMAND  test_serialization_v3 test_container)

#==============================================================================

