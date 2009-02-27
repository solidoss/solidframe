
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

add_library (foundation_aio_openssl
	${Sources} ${Headers} ${Inlines}
)

