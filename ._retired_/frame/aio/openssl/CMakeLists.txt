
#FILE(GLOB Sources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
#FILE(GLOB Headers RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.hpp)


set(Sources
	src/opensslsocket.cpp
)

set(Headers
	opensslsocket.hpp
)

set(Inlines
)

add_library (solid_frame_aio_openssl
	${Sources} ${Headers} ${Inlines}
)

