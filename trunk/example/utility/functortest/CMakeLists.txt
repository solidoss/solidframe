add_executable (functortest functortest.cpp)
add_executable (functortestb functortestb.cpp)

target_link_libraries (functortest utility system boost_filesystem ${SYS_BASIC_LIBS})
target_link_libraries (functortestb utility system boost_filesystem ${SYS_BASIC_LIBS})
