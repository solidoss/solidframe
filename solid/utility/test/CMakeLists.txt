#==============================================================================
set( UtilityTestSuite
    test_invalid_index.cpp
    test_innerlist.cpp
    test_any.cpp
    test_event.cpp
    test_memory_file.cpp
    test_workpool.cpp
    test_workpool_fast.cpp
    test_workpool2_fast.cpp
    test_ioformat.cpp
    test_function.cpp
    test_function_perf.cpp
)

create_test_sourcelist( UtilityTests test_utility.cpp ${UtilityTestSuite})

add_executable(test_utility ${UtilityTests})

target_link_libraries(test_utility
    solid_utility
    solid_system
    ${SYS_BASIC_LIBS}
)

add_test(NAME TestUtilityWorkPool           COMMAND  test_utility test_workpool)
add_test(NAME TestUtilityWorkPoolFast       COMMAND  test_utility test_workpool_fast)
add_test(NAME TestUtilityWorkPool2Fast      COMMAND  test_utility test_workpool2_fast)
add_test(NAME TestUtilityIoFormat           COMMAND  test_utility test_ioformat)
add_test(NAME TestUtilityInvalidIndex       COMMAND  test_utility test_invalid_index)
add_test(NAME TestUtilityInnerList          COMMAND  test_utility test_innerlist)
add_test(NAME TestUtilityAny                COMMAND  test_utility test_any)
add_test(NAME TestUtilityEvent              COMMAND  test_utility test_event)
add_test(NAME TestUtilityMemoryFile         COMMAND  test_utility test_memory_file)
add_test(NAME TestUtilityMemoryFile2M       COMMAND  test_utility test_memory_file 2222222)
add_test(NAME TestUtilityMemoryFile3M       COMMAND  test_utility test_memory_file 3333333)
add_test(NAME TestUtilityMemoryFile5M       COMMAND  test_utility test_memory_file 5555555)
add_test(NAME TestUtilityFunction           COMMAND  test_utility test_function)

add_test(NAME TestUtilityFunctionPerf_s_0_10000_1000           COMMAND  test_utility test_function_perf s 0 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_0_10000_1000           COMMAND  test_utility test_function_perf S 0 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_1_10000_1000           COMMAND  test_utility test_function_perf s 1 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_1_10000_1000           COMMAND  test_utility test_function_perf S 1 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_2_10000_1000           COMMAND  test_utility test_function_perf s 2 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_2_10000_1000           COMMAND  test_utility test_function_perf S 2 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_4_10000_1000           COMMAND  test_utility test_function_perf s 4 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_4_10000_1000           COMMAND  test_utility test_function_perf S 4 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_16_10000_1000           COMMAND  test_utility test_function_perf s 16 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_16_10000_1000           COMMAND  test_utility test_function_perf S 16 10000 1000)

#==============================================================================
