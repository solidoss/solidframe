set_source_files_properties(natp2pserver.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(example_natp2p_server natp2pserver.cpp)

target_link_libraries (example_natp2p_server ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

set_source_files_properties(natp2pclient.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(example_natp2p_client natp2pclient.cpp)

target_link_libraries (example_natp2p_client ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

