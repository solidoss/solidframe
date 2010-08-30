add_executable (functortest functortest.cpp)
add_executable (functortestb functortestb.cpp)
add_executable (holdertest holdertest.cpp)

target_link_libraries (functortest utility system ${SYS_BASIC_LIBS})
target_link_libraries (functortestb utility system ${SYS_BASIC_LIBS})
target_link_libraries (holdertest utility system ${SYS_BASIC_LIBS})
