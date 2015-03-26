
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

