#set_source_files_properties(boostproxyserver.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(boost_proxyserver boostproxyserver.cpp)

target_link_libraries (boost_proxyserver ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

#set_source_files_properties(boostechoclient.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(boost_echoclient0 boostechoclient0.cpp)

target_link_libraries (boost_echoclient0 solid_system ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

#set_source_files_properties(boostechoserver.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)


add_executable(boost_echoclient1 boostechoclient1.cpp)

target_link_libraries (boost_echoclient1 solid_system ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})

#set_source_files_properties(boostechoserver.cpp COMPILE_FLAGS -DBOOST_ALL_NO_LIB=1)

add_executable(boost_echoserver boostechoserver.cpp)

target_link_libraries (boost_echoserver ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${SYS_BASIC_LIBS})
