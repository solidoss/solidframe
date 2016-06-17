if(SOLID_USE_CPP11)
	add_executable(example_sockaddrhash sockaddrhash.cpp)
	target_link_libraries (example_sockaddrhash solid_system ${SYS_BASIC_LIBS})
endif(SOLID_USE_CPP11)
