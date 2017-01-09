#==============================================================================
set( UtilityTestSuite
    test_invalid_index.cpp
    test_innerlist.cpp
    test_any.cpp
    test_event.cpp
    test_memory_file.cpp
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

add_test( TestUtility test_utility
    test_innerlist
)

add_test( TestUtility test_utility
    test_any
)

add_test( TestUtility test_utility
    test_event
)

add_test( TestUtility test_utility
    test_memory_file
)
add_test(NAME TestUtilityInvalidIndex       COMMAND  test_utility test_invalid_index)
add_test(NAME TestUtilityInnerList          COMMAND  test_utility test_innerlist)
add_test(NAME TestUtilityAny                COMMAND  test_utility test_any)
add_test(NAME TestUtilityEvent              COMMAND  test_utility test_event)
add_test(NAME TestUtilityMemoryFile         COMMAND  test_utility test_memory_file)
add_test(NAME TestUtilityMemoryFile2M       COMMAND  test_utility test_memory_file 2222222)
add_test(NAME TestUtilityMemoryFile3M       COMMAND  test_utility test_memory_file 3333333)
add_test(NAME TestUtilityMemoryFile5M       COMMAND  test_utility test_memory_file 5555555)
#==============================================================================
