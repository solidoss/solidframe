add_executable (simple simple.cpp)
add_executable (fork fork.cpp)
#add_executable (simple_new simple_new.cpp)

target_link_libraries (simple algorithm_serialization utility system ${SYS_BASIC_LIBS})
target_link_libraries (fork algorithm_serialization utility system ${SYS_BASIC_LIBS})
#target_link_libraries (simple_new serialization utility system ${SYS_BASIC_LIBS})
