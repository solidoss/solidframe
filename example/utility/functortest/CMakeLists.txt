add_executable (functortest functortest.cpp)

target_link_libraries (functortest utility system boost_filesystem ${SYS_BASIC_LIBS})
