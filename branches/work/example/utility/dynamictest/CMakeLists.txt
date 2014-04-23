add_executable (example_dynamictest dynamictest.cpp)

target_link_libraries (example_dynamictest solid_utility solid_system ${SYS_BASIC_LIBS})
