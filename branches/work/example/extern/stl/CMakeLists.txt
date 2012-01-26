if(${HAVE_CPP11})
	add_executable(sockaddrhash sockaddrhash.cpp)
	target_link_libraries (sockaddrhash system ${SYS_BASIC_LIBS})
endif(${HAVE_CPP11})
