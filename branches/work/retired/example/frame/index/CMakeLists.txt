add_executable (example_indextest indextest.cpp)

target_link_libraries (example_indextest solid_utility solid_system ${SYS_BASIC_LIBS})

add_executable (example_rotateprint rotateprint.cpp)

target_link_libraries (example_rotateprint solid_system ${SYS_BASIC_LIBS})
