if(HAVE_CPP11)
	add_executable(example_sockaddrhash sockaddrhash.cpp)
	target_link_libraries (example_sockaddrhash system ${SYS_BASIC_LIBS})
endif(HAVE_CPP11)
