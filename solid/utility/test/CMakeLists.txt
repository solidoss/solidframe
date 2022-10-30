#==============================================================================
set( UtilityTestSuite
    test_invalid_index.cpp
    test_innerlist.cpp
    test_any.cpp
    test_anytuple.cpp
    test_queue.cpp
    test_event.cpp
    test_event_size.cpp
    test_memory_file.cpp
    test_callpool_multicast_basic.cpp
    test_callpool_multicast_pattern.cpp
    test_workpool_multicast_basic.cpp
    test_workpool_multicast_sleep.cpp
    test_workpool_multicast_synchronization_context_basic.cpp
    test_ioformat.cpp
    test_function.cpp
    test_function_perf.cpp
    test_template_function.cpp
    test_function_any_speed_full_solid.cpp
    test_function_any_speed_full_stl.cpp
    test_shared_buffer.cpp
)

set( WorkPoolTestSuite
    test_workpool_context.cpp
    test_workpool_thread_context.cpp
    test_workpool.cpp
    test_workpool_basic.cpp
    test_workpool_chain.cpp
    test_workpool_pattern.cpp
    test_workpool_try.cpp
)

create_test_sourcelist( UtilityTests test_utility.cpp ${UtilityTestSuite})

add_executable(test_utility ${UtilityTests})

target_link_libraries(test_utility
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

create_test_sourcelist( WorkPoolLockfreeTests test_workpool_lockfree.cpp ${WorkPoolTestSuite})

add_executable(test_workpool_lockfree ${WorkPoolLockfreeTests})

target_link_libraries(test_workpool_lockfree
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

target_compile_definitions(test_workpool_lockfree PRIVATE SOLID_WORKPOOL_OPTION=0)

create_test_sourcelist( WorkPoolLockingTests test_workpool_locking.cpp ${WorkPoolTestSuite})

create_test_sourcelist( WorkPoolMulticastTests test_workpool_multicast.cpp ${WorkPoolTestSuite})

add_executable(test_workpool_multicast ${WorkPoolMulticastTests})

target_link_libraries(test_workpool_multicast
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

target_compile_definitions(test_workpool_multicast PRIVATE SOLID_WORKPOOL_OPTION=2)

### test_utility
add_test(NAME TestUtilityIoFormat                       COMMAND  test_utility test_ioformat)
add_test(NAME TestUtilityInvalidIndex                   COMMAND  test_utility test_invalid_index)
add_test(NAME TestUtilityInnerList                      COMMAND  test_utility test_innerlist)
add_test(NAME TestUtilityAny                            COMMAND  test_utility test_any)
add_test(NAME TestUtilityAnyTuple                       COMMAND  test_utility test_anytuple)
add_test(NAME TestUtilityQueue                          COMMAND  test_utility test_queue)
add_test(NAME TestUtilityEvent                          COMMAND  test_utility test_event)
add_test(NAME TestUtilityMemoryFile                     COMMAND  test_utility test_memory_file)
add_test(NAME TestUtilityMemoryFile2M                   COMMAND  test_utility test_memory_file 2222222)
add_test(NAME TestUtilityMemoryFile3M                   COMMAND  test_utility test_memory_file 3333333)
add_test(NAME TestUtilityMemoryFile5M                   COMMAND  test_utility test_memory_file 5555555)
add_test(NAME TestUtilityFunction                       COMMAND  test_utility test_function)
add_test(NAME TestUtilityTemplateFunction               COMMAND  test_utility test_template_function)
add_test(NAME TestUtilityFunctionAnySpeedFullSolid      COMMAND  test_utility test_function_any_speed_full_solid)
add_test(NAME TestUtilityFunctionAnySpeedFullStl        COMMAND  test_utility test_function_any_speed_full_stl)
add_test(NAME TestUtilityWorkpoolMulticastBasic         COMMAND  test_utility test_workpool_multicast_basic)
add_test(NAME TestUtilityWorkpoolMulticastSleep         COMMAND  test_utility test_workpool_multicast_sleep)
add_test(NAME TestUtilityCallPoolMulticastBasic         COMMAND  test_utility test_callpool_multicast_basic)
add_test(NAME TestUtilityCallPoolMulticastPattern       COMMAND  test_utility test_callpool_multicast_pattern)
add_test(NAME TestUtilityCallPoolMulticastSynchCtxBasic COMMAND  test_utility test_workpool_multicast_synchronization_context_basic)
#add_test(NAME TestUtilityFunctionPerf_s_0_10000_1000           COMMAND  test_utility test_function_perf s 0 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_0_10000_1000           COMMAND  test_utility test_function_perf S 0 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_1_10000_1000           COMMAND  test_utility test_function_perf s 1 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_1_10000_1000           COMMAND  test_utility test_function_perf S 1 10000 1000)

add_test(NAME TestUtilityFunctionPerf_s_2_10000_1000    COMMAND  test_utility test_function_perf s 2 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_2_10000_1000    COMMAND  test_utility test_function_perf S 2 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_4_10000_1000           COMMAND  test_utility test_function_perf s 4 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_4_10000_1000           COMMAND  test_utility test_function_perf S 4 10000 1000)

#add_test(NAME TestUtilityFunctionPerf_s_16_10000_1000           COMMAND  test_utility test_function_perf s 16 10000 1000)
#add_test(NAME TestUtilityFunctionPerf_S_16_10000_1000           COMMAND  test_utility test_function_perf S 16 10000 1000)


### test_workpool_lockfree
add_test(NAME TestWorkPoolLockfreeThreadContext          COMMAND  test_workpool_lockfree test_workpool_thread_context)
add_test(NAME TestWorkPoolLockfreeContext                COMMAND  test_workpool_lockfree test_workpool_context)
add_test(NAME TestWorkPoolLockfreePattern                COMMAND  test_workpool_lockfree test_workpool_pattern)
add_test(NAME TestWorkPoolLockfreeBasic                  COMMAND  test_workpool_lockfree test_workpool_basic)
add_test(NAME TestWorkPoolLockfreeTry                    COMMAND  test_workpool_lockfree test_workpool_try)
#add_test(NAME TestWorkPoolLockfreeChain1                 COMMAND  test_workpool_lockfree test_workpool_chain 1)
add_test(NAME TestWorkPoolLockfreeChain2                 COMMAND  test_workpool_lockfree test_workpool_chain 2)
add_test(NAME TestWorkPoolLockfreeChain4                 COMMAND  test_workpool_lockfree test_workpool_chain 4)
add_test(NAME TestWorkPoolLockfreeChain8                 COMMAND  test_workpool_lockfree test_workpool_chain 8)
add_test(NAME TestWorkPoolLockfreeChain15                COMMAND  test_workpool_lockfree test_workpool_chain 15)
#add_test(NAME TestWorkPoolLockfreeChain                  COMMAND  test_workpool_lockfree test_workpool_chain)
#add_test(NAME TestWorkPoolLockfreeChain0_1               COMMAND  test_workpool_lockfree test_workpool_chain 0 1)
#add_test(NAME TestWorkPoolLockfreeChain0_2               COMMAND  test_workpool_lockfree test_workpool_chain 0 2)

# test_workpool args: JOB_COUNT WAIT_SECONDS QUEUE_SIZE PRODUCER_COUNT CONSUMER_COUNT PUSH_SLEEP_MSECS JOB_SLEEP_MSECS
add_test(NAME TestWorkPoolLockfree                       COMMAND  test_workpool_lockfree test_workpool)

add_test(NAME TestWorkPoolLockfree_0                     COMMAND  test_workpool_lockfree test_workpool 1000 10 0 0 1 0 0)
add_test(NAME TestWorkPoolLockfree_1                     COMMAND  test_workpool_lockfree test_workpool 100000 10 100 0 2 0 0)
add_test(NAME TestWorkPoolLockfree_2                     COMMAND  test_workpool_lockfree test_workpool 10 10 0 0 2 100 0)
add_test(NAME TestWorkPoolLockfree_3                     COMMAND  test_workpool_lockfree test_workpool 10 10 0 0 2 0 100)
add_test(NAME TestWorkPoolLockfree_4                     COMMAND  test_workpool_lockfree test_workpool 1  10 0 0 2 100 100)

add_test(NAME TestWorkPoolLockfree_1_2                   COMMAND  test_workpool_lockfree test_workpool 100000 10 100 2 2 0 0)
add_test(NAME TestWorkPoolLockfree_2_2                   COMMAND  test_workpool_lockfree test_workpool 10 10 0 2 2 100 0)
add_test(NAME TestWorkPoolLockfree_3_2                   COMMAND  test_workpool_lockfree test_workpool 10 10 0 2 2 0 100)
add_test(NAME TestWorkPoolLockfree_4_2                   COMMAND  test_workpool_lockfree test_workpool 1  10 0 2 2 100 100)

add_test(NAME TestWorkPoolLockfree_1_4                   COMMAND  test_workpool_lockfree test_workpool 100000 10 100 4 4 0 0)
add_test(NAME TestWorkPoolLockfree_2_4                   COMMAND  test_workpool_lockfree test_workpool 10 10 0 4 4 100 0)
add_test(NAME TestWorkPoolLockfree_3_4                   COMMAND  test_workpool_lockfree test_workpool 10 10 0 4 4 0 100)
add_test(NAME TestWorkPoolLockfree_4_4                   COMMAND  test_workpool_lockfree test_workpool 1  10 0 4 4 100 100)

### test_workpool_multicast
add_test(NAME TestWorkPoolMulticastThreadContext          COMMAND  test_workpool_multicast test_workpool_thread_context)
add_test(NAME TestWorkPoolMulticastContext                COMMAND  test_workpool_multicast test_workpool_context)
add_test(NAME TestWorkPoolMulticastPattern                COMMAND  test_workpool_multicast test_workpool_pattern)
add_test(NAME TestWorkPoolMulticastBasic                  COMMAND  test_workpool_multicast test_workpool_basic)
add_test(NAME TestWorkPoolMulticastTry                    COMMAND  test_workpool_multicast test_workpool_try)
#add_test(NAME TestWorkPoolMulticastChain1                 COMMAND  test_workpool_multicast test_workpool_chain 1)
add_test(NAME TestWorkPoolMulticastChain2                 COMMAND  test_workpool_multicast test_workpool_chain 2)
add_test(NAME TestWorkPoolMulticastChain4                 COMMAND  test_workpool_multicast test_workpool_chain 4)
add_test(NAME TestWorkPoolMulticastChain8                 COMMAND  test_workpool_multicast test_workpool_chain 8)
add_test(NAME TestWorkPoolMulticastChain15                COMMAND  test_workpool_multicast test_workpool_chain 15)
#add_test(NAME TestWorkPoolMulticastChain                  COMMAND  test_workpool_multicast test_workpool_chain)
#add_test(NAME TestWorkPoolMulticastChain0_1               COMMAND  test_workpool_multicast test_workpool_chain 0 1)
#add_test(NAME TestWorkPoolMulticastChain0_2               COMMAND  test_workpool_multicast test_workpool_chain 0 2)

# test_workpool args: JOB_COUNT WAIT_SECONDS QUEUE_SIZE PRODUCER_COUNT CONSUMER_COUNT PUSH_SLEEP_MSECS JOB_SLEEP_MSECS
add_test(NAME TestWorkPoolMulticast                       COMMAND  test_workpool_multicast test_workpool)

add_test(NAME TestWorkPoolMulticast_0                     COMMAND  test_workpool_multicast test_workpool 1000 10 0 0 1 0 0)
add_test(NAME TestWorkPoolMulticast_1                     COMMAND  test_workpool_multicast test_workpool 100000 20 100 0 2 0 0)
add_test(NAME TestWorkPoolMulticast_2                     COMMAND  test_workpool_multicast test_workpool 10 10 0 0 2 100 0)
add_test(NAME TestWorkPoolMulticast_3                     COMMAND  test_workpool_multicast test_workpool 10 10 0 0 2 0 100)
add_test(NAME TestWorkPoolMulticast_4                     COMMAND  test_workpool_multicast test_workpool 1  10 0 0 2 100 100)

add_test(NAME TestWorkPoolMulticast_1_2                   COMMAND  test_workpool_multicast test_workpool 100000 20 100 2 2 0 0)
add_test(NAME TestWorkPoolMulticast_2_2                   COMMAND  test_workpool_multicast test_workpool 10 10 0 2 2 100 0)
add_test(NAME TestWorkPoolMulticast_3_2                   COMMAND  test_workpool_multicast test_workpool 10 10 0 2 2 0 100)
add_test(NAME TestWorkPoolMulticast_4_2                   COMMAND  test_workpool_multicast test_workpool 1  10 0 2 2 100 100)

add_test(NAME TestWorkPoolMulticast_1_4                   COMMAND  test_workpool_multicast test_workpool 100000 20 100 4 4 0 0)
add_test(NAME TestWorkPoolMulticast_2_4                   COMMAND  test_workpool_multicast test_workpool 10 10 0 4 4 100 0)
add_test(NAME TestWorkPoolMulticast_3_4                   COMMAND  test_workpool_multicast test_workpool 10 10 0 4 4 0 100)
add_test(NAME TestWorkPoolMulticast_4_4                   COMMAND  test_workpool_multicast test_workpool 1  10 0 4 4 100 100)



#==============================================================================
