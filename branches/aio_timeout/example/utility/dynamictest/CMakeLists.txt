add_executable (dynamictest dynamictest.cpp)

target_link_libraries (dynamictest utility system boost_filesystem ${SYS_BASIC_LIBS})
