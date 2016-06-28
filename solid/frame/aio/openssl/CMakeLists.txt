
set(Sources
	src/aiosecuresocket.cpp
)

set(Headers
	aiosecuresocket.hpp
	aiosecurecontext.hpp
)

set(Inlines
)

add_library (solid_frame_aio_openssl
	${Sources} ${Headers} ${Inlines}
)

install (FILES ${Headers} ${Inlines} DESTINATION include/solid/frame/aio/openssl)
install (TARGETS solid_frame_aio_openssl DESTINATION lib)
