#==============================================================================
set( UtilityTestSuite
    test_invalid_index.cpp
    test_innerlist.cpp
    test_any.cpp
    test_event.cpp
    test_memory_file.cpp
    test_workpool_context.cpp
    test_workpool_thread_context.cpp
    test_workpool.cpp
    test_workpool_basic.cpp
    test_workpool_chain.cpp
    test_workpool_pattern.cpp
    test_workpool_try.cpp
    test_ioformat.cpp
    test_function.cpp
    test_function_perf.cpp
    test_template_function.cpp
)

create_test_sourcelist( UtilityTests test_utility.cpp ${UtilityTestSuite})

add_executable(test_utility ${UtilityTests})

target_link_libraries(test_utility
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

add_test(NAME TestUtilityWorkPoolThreadContext          COMMAND  test_utility test_workpool_thread_context)
add_test(NAME TestUtilityWorkPoolContext                COMMAND  test_utility test_workpool_context)
add_test(NAME TestUtilityWorkPoolPattern                COMMAND  test_utility test_workpool_pattern)
add_test(NAME TestUtilityWorkPoolBasic                  COMMAND  test_utility test_workpool_basic)
add_test(NAME TestUtilityWorkPoolTry                    COMMAND  test_utility test_workpool_try)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain 1)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain 2)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain 4)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain 8)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain 16)
add_test(NAME TestUtilityWorkPoolChain                  COMMAND  test_utility test_workpool_chain)
add_test(NAME TestUtilityWorkPoolChain1                 COMMAND  test_utility test_workpool_chain 0 1)
add_test(NAME TestUtilityWorkPoolChain2                 COMMAND  test_utility test_workpool_chain 0 2)
# test_workpool args: JOB_COUNT WAIT_SECONDS QUEUE_SIZE PRODUCER_COUNT CONSUMER_COUNT PUSH_SLEEP_MSECS JOB_SLEEP_MSECS
add_test(NAME TestUtilityWorkPool                       COMMAND  test_utility test_workpool)

add_test(NAME TestUtilityWorkPool_0                     COMMAND  test_utility test_workpool 1000 10 0 0 1 0 0)
add_test(NAME TestUtilityWorkPool_1                     COMMAND  test_utility test_workpool 100000 10 100 0 2 0 0)
add_test(NAME TestUtilityWorkPool_2                     COMMAND  test_utility test_workpool 10 10 0 0 2 100 0)
add_test(NAME TestUtilityWorkPool_3                     COMMAND  test_utility test_workpool 10 10 0 0 2 0 100)
add_test(NAME TestUtilityWorkPool_4                     COMMAND  test_utility test_workpool 1  10 0 0 2 100 100)

add_test(NAME TestUtilityWorkPool_1_2                   COMMAND  test_utility test_workpool 100000 10 100 2 2 0 0)
add_test(NAME TestUtilityWorkPool_2_2                   COMMAND  test_utility test_workpool 10 10 0 2 2 100 0)
add_test(NAME TestUtilityWorkPool_3_2                   COMMAND  test_utility test_workpool 10 10 0 2 2 0 100)
add_test(NAME TestUtilityWorkPool_4_2                   COMMAND  test_utility test_workpool 1  10 0 2 2 100 100)

add_test(NAME TestUtilityWorkPool_1_4                   COMMAND  test_utility test_workpool 100000 10 100 4 4 0 0)
add_test(NAME TestUtilityWorkPool_2_4                   COMMAND  test_utility test_workpool 10 10 0 4 4 100 0)
add_test(NAME TestUtilityWorkPool_3_4                   COMMAND  test_utility test_workpool 10 10 0 4 4 0 100)
add_test(NAME TestUtilityWorkPool_4_4                   COMMAND  test_utility test_workpool 1  10 0 4 4 100 100)

add_test(NAME TestUtilityIoFormat                       COMMAND  test_utility test_ioformat)
add_test(NAME TestUtilityInvalidIndex                   COMMAND  test_utility test_invalid_index)
add_test(NAME TestUtilityInnerList                      COMMAND  test_utility test_innerlist)
add_test(NAME TestUtilityAny                            COMMAND  test_utility test_any)
add_test(NAME TestUtilityEvent                          COMMAND  test_utility test_event)
add_test(NAME TestUtilityMemoryFile                     COMMAND  test_utility test_memory_file)
add_test(NAME TestUtilityMemoryFile2M                   COMMAND  test_utility test_memory_file 2222222)
add_test(NAME TestUtilityMemoryFile3M                   COMMAND  test_utility test_memory_file 3333333)
add_test(NAME TestUtilityMemoryFile5M                   COMMAND  test_utility test_memory_file 5555555)
add_test(NAME TestUtilityFunction                       COMMAND  test_utility test_function)
add_test(NAME TestUtilityTemplateFunction               COMMAND  test_utility test_template_function)
                    
#add_test(NAME TestUtilityFunctionPerf_s_0_10000_1000           COMMAND  test_utility test_function_perf s 0 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_0_10000_1000           COMMAND  test_utility test_function_perf S 0 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_1_10000_1000           COMMAND  test_utility test_function_perf s 1 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_1_10000_1000           COMMAND  test_utility test_function_perf S 1 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_2_10000_1000     COMMAND  test_utility test_function_perf s 2 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_2_10000_1000     COMMAND  test_utility test_function_perf S 2 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_4_10000_1000           COMMAND  test_utility test_function_perf s 4 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_4_10000_1000           COMMAND  test_utility test_function_perf S 4 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_16_10000_1000           COMMAND  test_utility test_function_perf s 16 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_16_10000_1000           COMMAND  test_utility test_function_perf S 16 10000 1000)

#==============================================================================
