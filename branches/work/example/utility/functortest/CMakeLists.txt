add_executable (example_functortest functortest.cpp)
add_executable (example_functortestb functortestb.cpp)
add_executable (example_holdertest holdertest.cpp)

target_link_libraries (example_functortest utility system ${SYS_BASIC_LIBS})
target_link_libraries (example_functortestb utility system ${SYS_BASIC_LIBS})
target_link_libraries (example_holdertest utility system ${SYS_BASIC_LIBS})
