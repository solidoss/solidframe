add_executable (example_indextest indextest.cpp)

target_link_libraries (example_indextest utility system ${SYS_BASIC_LIBS})

add_executable (example_rotateprint rotateprint.cpp)

target_link_libraries (example_rotateprint system ${SYS_BASIC_LIBS})
