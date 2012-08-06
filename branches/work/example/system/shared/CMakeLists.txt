set_source_files_properties(testshared.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(example_testshared EXCLUDE_FROM_ALL testshared.cpp)

target_link_libraries (example_testshared system ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

