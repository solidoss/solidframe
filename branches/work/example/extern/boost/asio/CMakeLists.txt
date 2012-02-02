set_source_files_properties(proxyserver.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(proxyserver proxyserver.cpp)

target_link_libraries (proxyserver ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

