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


add_executable(boost_sslechoserver boostsslechoserver.cpp)

target_link_libraries (boost_sslechoserver ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${OPENSSL_LIBS} ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})

add_executable(boost_sslechoclient1 boostsslechoclient1.cpp)

target_link_libraries (boost_sslechoclient1 solid_system ${BOOST_THREAD_LIB} ${BOOST_SYSTEM_LIB} ${OPENSSL_LIBS} ${SYS_BASIC_LIBS} ${SYS_DYNAMIC_LOAD_LIB})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-ca-cert.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-server-key.pem
)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../../../frame/echoserver/echo-server-cert.pem
)

add_custom_target(boost_ssl_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
)
