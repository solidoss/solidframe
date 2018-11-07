#add_executable (example_ipc_echo_stress ipcechostress.cpp)

# target_link_libraries (example_ipc_echo_stress
#   solid_frame_ipc
#   solid_frame_aio
#   solid_frame_aio_openssl
#   solid_frame_core
#   solid_serialization_v1
#   solid_utility
#   solid_system
#   ${OPENSSL_LIBS}
#   ${BOOST_PROGRAM_OPTIONS_LIB}
#   ${BOOST_SYSTEM_LIB}
#   ${OPENSSL_LIBS}
#   ${SYSTEM_BASIC_LIBRARIES}
#   ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
# )

#if(SOLID_ON_WINDOWS)
#    set(SUFFIX "${CMAKE_BUILD_TYPE}")
#endif()

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-ca-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-key.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-key.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    COMMAND ${CMAKE_COMMAND} -E copy ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-cert.pem ${CMAKE_CURRENT_BINARY_DIR}/${SUFFIX}/
    DEPENDS ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-ca-cert.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-key.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-server-cert.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-key.pem ${PROJECT_SOURCE_DIR}/tutorials/certs/echo-client-cert.pem
)

add_custom_target(example_mprpc_echo_copy_certs
    DEPENDS
    ${CMAKE_CURRENT_BINARY_DIR}/echo-ca-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-server-cert.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-key.pem
    ${CMAKE_CURRENT_BINARY_DIR}/echo-client-cert.pem
)

if(${Boost_FOUND} AND OPENSSL_FOUND)
    add_executable (example_mprpc_echo example_mprpc_echo.cpp)

    add_dependencies(example_mprpc_echo example_mprpc_echo_copy_certs build-boost build-openssl)

    target_link_libraries (example_mprpc_echo
        solid_frame_mprpc
        solid_frame_aio
        solid_frame_aio_openssl
        solid_frame
        solid_serialization_v2
        solid_utility
        solid_system
        ${OPENSSL_LIBRARIES}
        ${Boost_PROGRAM_OPTIONS_LIBRARY}
        ${Boost_SYSTEM_LIBRARY}
        ${SYSTEM_BASIC_LIBRARIES}
        ${SYSTEM_DYNAMIC_LOAD_LIBRARY}
    )
endif()


