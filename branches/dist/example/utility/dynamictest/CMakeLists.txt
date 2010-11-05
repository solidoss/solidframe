add_executable (dynamictest dynamictest.cpp)

target_link_libraries (dynamictest utility system ${SYS_BASIC_LIBS})
