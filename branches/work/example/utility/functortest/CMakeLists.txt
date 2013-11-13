add_executable (example_functortest functortest.cpp)
add_executable (example_holdertest holdertest.cpp)
add_executable (example_functorqueue functorqueue.cpp)
add_executable (example_allocatetest allocatetest.cpp)

target_link_libraries (example_functortest solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_functorqueue solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_holdertest solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_allocatetest solid_utility solid_system ${SYS_BASIC_LIBS})
