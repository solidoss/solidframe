add_executable (example_dynamictest dynamictest.cpp)

target_link_libraries (example_dynamictest utility system ${SYS_BASIC_LIBS})
