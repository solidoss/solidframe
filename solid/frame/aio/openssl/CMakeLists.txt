if(OPENSSL_FOUND)
	set(Sources
		src/aiosecuresocket.cpp
	)

	set(Headers
		aiosecuresocket.hpp
		aiosecurecontext.hpp
	)

	set(Inlines
	)

	add_library (solid_frame_aio_openssl STATIC
		${Sources} ${Headers} ${Inlines}
	)
	
	target_link_libraries(solid_frame_aio_openssl solid_frame_aio ${OPENSSL_LIBRARIES})
	
	install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio/openssl)
	install (TARGETS solid_frame_aio_openssl DESTINATION lib EXPORT SolidFrameConfig)
endif()
