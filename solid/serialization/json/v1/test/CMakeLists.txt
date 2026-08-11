#==============================================================================
set( SerializationJsonV1TestSuite
    test_json.cpp
)

create_test_sourcelist( SerializationJsonV1Tests test_serialization_json.cpp ${SerializationJsonV1TestSuite})

add_executable(test_serialization_json_v1 ${SerializationJsonV1Tests})

target_include_directories(test_serialization_json_v1 PRIVATE
    ${SolidFrame_SOURCE_DIR}
)

target_link_libraries(test_serialization_json_v1
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestSerializationJsonV1 COMMAND test_serialization_json_v1 test_json)

#==============================================================================
