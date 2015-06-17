add_executable (example_functortest functortest.cpp)
add_executable (example_functorspeed functorspeed.cpp)
add_executable (example_holdertest holdertest.cpp)
add_executable (study_allocatetest allocatetest.cpp)

target_link_libraries (example_functortest solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_functorspeed solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (example_holdertest solid_utility solid_system ${SYS_BASIC_LIBS})
target_link_libraries (study_allocatetest solid_utility solid_system ${SYS_BASIC_LIBS})
