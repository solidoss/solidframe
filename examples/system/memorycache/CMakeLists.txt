add_executable (example_memorycache memorycachetest.cpp)
target_link_libraries (example_memorycache solid_system ${SYS_BASIC_LIBS})

add_executable (example_functionmemorycache functionmemorycache.cpp)
target_link_libraries (example_functionmemorycache solid_system ${SYS_BASIC_LIBS})
