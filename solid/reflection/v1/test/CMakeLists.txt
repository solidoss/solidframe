#==============================================================================
set( ReflectionTestSuite
    test_reflection_basic.cpp
)

create_test_sourcelist( ReflectionTests test_reflection.cpp ${ReflectionTestSuite})

add_executable(test_reflection ${ReflectionTests})

target_link_libraries(test_reflection
    solid_reflection_v1
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestReflectionBasic       COMMAND  test_reflection test_reflection_basic)

#==============================================================================
