#set_source_files_properties(testcallonce.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(example_testcallonce testcallonce.cpp)

target_link_libraries (example_testcallonce ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})


