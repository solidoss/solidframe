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
    test_threadpool_multicast_basic.cpp
    test_threadpool_multicast_sleep.cpp
    test_threadpool_multicast_synchronization_context_basic.cpp
    test_ioformat.cpp
    test_function.cpp
    test_function_perf.cpp
    test_template_function.cpp
    test_function_any_speed_full_solid.cpp
    test_function_any_speed_full_stl.cpp
    test_shared_buffer.cpp
    test_collapse.cpp
    test_intrusiveptr.cpp
)

set( ThreadPoolTestSuite
    test_threadpool_context.cpp
    test_threadpool_thread_context.cpp
    test_threadpool.cpp
    test_threadpool_basic.cpp
    test_threadpool_chain.cpp
    test_threadpool_pattern.cpp
    #test_threadpool_try.cpp
)

create_test_sourcelist( UtilityTests test_utility.cpp ${UtilityTestSuite})

add_executable(test_utility ${UtilityTests})

target_link_libraries(test_utility
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

create_test_sourcelist( ThreadPoolTests test_threadpool_suite.cpp ${ThreadPoolTestSuite})

add_executable(test_threadpool ${ThreadPoolTests})

target_link_libraries(test_threadpool
    solid_utility
    solid_system
    ${SYSTEM_BASIC_LIBRARIES}
)

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
add_test(NAME TestUtilityThreadpoolMulticastBasic       COMMAND  test_utility test_threadpool_multicast_basic)
add_test(NAME TestUtilityThreadpoolMulticastSleep       COMMAND  test_utility test_threadpool_multicast_sleep)
add_test(NAME TestUtilityCallPoolMulticastBasic         COMMAND  test_utility test_callpool_multicast_basic)
add_test(NAME TestUtilityCallPoolMulticastPattern       COMMAND  test_utility test_callpool_multicast_pattern)
add_test(NAME TestUtilityCallPoolMulticastSynchCtxBasic COMMAND  test_utility test_threadpool_multicast_synchronization_context_basic)
add_test(NAME TestUtilityFunctionPerf_s_2_10000_1000    COMMAND  test_utility test_function_perf s 2 10000 1000)
add_test(NAME TestUtilityFunctionPerf_S_2_10000_1000    COMMAND  test_utility test_function_perf S 2 10000 1000)
add_test(NAME TestUtilitySharedBuffer                   COMMAND  test_utility test_shared_buffer)
add_test(NAME TestCollapse_B                            COMMAND  test_utility test_collapse B)
add_test(NAME TestCollapse_p                            COMMAND  test_utility test_collapse p 10 4)
add_test(NAME TestCollapse_b                            COMMAND  test_utility test_collapse b 10 4)
add_test(NAME TestIntrusivePtr                          COMMAND  test_utility test_intrusiveptr)
add_test(NAME TestThreadPoolThreadContext               COMMAND  test_threadpool test_threadpool_thread_context)
add_test(NAME TestThreadPoolContext                     COMMAND  test_threadpool test_threadpool_context)
add_test(NAME TestThreadPoolPattern                     COMMAND  test_threadpool test_threadpool_pattern)
add_test(NAME TestThreadPoolBasic                       COMMAND  test_threadpool test_threadpool_basic)
add_test(NAME TestThreadPoolChain2                      COMMAND  test_threadpool test_threadpool_chain 2)
add_test(NAME TestThreadPoolChain4                      COMMAND  test_threadpool test_threadpool_chain 4)
add_test(NAME TestThreadPoolChain8                      COMMAND  test_threadpool test_threadpool_chain 8)
add_test(NAME TestThreadPoolChain10                     COMMAND  test_threadpool test_threadpool_chain 10)
add_test(NAME TestThreadPool                            COMMAND  test_threadpool test_threadpool)
add_test(NAME TestThreadPool_0                          COMMAND  test_threadpool test_threadpool 1000 10 0 0 1 0 0)
add_test(NAME TestThreadPool_1                          COMMAND  test_threadpool test_threadpool 100000 10 100 0 2 0 0)
add_test(NAME TestThreadPool_2                          COMMAND  test_threadpool test_threadpool 10 10 0 0 2 100 0)
add_test(NAME TestThreadPool_3                          COMMAND  test_threadpool test_threadpool 10 10 0 0 2 0 100)
add_test(NAME TestThreadPool_4                          COMMAND  test_threadpool test_threadpool 1  10 0 0 2 100 100)
add_test(NAME TestThreadPool_1_2                        COMMAND  test_threadpool test_threadpool 100000 10 100 2 2 0 0)
add_test(NAME TestThreadPool_2_2                        COMMAND  test_threadpool test_threadpool 10 10 0 2 2 100 0)
add_test(NAME TestThreadPool_3_2                        COMMAND  test_threadpool test_threadpool 10 10 0 2 2 0 100)
add_test(NAME TestThreadPool_4_2                        COMMAND  test_threadpool test_threadpool 1  10 0 2 2 100 100)
add_test(NAME TestThreadPool_1_4                        COMMAND  test_threadpool test_threadpool 100000 10 100 4 4 0 0)
add_test(NAME TestThreadPool_2_4                        COMMAND  test_threadpool test_threadpool 10 10 0 4 4 100 0)
add_test(NAME TestThreadPool_3_4                        COMMAND  test_threadpool test_threadpool 10 10 0 4 4 0 100)
add_test(NAME TestThreadPool_4_4                        COMMAND  test_threadpool test_threadpool 1  10 0 4 4 100 100)

#==============================================================================
